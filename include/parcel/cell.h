#pragma once

/**
 * @file cell.h
 * @brief Core `ICell` interface, `cell_t` handle, `BaseCell` CRTP base, and `CellLike` concept.
 *
 * The `ICell` interface is the polymorphic root of every parcel cell.
 * Concrete cells inherit from `BaseCell<Derived, Storage>` to get a ready
 * implementation of `to_json` / `clone` / kind plumbing on top of a
 * user-chosen storage type. The `CellLike` concept names the static
 * interface (`kind_id`, `from_json`, `descriptor`) that registry-aware code
 * relies on. See the README "Cell interface" section.
 */

#include <parcel/common.h>
#include <parcel/error.h>

#include <compare>
#include <concepts>
#include <cstddef>
#include <functional>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <variant>

#if PARCEL_HAS_EXPECTED
#include <expected>
#endif

namespace parcel {
class ParcelRegistry;
struct ICellTypeDescriptor;

namespace detail {
template <typename T>
struct is_std_variant : std::false_type {};
template <typename... Ts>
struct is_std_variant<std::variant<Ts...>> : std::true_type {};

// Used to suppress the BaseCell::compare default for storage types whose
// `operator==` is unconstrained but instantiates per-element comparisons —
// they would emit hard errors on substitution even though the cell type
// (UnionCell, TypedListCell, TypedMapCell) overrides compare(). Names match
// nominally; specializations are added for the std container types we
// expose as cell storage.
template <typename T>
struct disables_basecell_compare : std::false_type {};
}  // namespace detail

template <typename T>
inline constexpr bool is_std_variant_v = detail::is_std_variant<std::remove_cvref_t<T>>::value;

template <typename T>
inline constexpr bool disables_basecell_compare_v =
    detail::disables_basecell_compare<std::remove_cvref_t<T>>::value;

/** @brief Shared handle to a runtime cell-type descriptor. */
using cell_type_descriptor_t = std::shared_ptr<ICellTypeDescriptor>;

struct ICell;

/** @brief Shared handle to any `ICell`-derived value — the canonical cell pointer. */
using cell_t = std::shared_ptr<ICell>;

/**
 * @brief Polymorphic root of every parcel cell.
 *
 * Wire shape: `{"k": <kind>, "v": <value>, "d": <display info>}` where the `"d"`
 * block is omitted when no display info is set. Every concrete cell exposes its
 * kind via `kind()`, can serialize through `to_json()`, render via
 * `to_string()`, and clone deeply via `clone()`.
 *
 * Display info is read-only on the public surface: callers retrieve it via
 * `overridden_display_info()` and only the immutable `with_*` builders may change it.
 *
 * @see BaseCell — CRTP base providing default implementations.
 * @see CellLike — concept naming the static side of a cell type.
 */
struct ICell {
    /** @brief JSON key for the kind id (`"k"`). */
    static constexpr std::string_view KEY_KIND = "k";
    /** @brief JSON key for the value payload (`"v"`). */
    static constexpr std::string_view KEY_VALUE = "v";
    /** @brief JSON key for the optional display info block (`"d"`). */
    static constexpr std::string_view KEY_DESCRIPTION = "d";

    virtual ~ICell() = default;

    /**
     * @brief Wire-stable kind identifier for this cell.
     * @return The same value as the static `kind_id` on the concrete type.
     */
    [[nodiscard]] virtual std::string_view kind() const = 0;

    /**
     * @brief Read-only access to this cell's optional display info.
     * @return Reference to the held optional; empty when no display info is set.
     */
    [[nodiscard]] virtual std::optional<DisplayInfo> const& overridden_display_info() const = 0;

    /**
     * @brief Serialize this cell to its canonical JSON representation.
     * @return JSON object of shape `{"k", "v", optional "d"}`.
     */
    [[nodiscard]] virtual json_t to_json() const = 0;

