#pragma once

/**
 * @file registry.h
 * @brief `ParcelRegistry`, `Definition`, and `BuiltinsOptions` for polymorphic dispatch.
 *
 * `ParcelRegistry` is the runtime catalog of cell-type descriptors keyed
 * by wire kind id. Heterogeneous containers (`ListCell`, `MapCell`) and
 * any user code dispatching from a JSON `"k"` go through it. The registry
 * also walks the schema graph in `define()`, returning a root descriptor
 * plus every kind it transitively references. See the README "Registry"
 * section.
 */

#include <parcel/cell.h>
#include <parcel/descriptor.h>
#include <parcel/error.h>
#include <parcel/json.h>

#include <cstddef>
#include <functional>
#include <map>
#include <memory>
#include <ranges>
#include <stdexcept>
#include <string>
#include <string_view>
#include <typeindex>
#include <typeinfo>
#include <utility>
#include <vector>

#if PARCEL_HAS_EXPECTED
#include <expected>
#endif

namespace parcel {

/**
 * @brief Toggles for the built-in cell registrations performed by `register_builtins`.
 *
 * Defaults register everything: primitives, the heterogeneous `ListCell` /
 * `MapCell`, a `TypedListCell<P>` / `TypedMapCell<P>` for every primitive
 * `P`, the std-adapter cells (`SystemTimePointCell`, `UnixMillisCell`,
 * `DurationMsCell`, `YmdCell`, `PathCell`), the commons-adapter cells, and —
 * when the ULID dependency is enabled (`PARCEL_HAS_ULID`) — `UlidCell`.
 *
 * @see register_builtins
 */
struct BuiltinsOptions {
    /** @brief Register every `PrimitiveCell<T>`. */
    bool primitives = true;
    /** @brief Register `ListCell`, `MapCell`, and `HashMapCell` (heterogeneous). */
    bool collections = true;
    /**
     * @brief Register `TypedListCell<P>`, `TypedMapCell<P>`, and `TypedHashMapCell<P>`
     *        for every primitive `P`.
     */
    bool typed_collections = true;
    /**
     * @brief Register the std-adapter cells: `SystemTimePointCell`,
     *        `UnixMillisCell`, `DurationMsCell`, `YmdCell`, `PathCell`.
     */
    bool std = true;
    /**
     * @brief Register the commons cells: `ColorCell`, `IconCell`,
     *        `DisplayInfoCell`, `FlagCell`, `FlagSetCell`, `SemVerCell`,
     *        `VersionConstraintCell`, `OriginCell`.
     */
    bool commons = true;
    /**
     * @brief Register `UlidCell` (only takes effect when the ULID dependency
     *        is enabled — `PARCEL_HAS_ULID`; an inert flag otherwise).
     */
    bool ulid = true;
};

/**
 * @brief Closed-over schema for a single root kind plus everything it references.
 *
 * Returned by `ParcelRegistry::define`. Useful for emitting a self-contained
 * schema document for a particular cell type.
 */
struct Definition {
    /** @brief Descriptor for the requested root kind. */
    cell_type_descriptor_t root;
    /** @brief Every kind transitively referenced by @ref root, keyed by kind id. */
    std::map<std::string, cell_type_descriptor_t, std::less<>> referenced;

    /**
     * @brief Serialize to a JSON object with `root` and `referenced` blocks.
     * @return JSON object describing the whole schema slice.
     */
    [[nodiscard]] json_t to_json() const {
        json_t refs = json_t::object();
        for (auto const& [k, d] : referenced) {
            refs[k] = d->to_json();
        }
        return json_t{
            {"root", root->to_json()},
            {"referenced", std::move(refs)},
        };
    }
};

/**
 * @brief Runtime catalog of cell-type descriptors, keyed by wire kind id.
 *
 * Heterogeneous containers and any code deserializing from an arbitrary
 * `"k"` go through the registry. The default-constructed registry calls
 * `register_builtins` so the standard kinds are immediately available.
 *
 * @see BuiltinsOptions — toggles for what gets pre-registered.
 */
class ParcelRegistry {
    std::map<std::string, cell_type_descriptor_t, std::less<>> by_kind_;

public:
    /**
     * @brief Construct, optionally tuning which builtins are pre-registered.
     * @param opts Toggles forwarded to `register_builtins`.
     */
    explicit ParcelRegistry(BuiltinsOptions opts = {});

    /**
     * @brief Register or replace a descriptor by its kind id.
     * @param d Descriptor to register; must be non-null.
     * @throws std::runtime_error if @p d is null.
     */
    void register_kind(cell_type_descriptor_t d);

    /**
     * @brief Variadic shorthand: register many descriptors in one call.
     *
     * @code{.cpp}
     * registry.register_kinds(MyCellA::descriptor(), MyCellB::descriptor());
     * @endcode
     *
     * @tparam Ds  Pack of descriptor types convertible to `cell_type_descriptor_t`.
     * @param  ds  Descriptors to register.
     * @return `*this` for chaining.
     */
    template <typename... Ds>
    ParcelRegistry& register_kinds(Ds&&... ds) {
        (register_kind(std::forward<Ds>(ds)), ...);
        return *this;
    }