    /**
     * @brief Render the cell's value as a compact human-readable string.
     * @return String form — implementation-defined per cell type.
     */
    [[nodiscard]] virtual std::string to_string() const = 0;

    /**
     * @brief Render the cell as a multi-line, indented string.
     * @return Defaults to `to_string()`; concrete cells may override.
     */
    [[nodiscard]] virtual std::string to_formatted_string() const {
        return to_string();
    }

    /**
     * @brief Deep-copy this cell.
     * @return A new `cell_t` whose state is independent of the original.
     */
    [[nodiscard]] virtual cell_t clone() const = 0;

    /**
     * @brief Three-way compare against another cell.
     *
     * Compares by kind id first (lexicographic on the string view); within
     * the same kind, compares the held value. Display info is intentionally
     * ignored — two cells with the same kind/value but different `DisplayInfo`
     * are equivalent under this relation.
     *
     * @param other Cell to compare with.
     * @return `std::partial_ordering` — `unordered` whenever the storage
     *         type only supports equality (or contains incomparable values
     *         such as floating-point NaN).
     */
    [[nodiscard]] virtual std::partial_ordering compare(ICell const& other) const = 0;

    /**
     * @brief Equality-consistent hash that mirrors `compare`'s display-info-insensitivity.
     *
     * Default implementation hashes the kind id and the JSON dump of `"v"`,
     * which works for every cell whose `to_json()` payload omits nested
     * `DisplayInfo`. Heterogeneous container cells (`ListCell`, `MapCell`,
     * `HashMapCell`) override this to recurse through `hash_value()` on
     * their children — that is what keeps the hash display-info-insensitive when
     * children carry display info of their own.
     */
    [[nodiscard]] virtual std::size_t hash_value() const noexcept {
        const std::size_t hk = std::hash<std::string_view>{}(this->kind());
        try {
            const auto j = this->to_json();
            const auto it = j.find(KEY_VALUE);
            const std::size_t hv =
                (it != j.end()) ? std::hash<std::string>{}(it->dump()) : std::size_t{0};
            return hk ^ (hv + 0x9e3779b97f4a7c15ULL + (hk << 6) + (hk >> 2));
        } catch (...) {
            return hk;
        }
    }

    /** @brief Equality through `compare`; ignores display info. */
    friend bool operator==(ICell const& a, ICell const& b) {
        return a.compare(b) == std::partial_ordering::equivalent;
    }

    /** @brief Three-way comparison through `compare`; ignores display info. */
    friend std::partial_ordering operator<=>(ICell const& a, ICell const& b) {
        return a.compare(b);
    }

    // Immutable builders: each clones the cell, mutates the clone's display info,
    // and returns the new cell_t. The original is untouched.

    /**
     * @brief Return a deep copy with the entire display info block replaced.
     * @param m New display info to attach.
     * @return Fresh `cell_t`; this cell is unmodified.
     */
    [[nodiscard]] cell_t with_display_info(DisplayInfo m) const {
        auto c = clone();
        c->set_display_info(std::move(m));
        return c;
    }

    /**
     * @brief Return a deep copy with `name` set to @p v.
     * @param v New name.
     * @return Fresh `cell_t`; this cell is unmodified.
     */
    [[nodiscard]] cell_t with_name(std::string v) const {
        auto m = overridden_display_info().value_or(DisplayInfo{});
        m.name = std::move(v);
        return with_display_info(std::move(m));
    }

    /**
     * @brief Return a deep copy with `description` set to @p v.
     * @param v New description.
     * @return Fresh `cell_t`; this cell is unmodified.
     */
    [[nodiscard]] cell_t with_description(std::string v) const {
        auto m = overridden_display_info().value_or(DisplayInfo{});
        m.description = std::move(v);
        return with_display_info(std::move(m));
    }

    /**
     * @brief Return a deep copy with `icon` set to the typed @p icon.
     * @param icon New icon.
     * @return Fresh `cell_t`; this cell is unmodified.
     */
    [[nodiscard]] cell_t with_icon(comms::Icon icon) const {
        auto m = overridden_display_info().value_or(DisplayInfo{});
        m.icon = icon;
        return with_display_info(std::move(m));
    }

    /**
     * @brief Return a deep copy with `icon` parsed from an Iconify
     *        `set:name` string (e.g. `"mdi:account"`).
     * @param v Iconify `set:name` identifier.
     * @return Fresh `cell_t`; this cell is unmodified.
     * @throws std::invalid_argument if @p v is not a valid `set:name` icon.
     */
    [[nodiscard]] cell_t with_icon(std::string const& v) const {
        return with_icon(comms::Icon::from(v));
    }

    /**
     * @brief Return a deep copy with `color` set to the typed @p color.
     * @param color New color.
     * @return Fresh `cell_t`; this cell is unmodified.
     */
    [[nodiscard]] cell_t with_color(comms::Color color) const {
        auto m = overridden_display_info().value_or(DisplayInfo{});
        m.color = color;
        return with_display_info(std::move(m));
    }

    /**
     * @brief Return a deep copy with `color` parsed from a color string
     *        (hex like `"#ffcc00"`, a CSS-functional form, or a CSS color name).
     * @param v Color string accepted by `comms::Color::parse`.
     * @return Fresh `cell_t`; this cell is unmodified.
     * @throws InvalidJsonException if @p v does not parse to a color.
     */
    [[nodiscard]] cell_t with_color(std::string const& v) const {
        const auto parsed = comms::Color::parse(v);
        if (!parsed) {
            throw InvalidJsonException("with_color: '" + v + "' is not a valid color");
        }
        return with_color(*parsed);
    }

protected:
    /**
     * @brief Replace this cell's display info in place. Used by `with_*` builders
     *        on freshly cloned receivers and by JSON deserializers; no
     *        other public path mutates display info.
     * @param m New display info value (may be empty to clear).
     */
    virtual void set_display_info(std::optional<DisplayInfo> m) = 0;

    template <typename, typename>
    friend struct BaseCell;

    template <typename>
    friend class SelfStructCell;
};

/**
 * @brief ADL hook that lets nlohmann serialize any `ICell`-derived cell.
 *
 * Enables `json_t j = some_cell;` and lets containers of `ICell` subtypes
 * (e.g. `std::vector<T>` in `TypedListCell`) be serialized without bespoke
 * per-type adl_serializers.
 *
 * @tparam T Concrete cell type derived from `ICell`.
 * @param  j Output JSON node.
 * @param  v Cell to serialize.
 */
template <typename T>
    requires std::derived_from<T, ICell>
inline void to_json(json_t& j, T const& v) {
    j = v.to_json();
}

/**
 * @brief ADL hook for serializing `std::shared_ptr<T>` of an `ICell`-derived type.
 *
 * Needed for heterogeneous containers whose storage element is `cell_t`.
 *
 * @tparam T Concrete cell type derived from `ICell`.
 * @param  j Output JSON node.
 * @param  v Shared pointer to serialize. Null becomes JSON `null`.
 */
template <typename T>
    requires std::derived_from<T, ICell>
inline void to_json(json_t& j, std::shared_ptr<T> const& v) {
    j = v ? v->to_json() : json_t();
}

/**
 * @brief Concept naming the static interface every cell type must expose.
 *
 * A `CellLike` type derives from `ICell`, declares a `kind_id` string view,
 * provides a `from_json(j, registry)` factory, and exposes a static
 * `descriptor()` returning a `cell_type_descriptor_t`.
 *
 * @tparam T Candidate cell type.
 */
template <typename T>
concept CellLike = std::derived_from<T, ICell> && requires {
    { T::kind_id } -> std::convertible_to<std::string_view>;
    {
        T::from_json(std::declval<json_t const&>(), std::declval<ParcelRegistry const&>())
    } -> std::convertible_to<cell_t>;
    { T::descriptor() } -> std::convertible_to<cell_type_descriptor_t>;
};

/**
 * @brief CRTP base providing default `to_json` / `clone` / kind plumbing on top of a storage type.
 *
 * Concrete cells inherit `BaseCell<Derived, Storage>` and supply
 * `kind_id`, `from_json`, `descriptor`, and `to_string`. The storage type
 * is held by value as `BaseCell::value` and exposed through `get()` plus
 * inherited container-style helpers in derived cells (`PrimitiveCell`,
 * `TypedListCell`, etc.).
 *
 * @tparam Derived The concrete cell type (CRTP self).
 * @tparam Storage The held value type.
 */
template <typename Derived, typename Storage>
struct BaseCell : ICell {
    using derived_t = Derived;
    using storage_t = Storage;