    /**
     * @brief Variadic shorthand: register one descriptor per cell type.
     *
     * @code{.cpp}
     * registry.register_cells<PersonCell, AddressCell>();
     * @endcode
     *
     * @tparam Cs  Pack of `CellLike` types whose `descriptor()` is registered.
     * @return `*this` for chaining.
     */
    template <typename... Cs>
    ParcelRegistry& register_cells() {
        (register_kind(Cs::descriptor()), ...);
        return *this;
    }

    /**
     * @brief Look up a descriptor by kind id.
     * @param kind Wire kind id.
     * @return Descriptor handle, or `nullptr` if not registered.
     */
    [[nodiscard]] cell_type_descriptor_t find(std::string_view kind) const;

    /**
     * @brief Deserialize any registered cell from JSON, dispatching by `"k"`.
     * @param j Input JSON object — must contain a `"k"` matching a registered kind.
     * @return Newly built cell handle.
     * @throws std::runtime_error on shape mismatch or unknown kind.
     */
    [[nodiscard]] cell_t cell_from_json(json_t const& j) const;

#if PARCEL_HAS_EXPECTED
    /**
     * @brief Non-throwing variant of `cell_from_json` returning `std::expected`.
     *
     * Same dispatch rules as the throwing form, but a malformed input yields
     * a `ParcelError` instead of throwing. The underlying cell deserializer
     * may still throw; those exceptions are caught and translated into
     * `ParcelError::Code::TypeError`. The function is not `noexcept` because
     * `ParcelError::make` allocates std::strings — a `bad_alloc` from the
     * allocator path may still escape.
     *
     * @param j Input JSON.
     * @return Either the built cell or a structured error.
     */
    [[nodiscard]] std::expected<cell_t, ParcelError> try_cell_from_json(json_t const& j) const;

    /**
     * @brief Parse a JSON document from a string and dispatch via the registry.
     *
     * Useful for parsing a wire-shape payload directly without a separate
     * `nlohmann::json::parse` step.
     */
    [[nodiscard]] std::expected<cell_t, ParcelError> try_cell_from_string(std::string_view s) const;
#endif

    /** @brief Every registered descriptor. */
    [[nodiscard]] std::vector<cell_type_descriptor_t> all() const;

    /** @brief Every registered kind id. */
    [[nodiscard]] std::vector<std::string_view> kinds() const;

    /** @brief Number of registered kinds. */
    [[nodiscard]] std::size_t count() const noexcept;

    /**
     * @brief Whether @p kind is registered.
     * @param kind Wire kind id.
     */
    [[nodiscard]] bool contains(std::string_view kind) const;

    /**
     * @brief Every descriptor whose `category()` matches @p c.
     * @param c Category filter.
     */
    [[nodiscard]] std::vector<cell_type_descriptor_t>
    find_by_category(descriptor::CellCategory c) const;

    /**
     * @brief Every descriptor whose `storage_type()` matches @p ti.
     * @param ti Storage `std::type_index` filter.
     */
    [[nodiscard]] std::vector<cell_type_descriptor_t> find_by_storage(std::type_index ti) const;

    /**
     * @brief Every descriptor whose storage type is @p T.
     * @tparam T Storage type to match.
     */
    template <typename T>
    [[nodiscard]] std::vector<cell_type_descriptor_t> find_by_storage() const {
        return find_by_storage(std::type_index(typeid(T)));
    }