    /** @brief Held value of the cell. */
    Storage value{};

    BaseCell() = default;

    /**
     * @brief Construct by forwarding into the storage (`v` becomes `Storage::value`).
     *
     * Conditionally non-explicit so concrete cells (e.g. `I32Cell c = 42`)
     * can be brace-/copy-initialized from a raw storage value.
     *
     * @tparam U Any type the storage is constructible from.
     */
    template <typename U>
        requires std::constructible_from<Storage, U&&>
    explicit(false) BaseCell(U&& v) : value(std::forward<U>(v)) {}

    /**
     * @brief Assign to the storage from @p v.
     * @tparam U Any type assignable to the storage.
     * @param  v Replacement value.
     * @return Reference to the derived cell for chaining.
     */
    template <typename U>
        requires std::assignable_from<Storage&, U&&>
    Derived& operator=(U&& v) {
        value = std::forward<U>(v);
        return static_cast<Derived&>(*this);
    }

    /**
     * @brief Construct a `shared_ptr<Derived>` forwarding the arguments.
     * @tparam Args Any pack accepted by a `Derived` constructor.
     * @param  args Arguments forwarded to the constructor.
     * @return `std::shared_ptr<Derived>` owning the new instance.
     */
    template <typename... Args>
        requires std::constructible_from<Derived, Args&&...>
    static std::shared_ptr<Derived> of(Args&&... args) {
        return std::make_shared<Derived>(std::forward<Args>(args)...);
    }

    /**
     * @brief Construct a `unique_ptr<Derived>` forwarding the arguments.
     * @tparam Args Any pack accepted by a `Derived` constructor.
     * @param  args Arguments forwarded to the constructor.
     * @return `std::unique_ptr<Derived>` owning the new instance.
     */
    template <typename... Args>
        requires std::constructible_from<Derived, Args&&...>
    static std::unique_ptr<Derived> unique(Args&&... args) {
        return std::make_unique<Derived>(std::forward<Args>(args)...);
    }

    /** @brief Mutable access to the held storage. */
    Storage& get() {
        return value;
    }

    /** @brief Read-only access to the held storage. */
    Storage const& get() const {
        return value;
    }

    [[nodiscard]] std::string_view kind() const override {
        return Derived::kind_id;
    }

    [[nodiscard]] std::optional<DisplayInfo> const& overridden_display_info() const override {
        return display_info_;
    }

    [[nodiscard]] cell_t clone() const override {
        return std::make_shared<Derived>(static_cast<Derived const&>(*this));
    }

    /**
     * @brief Default three-way comparison: kind first, then storage value.
     *
     * If `Storage` supports `<=>`, that comparison is forwarded directly
     * (which yields `partial_ordering` for floating-point storage). If only
     * `==` is available, equal values map to `equivalent` and unequal values
     * to `unordered`. Storage types lacking either, or `std::variant`
     * storage (which `UnionCell` overrides), return `unordered`.
     *
     * @param other Cell to compare with.
     * @return Three-way comparison result; ignores display info.
     */
    [[nodiscard]] std::partial_ordering compare(ICell const& other) const override {
        if (const auto k_cmp = this->kind() <=> other.kind(); k_cmp != 0) {
            return k_cmp;
        }
        const auto* o = dynamic_cast<Derived const*>(&other);
        if (o == nullptr) {
            return std::partial_ordering::unordered;
        }
        // std::variant<T...>'s operator== is unconstrained and instantiates
        // its alternatives' ==; same for std::vector / std::map. If an
        // element type lacks ==, the body fails to compile. The cell types
        // built on those storages (UnionCell, TypedListCell, TypedMapCell,
        // ListCell, MapCell) override compare() with their own dispatch —
        // skip the body here so BaseCell stays instantiable.
        if constexpr (!is_std_variant_v<Storage> && !disables_basecell_compare_v<Storage>) {
            if constexpr (std::three_way_comparable<Storage>) {
                return this->value <=> o->value;
            } else if constexpr (std::equality_comparable<Storage>) {
                return this->value == o->value ? std::partial_ordering::equivalent
                                               : std::partial_ordering::unordered;
            }
        }
        return std::partial_ordering::unordered;
    }

    /**
     * @brief Default JSON serialization for cells with JSON-convertible storage.
     *
     * If `Storage` has a JSON adapter, emits `{"k": kind_id, "v": value,
     * optional "d": display info}`. Otherwise the call throws — the derived cell
     * must override `to_json` itself (`ListCell`, `MapCell`, etc.).
     *
     * @return JSON object representation.
     * @throws std::runtime_error if `Storage` is not JSON-convertible.
     */
    [[nodiscard]] json_t to_json() const override {
        if constexpr (requires(Storage const& s) { json_t(s); }) {
            json_t j{{KEY_KIND, kind()}, {KEY_VALUE, this->value}};
            inject_display_info(j);
            return j;
        } else {
            throw std::runtime_error(
                "BaseCell::to_json() requires Derived to override or Storage to be "
                "JSON-convertible");
        }
    }

    /**
     * @brief Copy this cell's display info (if any) into the JSON object under `"d"`.
     * @param j JSON object to mutate.
     */
    void inject_display_info(json_t& j) const {
        if (display_info_) {
            j[KEY_DESCRIPTION] = *display_info_;
        }
    }

    /**
     * @brief Read `"d"` (if present) from a JSON object and assign it onto a cell.
     * @tparam Out Any pointer-like type whose pointee derives from `ICell`.
     * @param  j   Input JSON object.
     * @param  out Pointer-like to a cell whose display info should be populated.
     */
    template <typename Out>
    static void absorb_display_info(json_t const& j, Out& out) {
        if (const auto it = j.find(KEY_DESCRIPTION); it != j.end()) {
            out->set_display_info(it->get<DisplayInfo>());
        }
    }

protected:
    /** @brief Optional display info; omitted from JSON when empty. */
    std::optional<DisplayInfo> display_info_;

    void set_display_info(std::optional<DisplayInfo> m) override {
        display_info_ = std::move(m);
    }