    /**
     * @brief Build a `Definition` rooted at @p kind plus everything it references.
     * @param kind Wire kind id of the root.
     * @return `Definition` containing the root descriptor and every transitively referenced kind.
     * @throws std::runtime_error if @p kind is not registered, or if a
     *         referenced kind is missing from the registry.
     */
    [[nodiscard]] Definition define(std::string_view kind) const;
};

inline void ParcelRegistry::register_kind(cell_type_descriptor_t d) {
    if (d == nullptr) {
        throw std::runtime_error("ParcelRegistry::register_kind: null descriptor");
    }
    by_kind_.insert_or_assign(std::string(d->kind()), std::move(d));
}

inline cell_type_descriptor_t ParcelRegistry::find(const std::string_view kind) const {
    if (const auto it = by_kind_.find(kind); it != by_kind_.end()) {
        return it->second;
    }
    return nullptr;
}

inline cell_t ParcelRegistry::cell_from_json(json_t const& j) const {
    if (!j.is_object()) {
        throw InvalidJsonException("ParcelRegistry::cell_from_json: expected JSON object");
    }
    const auto it_k = j.find(ICell::KEY_KIND);
    if (it_k == j.end() || !it_k->is_string()) {
        throw InvalidJsonException("ParcelRegistry::cell_from_json: missing/invalid 'k'");
    }
    const auto kind = it_k->get<std::string_view>();
    const auto desc = find(kind);
    if (desc == nullptr) {
        throw UnknownKindException("ParcelRegistry::cell_from_json: unknown kind '" +
                                       std::string(kind) + "'",
                                   std::string(kind));
    }
    return desc->cell_from_json(j, *this);
}

#if PARCEL_HAS_EXPECTED
inline std::expected<cell_t, ParcelError>
ParcelRegistry::try_cell_from_json(json_t const& j) const {
    if (!j.is_object()) {
        return std::unexpected(
            ParcelError::make(ParcelError::Code::InvalidJson, "expected JSON object"));
    }
    const auto it_k = j.find(ICell::KEY_KIND);
    if (it_k == j.end() || !it_k->is_string()) {
        return std::unexpected(
            ParcelError::make(ParcelError::Code::InvalidJson, "missing or non-string 'k'"));
    }
    const auto kind = it_k->get<std::string_view>();
    const auto desc = find(kind);
    if (desc == nullptr) {
        return std::unexpected(
            ParcelError::make(ParcelError::Code::UnknownKind, "unknown kind", std::string(kind)));
    }
    try {
        return desc->cell_from_json(j, *this);
    } catch (ParcelException const& e) {
        // Preserve the structured code / kind / field carried by the
        // exception. If the throw site didn't tag a kind, fall back to the
        // dispatch kind we already know.
        auto err = e.to_error();
        if (err.kind.empty()) {
            err.kind = std::string(kind);
        }
        return std::unexpected(std::move(err));
    } catch (std::exception const& e) {
        return std::unexpected(
            ParcelError::make(ParcelError::Code::TypeError, e.what(), std::string(kind)));
    } catch (...) {
        return std::unexpected(ParcelError::make(
            ParcelError::Code::TypeError, "unknown deserialization error", std::string(kind)));
    }
}

inline std::expected<cell_t, ParcelError>
ParcelRegistry::try_cell_from_string(std::string_view s) const {
    json_t j;
    try {
        j = json_t::parse(s);
    } catch (std::exception const& e) {
        return std::unexpected(ParcelError::make(ParcelError::Code::InvalidJson, e.what()));
    } catch (...) {
        return std::unexpected(
            ParcelError::make(ParcelError::Code::InvalidJson, "JSON parse failed"));
    }
    return try_cell_from_json(j);
}
#endif

inline std::vector<cell_type_descriptor_t> ParcelRegistry::all() const {
    std::vector<cell_type_descriptor_t> out;
    out.reserve(by_kind_.size());
    for (const auto& d : by_kind_ | std::views::values) {
        out.push_back(d);
    }
    return out;
}

inline std::vector<std::string_view> ParcelRegistry::kinds() const {
    std::vector<std::string_view> out;
    out.reserve(by_kind_.size());
    for (const auto& d : by_kind_ | std::views::values) {
        out.push_back(d->kind());
    }
    return out;
}

inline std::size_t ParcelRegistry::count() const noexcept {
    return by_kind_.size();
}

inline bool ParcelRegistry::contains(const std::string_view kind) const {
    return by_kind_.contains(kind);
}

inline std::vector<cell_type_descriptor_t>
ParcelRegistry::find_by_category(const descriptor::CellCategory c) const {
    std::vector<cell_type_descriptor_t> out;
    for (const auto& d : by_kind_ | std::views::values) {
        if (d->category() == c) {
            out.push_back(d);
        }
    }
    return out;
}

inline std::vector<cell_type_descriptor_t>
ParcelRegistry::find_by_storage(const std::type_index ti) const {
    std::vector<cell_type_descriptor_t> out;
    for (const auto& d : by_kind_ | std::views::values) {
        if (d->storage_type() == ti) {
            out.push_back(d);
        }
    }
    return out;
}

inline Definition ParcelRegistry::define(const std::string_view kind) const {
    const auto root = find(kind);
    if (root == nullptr) {
        throw UnknownKindException(
            "ParcelRegistry::define: unknown kind '" + std::string(kind) + "'", std::string(kind));
    }
    Definition out{root, {}};
    std::vector<std::string_view> queue;

    auto enqueue_subs = [&](cell_type_descriptor_t const& d) {
        if (auto const* st = dynamic_cast<ISubTypes const*>(d.get()); st != nullptr) {
            for (auto sk : st->sub_kinds()) {
                queue.push_back(sk);
            }
        }
    };

    enqueue_subs(root);
    while (!queue.empty()) {
        auto k = queue.back();
        queue.pop_back();
        if (k == root->kind()) {
            continue;
        }
        if (out.referenced.contains(k)) {
            continue;
        }
        auto d = find(k);
        if (d == nullptr) {
            throw UnknownKindException(
                "ParcelRegistry::define: kind '" + std::string(root->kind()) +
                    "' references unregistered kind '" + std::string(k) + "'",
                std::string(k));
        }
        out.referenced.emplace(std::string(k), d);
        enqueue_subs(d);
    }
    return out;
}

}  // namespace parcel