    /**
     * @brief Validate a wrapped `{"k","v"}` JSON object and extract its `"v"` as @p T.
     *
     * Used by primitive-style `from_json` overloads.
     *
     * @tparam T Type to deserialize the `"v"` slot into.
     * @param  j             Input JSON object.
     * @param  expected_kind Kind id this cell expects under `"k"`.
     * @return Deserialized value.
     * @throws std::runtime_error if the object shape is invalid or the
     *         kind does not match @p expected_kind.
     */
    template <typename T>
    static T cell_from_json(const json_t& j, const std::string_view expected_kind) {
        if (!j.is_object()) {
            throw InvalidJsonException("Expected JSON object for ICell",
                                       std::string(expected_kind));
        }

        const auto it_kind = j.find(KEY_KIND);
        if (it_kind == j.end() || !it_kind->is_string()) {
            throw InvalidJsonException("Missing or invalid 'k' field in ICell JSON",
                                       std::string(expected_kind));
        }

        if (const auto kind = it_kind->get<std::string_view>(); kind != expected_kind) {
            throw KindMismatchException("Unexpected kind in ICell JSON: expected '" +
                                            std::string(expected_kind) + "', got '" +
                                            std::string(kind) + "'",
                                        std::string(expected_kind));
        }

        const auto it_value = j.find(KEY_VALUE);
        if (it_value == j.end()) {
            throw InvalidJsonException("Missing 'v' field in ICell JSON",
                                       std::string(expected_kind));
        }

        return it_value->get<T>();
    }

public:
    /**
     * @brief Strict shorthand for the recurring `cell_from_json + make_shared
     *        + absorb_display_info` pattern in concrete `from_json` overloads.
     *
     * Validates that @p j is an object whose `"k"` matches `Derived::kind_id`,
     * deserializes `"v"` as `Storage`, builds a `shared_ptr<Derived>` from
     * it, and absorbs any display info block.
     *
     * Requires `Storage` to be JSON-deserializable (every primitive cell
     * already is) — concrete cells whose payload needs custom decoding (e.g.
     * `TypedListCell`, `StructCell`) keep their own `from_json` body.
     *
     * @param j Input JSON object.
     * @return Newly built cell handle.
     * @throws std::runtime_error on shape or kind mismatch.
     */
    static cell_t from_json_strict(json_t const& j) {
        auto v = cell_from_json<Storage>(j, Derived::kind_id);
        auto cell = std::make_shared<Derived>(std::move(v));
        absorb_display_info(j, cell);
        return cell;
    }
};

/**
 * @brief Type-safe `dynamic_cast` from `cell_t` to a concrete cell type.
 *
 * @tparam C Target cell type (must satisfy `CellLike`).
 * @param  c Source cell handle.
 * @return `shared_ptr<C>` aliasing the original ownership.
 * @throws std::runtime_error if @p c is null or not actually a @p C.
 */
template <typename C>
    requires std::derived_from<C, ICell>
[[nodiscard]] std::shared_ptr<C> cell_cast(cell_t const& c) {
    if (!c) {
        throw TypeException("cell_cast: null cell", std::string(C::kind_id));
    }
    auto out = std::dynamic_pointer_cast<C>(c);
    if (!out) {
        throw KindMismatchException(std::string("cell_cast: expected '") + std::string(C::kind_id) +
                                        "' but got '" + std::string(c->kind()) + "'",
                                    std::string(C::kind_id));
    }
    return out;
}

/**
 * @brief `cell_cast` that returns @p fallback on failure rather than throwing.
 *
 * @tparam C Target cell type.
 * @param  c        Source cell handle.
 * @param  fallback Returned when @p c is null or not a @p C.
 * @return Either the cast handle or @p fallback.
 */
template <typename C>
    requires std::derived_from<C, ICell>
[[nodiscard]] std::shared_ptr<C> cell_cast_or(cell_t const& c, std::shared_ptr<C> fallback) {
    if (!c) {
        return fallback;
    }
    auto out = std::dynamic_pointer_cast<C>(c);
    return out ? out : std::move(fallback);
}

#if PARCEL_HAS_EXPECTED
/**
 * @brief Non-throwing `cell_cast` returning `std::expected`.
 *
 * @tparam C Target cell type.
 * @param  c Source cell handle.
 * @return The handle on success, a `ParcelError` on null/mismatch.
 */
template <typename C>
    requires std::derived_from<C, ICell>
[[nodiscard]] std::expected<std::shared_ptr<C>, ParcelError> try_cell_cast(cell_t const& c) {
    if (!c) {
        return std::unexpected(ParcelError::make(ParcelError::Code::TypeError, "null cell"));
    }
    auto out = std::dynamic_pointer_cast<C>(c);
    if (!out) {
        return std::unexpected(ParcelError::make(ParcelError::Code::KindMismatch,
                                                 std::string("expected '") +
                                                     std::string(C::kind_id) + "' but got '" +
                                                     std::string(c->kind()) + "'",
                                                 std::string(C::kind_id)));
    }
    return out;
}
#endif

/**
 * @brief Cast and unwrap to the inner storage value in one call.
 *
 * @tparam C Target cell type.
 * @param  c Source cell handle.
 * @return Copy of `C::storage_t` held by the cast cell.
 * @throws std::runtime_error if @p c is null or not a @p C.
 */
template <typename C>
    requires std::derived_from<C, ICell>
[[nodiscard]] typename C::storage_t cell_cast_value(cell_t const& c) {
    return cell_cast<C>(c)->value;
}

/**
 * @brief `std::optional` view over a cell's inner storage.
 *
 * Mirrors `std::optional`'s "either present or not" shape — `nullopt` means
 * either @p c is null or it isn't a @p C.
 *
 * @tparam C Target cell type.
 * @param  c Source cell handle.
 * @return `optional<C::storage_t>` with the value on success.
 */
template <typename C>
    requires std::derived_from<C, ICell>
[[nodiscard]] std::optional<typename C::storage_t> as(cell_t const& c) noexcept {
    if (!c) {
        return std::nullopt;
    }
    auto* typed = dynamic_cast<C const*>(c.get());
    if (typed == nullptr) {
        return std::nullopt;
    }
    return typed->value;
}

/**
 * @brief Extract @p c's storage as @p C, falling back to @p fallback on failure.
 *
 * @tparam C Target cell type.
 * @param  c        Source cell handle.
 * @param  fallback Returned when @p c is null or not a @p C.
 * @return Storage value or @p fallback.
 */
template <typename C>
    requires std::derived_from<C, ICell>
[[nodiscard]] typename C::storage_t value_or(cell_t const& c, typename C::storage_t fallback) {
    if (!c) {
        return fallback;
    }
    auto* typed = dynamic_cast<C const*>(c.get());
    return typed ? typed->value : std::move(fallback);
}

/**
 * @brief Free `to_json` over `cell_t`; returns `null` for null handles.
 */
[[nodiscard]] inline json_t to_json(cell_t const& c) {
    return c ? c->to_json() : json_t{};
}

/**
 * @brief Free `to_string` over `cell_t`; returns `"<null>"` for null handles.
 */
[[nodiscard]] inline std::string to_string(cell_t const& c) {
    return c ? c->to_string() : std::string{"<null>"};
}

/**
 * @brief Multi-line `to_string` over `cell_t`; returns `"<null>"` for null handles.
 */
[[nodiscard]] inline std::string to_string_pretty(cell_t const& c) {
    return c ? c->to_formatted_string() : std::string{"<null>"};
}

}  // namespace parcel

namespace std {

/**
 * @brief `std::hash<ICell>` — equality-consistent with `operator==`.
 *
 * Routes to `ICell::hash_value`. Container cells recurse through their
 * children's `hash_value` so two cells that compare equal under
 * display-info-insensitive equality also hash equal, even when nested cells differ
 * in their `DisplayInfo`.
 */
template <>
struct hash<parcel::ICell> {
    [[nodiscard]] size_t operator()(parcel::ICell const& c) const noexcept {
        return c.hash_value();
    }
};

/** @brief `std::hash<cell_t>` — null hashes to 0; otherwise routes to `hash<ICell>`. */
template <>
struct hash<parcel::cell_t> {
    [[nodiscard]] size_t operator()(parcel::cell_t const& c) const noexcept {
        return c ? std::hash<parcel::ICell>{}(*c) : std::size_t{0};
    }
};

}  // namespace std

namespace parcel {

/**
 * @brief Hash a cell value compatibly with `operator==`.
 *
 * Free helper that mirrors the `std::hash<ICell>` specialization for places
 * where a function-call shape is more readable.
 */
[[nodiscard]] inline std::size_t hash_cell(ICell const& c) noexcept {
    return std::hash<ICell>{}(c);
}

[[nodiscard]] inline std::size_t hash_cell(cell_t const& c) noexcept {
    return std::hash<cell_t>{}(c);
}

}  // namespace parcel
