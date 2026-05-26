#pragma once

/**
 * @file union.h
 * @brief `UnionCell<Ts...>` closed-set polymorphic cell and its descriptor.
 *
 * A `UnionCell<Ts...>` holds exactly one value drawn from a fixed set of
 * cell alternatives. Storage is `std::variant<typename Ts::storage_t...>`.
 * The wire kind id encodes every alternative kind in template order so
 * each instantiation registers under its own kind. See the README "Unions"
 * section.
 */

#include <parcel/cell.h>
#include <parcel/descriptor.h>

#include <array>
#include <cstddef>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <utility>
#include <variant>

namespace parcel {
class ParcelRegistry;

template <CellLike... Ts>
class UnionCell;

/**
 * @brief Descriptor for `UnionCell<Ts...>`.
 *
 * Adds an `"alternatives"` array (kind id per alternative) to the JSON
 * descriptor and exposes the alternative kinds through `ISubTypes`.
 *
 * @tparam Ts Cell alternative types.
 */
template <CellLike... Ts>
class UnionCellTypeDescriptor final : public BaseCellTypeDescriptor<UnionCell<Ts...>>,
                                      public ISubTypes {
public:
    using cell_type = UnionCell<Ts...>;
    using base_t = BaseCellTypeDescriptor<UnionCell<Ts...>>;

    /**
     * @brief Construct with the given display info.
     * @param info Display info.
     */
    explicit UnionCellTypeDescriptor(DisplayInfo info) : base_t(std::move(info)) {}

    /** @brief Compile-time array of every alternative's kind id. */
    [[nodiscard]] static constexpr std::array<std::string_view, sizeof...(Ts)> alternatives() {
        return {Ts::kind_id...};
    }

    [[nodiscard]] descriptor::CellCategory category() const override {
        return descriptor::CellCategory::Union;
    }

    [[nodiscard]] std::vector<std::string_view> sub_kinds() const override {
        return {Ts::kind_id...};
    }

    [[nodiscard]] cell_t cell_from_json(json_t const& j, ParcelRegistry const& reg) const override {
        return UnionCell<Ts...>::from_json(j, reg);
    }

    [[nodiscard]] json_t to_json() const override {
        auto j = base_t::base_to_json();
        json_t arr = json_t::array();
        ((arr.push_back(Ts::kind_id)), ...);
        j["alternatives"] = std::move(arr);
        return j;
    }
};

/**
 * @brief Closed-set polymorphic cell — exactly one of @p Ts at runtime.
 *
 * Wire shape (example for `UnionCell<I32Cell, StringCell>`):
 * @code{.json}
 * {"k": "u:i32,string", "v": {"k": "i32", "v": 42}}
 * @endcode
 * The active alternative is recovered from the inner `v.k`. Storage is
 * `std::variant<typename Ts::storage_t...>` so values are raw, not
 * wrapped cells. Each unique parameter pack registers under its own kind:
 * @code{.cpp}
 * UnionCell<I32Cell, StringCell>::kind_id            == "u:i32,string";
 * UnionCell<I32Cell, TypedListCell<I32Cell>>::kind_id == "u:i32,l:i32";
 * @endcode
 *
 * @tparam Ts One or more `CellLike` alternative types (at least one).
 * @see UnionCellTypeDescriptor
 */
template <CellLike... Ts>
class UnionCell : public BaseCell<UnionCell<Ts...>, std::variant<typename Ts::storage_t...>> {
    using base_t = BaseCell<UnionCell<Ts...>, std::variant<typename Ts::storage_t...>>;

    static_assert(sizeof...(Ts) > 0, "UnionCell requires at least one alternative");

public:
    using base_t::base_t;
    using base_t::operator=;

    /** @brief Storage variant — one slot per alternative. */
    using variant_storage_t = std::variant<typename Ts::storage_t...>;

    /** @brief Wrapper type at the @p I -th alternative slot. */
    template <std::size_t I>
    using nth_wrapper_t = std::tuple_element_t<I, std::tuple<Ts...>>;

    /** @brief Number of alternatives. */
    static constexpr std::size_t alternative_count = sizeof...(Ts);

    /** @brief Kind id of each alternative in template order. */
    static constexpr std::array<std::string_view, sizeof...(Ts)> alternatives{Ts::kind_id...};

    // kind_id encodes every alternative kind in template order so that each
    // distinct UnionCell<Ts...> instantiation is its own registry kind:
    //   UnionCell<I32Cell, StringCell>::kind_id           == "u:i32,string"
    //   UnionCell<I32Cell, TypedListCell<I32Cell>>::kind_id == "u:i32,l:i32"
private:
    static constexpr auto kind_id_buf = [] {
        constexpr std::array<std::string_view, sizeof...(Ts)> alts{Ts::kind_id...};
        constexpr std::size_t total = [&] {
            std::size_t n = 2 + (sizeof...(Ts) - 1);
            for (auto sv : alts) {
                n += sv.size();
            }
            return n;
        }();
        std::array<char, total> buf{};
        std::size_t pos = 0;
        buf[pos++] = 'u';
        buf[pos++] = ':';
        for (std::size_t i = 0; i < alts.size(); ++i) {
            if (i > 0) {
                buf[pos++] = ',';
            }
            for (char c : alts[i]) {
                buf[pos++] = c;
            }
        }
        return buf;
    }();

public:
    /**
     * @brief Wire kind id of this union (`"u:" + alternatives joined by ","`).
     *
     * @code{.cpp}
     * UnionCell<I32Cell, StringCell>::kind_id == "u:i32,string";
     * @endcode
     */
    static constexpr std::string_view kind_id{kind_id_buf.data(), kind_id_buf.size()};

    // ---- variant-style accessors -----------------------------------------

    /** @brief Index of the currently active alternative. */
    [[nodiscard]] std::size_t index() const noexcept {
        return this->value.index();
    }

    /**
     * @brief Access the value at alternative @p I (mutable).
     * @tparam I Alternative index.
     * @return Reference to the held value at slot @p I.
     */
    template <std::size_t I>
    [[nodiscard]] auto& get() {
        return std::get<I>(this->value);
    }

    /**
     * @brief Access the value at alternative @p I (const).
     * @tparam I Alternative index.
     * @return Const reference to the held value at slot @p I.
     */
    template <std::size_t I>
    [[nodiscard]] auto const& get() const {
        return std::get<I>(this->value);
    }

    /**
     * @brief Access the value held in storage type @p S (mutable).
     * @tparam S One of the storage types in the variant.
     * @return Reference to the held @p S value.
     */
    template <typename S>
    [[nodiscard]] S& get() {
        return std::get<S>(this->value);
    }

    /**
     * @brief Access the value held in storage type @p S (const).
     * @tparam S One of the storage types in the variant.
     * @return Const reference to the held @p S value.
     */
    template <typename S>
    [[nodiscard]] S const& get() const {
        return std::get<S>(this->value);
    }

    /** @brief Kind id of the currently active alternative. */
    [[nodiscard]] std::string_view active_kind() const {
        return alternatives[this->value.index()];
    }

    [[nodiscard]] std::string to_string() const override {
        return visit_indexed([]<std::size_t I>(std::integral_constant<std::size_t, I>,
                                               auto const& storage) -> std::string {
            return nth_wrapper_t<I>(storage).to_string();
        });
    }

    [[nodiscard]] json_t to_json() const override {
        json_t j = visit_indexed([]<std::size_t I>(std::integral_constant<std::size_t, I>,
                                                   auto const& storage) -> json_t {
            using WrapperT = nth_wrapper_t<I>;
            WrapperT wrapper(storage);
            return json_t{
                {ICell::KEY_KIND, kind_id},
                {ICell::KEY_VALUE, wrapper.to_json()},
            };
        });
        this->inject_display_info(j);
        return j;
    }

    [[nodiscard]] cell_t clone() const override {
        auto out = std::make_shared<UnionCell<Ts...>>();
        visit_indexed(
            [&out]<std::size_t I>(std::integral_constant<std::size_t, I>, auto const& storage) {
                using WrapperT = nth_wrapper_t<I>;
                const cell_t cloned = WrapperT(storage).clone();
                auto* typed = dynamic_cast<WrapperT*>(cloned.get());
                if (typed == nullptr) {
                    throw std::runtime_error("UnionCell::clone: alternative type mismatch");
                }
                out->value.template emplace<I>(std::move(typed->value));
            });
        out->display_info_ = this->display_info_;
        return out;
    }

    /**
     * @brief Three-way comparison: kind, then active alternative index, then
     *        the active alternative's wrapper-level `compare`.
     *
     * Routes through the alternative's `Wrapper::compare` instead of the
     * underlying `std::variant::operator<=>`, so storage types that lack
     * three-way comparison still work as long as their cell type does.
     */
    [[nodiscard]] std::partial_ordering compare(ICell const& other) const override {
        if (const auto k_cmp = this->kind() <=> other.kind(); k_cmp != 0) {
            return k_cmp;
        }
        const auto* o = dynamic_cast<UnionCell const*>(&other);
        if (o == nullptr) {
            return std::partial_ordering::unordered;
        }
        if (const auto idx_cmp = this->value.index() <=> o->value.index(); idx_cmp != 0) {
            return idx_cmp;
        }
        return compare_active(*o, std::index_sequence_for<Ts...>{});
    }

    /**
     * @brief Deserialize a `UnionCell<Ts...>` from JSON.
     *
     * The active alternative is recovered from the inner `v.k`; that kind
     * must equal one of `Ts::kind_id`.
     *
     * @param j   Input JSON object — must match `{"k": kind_id, "v": {…}}`.
     * @param reg Registry passed through to the active alternative's `from_json`.
     * @return Newly built cell handle.
     * @throws std::runtime_error on shape or kind mismatch, or if the
     *         inner kind does not match any alternative.
     */
    static cell_t from_json(json_t const& j, ParcelRegistry const& reg) {
        if (!j.is_object()) {
            throw InvalidJsonException("Expected JSON object for UnionCell", std::string(kind_id));
        }
        const auto it_k = j.find(ICell::KEY_KIND);
        if (it_k == j.end() || !it_k->is_string()) {
            throw InvalidJsonException("UnionCell: missing/invalid 'k' (expected '" +
                                           std::string(kind_id) + "')",
                                       std::string(kind_id));
        }
        if (it_k->get<std::string_view>() != kind_id) {
            throw KindMismatchException("UnionCell: missing/invalid 'k' (expected '" +
                                            std::string(kind_id) + "')",
                                        std::string(kind_id));
        }
        const auto it_v = j.find(ICell::KEY_VALUE);
        if (it_v == j.end() || !it_v->is_object()) {
            throw InvalidJsonException("UnionCell: missing/invalid 'v' (expected object)",
                                       std::string(kind_id));
        }
        const auto it_inner_k = it_v->find(ICell::KEY_KIND);
        if (it_inner_k == it_v->end() || !it_inner_k->is_string()) {
            throw InvalidJsonException("UnionCell: inner 'v' missing/invalid 'k'",
                                       std::string(kind_id));
        }
        const auto active = it_inner_k->get<std::string_view>();
        cell_t out = dispatch_from_json(active, *it_v, reg, std::index_sequence_for<Ts...>{});
        base_t::absorb_display_info(j, out);
        return out;
    }

    /**
     * @brief Cached descriptor for this union.
     * @return Shared `UnionCellTypeDescriptor<Ts...>` instance.
     */
    static cell_type_descriptor_t descriptor() {
        static const auto d = std::make_shared<UnionCellTypeDescriptor<Ts...>>(DisplayInfo{
            .name = "Union",
            .description = "Closed-set polymorphic cell (one of fixed alternatives)",
        });
        return d;
    }

    /**
     * @brief Visit the active alternative with @p f, called as `f(storage)`.
     *
     * Mirrors `std::visit` over `std::variant`. The visitor sees the raw
     * storage of the active alternative (e.g. `std::int32_t` for an
     * `I32Cell` slot), so each overload should be written against the
     * storage type, not the wrapper.
     *
     * @code{.cpp}
     * parcel::visit(parcel::Overload{
     *     [](std::int32_t i) { ... },
     *     [](std::string const& s) { ... },
     * }, my_union);
     * @endcode
     */
    template <typename F>
    decltype(auto) visit(F&& f) {
        return std::visit(std::forward<F>(f), this->value);
    }

    /** @brief Const overload of @ref visit. */
    template <typename F>
    decltype(auto) visit(F&& f) const {
        return std::visit(std::forward<F>(f), this->value);
    }

    /**
     * @brief Returns a pointer to the active alternative if it lives at slot
     *        @p I, else `nullptr` — never throws.
     */
    template <std::size_t I>
    [[nodiscard]] auto* get_if() noexcept {
        return std::get_if<I>(&this->value);
    }

    /** @brief Const overload of `get_if<I>`. */
    template <std::size_t I>
    [[nodiscard]] auto const* get_if() const noexcept {
        return std::get_if<I>(&this->value);
    }

    /**
     * @brief Returns a pointer to the active alternative if its storage is
     *        @p S, else `nullptr` — never throws.
     */
    template <typename S>
    [[nodiscard]] S* get_if() noexcept {
        return std::get_if<S>(&this->value);
    }

    /** @brief Const overload of `get_if<S>`. */
    template <typename S>
    [[nodiscard]] S const* get_if() const noexcept {
        return std::get_if<S>(&this->value);
    }

    /** @brief `true` if the active alternative lives at slot @p I. */
    template <std::size_t I>
    [[nodiscard]] bool holds_alternative() const noexcept {
        return this->value.index() == I;
    }

    /** @brief `true` if the active alternative's storage type is @p S. */
    template <typename S>
    [[nodiscard]] bool holds() const noexcept {
        return std::holds_alternative<S>(this->value);
    }

private:
    template <typename F>
    auto visit_indexed(F&& f) const {
        return visit_indexed_impl(std::forward<F>(f), std::index_sequence_for<Ts...>{});
    }

    template <typename F, std::size_t... Is>
    auto visit_indexed_impl(F&& f, std::index_sequence<Is...>) const {
        using R = std::invoke_result_t<F&,
                                       std::integral_constant<std::size_t, 0>,
                                       typename nth_wrapper_t<0>::storage_t const&>;
        if constexpr (std::is_void_v<R>) {
            (void)((this->value.index() == Is
                        ? (f(std::integral_constant<std::size_t, Is>{}, std::get<Is>(this->value)),
                           true)
                        : false) ||
                   ...);
        } else {
            R result{};
            const bool matched =
                ((this->value.index() == Is ? (result = f(std::integral_constant<std::size_t, Is>{},
                                                          std::get<Is>(this->value)),
                                               true)
                                            : false) ||
                 ...);
            if (!matched) {
                throw std::runtime_error("UnionCell: valueless variant");
            }
            return result;
        }
    }

    template <std::size_t... Is>
    [[nodiscard]] std::partial_ordering compare_active(UnionCell const& other,
                                                       std::index_sequence<Is...>) const {
        std::partial_ordering result = std::partial_ordering::equivalent;
        const auto idx = this->value.index();
        const bool matched = ((idx == Is ? (result = compare_alt<Is>(other), true) : false) || ...);
        (void)matched;
        return result;
    }

    template <std::size_t I>
    [[nodiscard]] std::partial_ordering compare_alt(UnionCell const& other) const {
        using WrapperT = nth_wrapper_t<I>;
        WrapperT a(std::get<I>(this->value));
        WrapperT b(std::get<I>(other.value));
        return a.compare(b);
    }

    template <std::size_t... Is>
    static cell_t dispatch_from_json(const std::string_view active,
                                     json_t const& raw,
                                     ParcelRegistry const& reg,
                                     std::index_sequence<Is...>) {
        cell_t out;
        if (const bool matched = ((try_alt<Is>(active, raw, reg, out)) || ...); !matched) {
            throw KindMismatchException("UnionCell: active kind '" + std::string(active) +
                                            "' is not one of the alternatives",
                                        std::string(kind_id));
        }
        return out;
    }

    template <std::size_t I>
    static bool
    try_alt(std::string_view active, json_t const& raw, ParcelRegistry const& reg, cell_t& out) {
        using NthT = nth_wrapper_t<I>;
        if (active != NthT::kind_id) {
            return false;
        }
        const cell_t v = NthT::from_json(raw, reg);
        auto* typed = dynamic_cast<NthT*>(v.get());
        if (typed == nullptr) {
            throw KindMismatchException(
                "UnionCell: alternative from_json did not return expected type",
                std::string(kind_id));
        }
        auto u = std::make_shared<UnionCell<Ts...>>();
        u->value.template emplace<I>(std::move(typed->value));
        out = u;
        return true;
    }
};

/**
 * @brief `std::visit`-style free function over `UnionCell<Ts...>`.
 *
 * Calls @p f on the active alternative's storage. Mirrors
 * `std::visit(visitor, variant)`.
 */
template <typename F, CellLike... Ts>
decltype(auto) visit(F&& f, UnionCell<Ts...>& u) {
    return u.visit(std::forward<F>(f));
}

/** @brief Const overload of @ref visit. */
template <typename F, CellLike... Ts>
decltype(auto) visit(F&& f, UnionCell<Ts...> const& u) {
    return u.visit(std::forward<F>(f));
}

/**
 * @brief Free `get<I>` over `UnionCell<Ts...>` mirroring `std::get<I>(variant)`.
 */
template <std::size_t I, CellLike... Ts>
auto& get(UnionCell<Ts...>& u) {
    return u.template get<I>();
}

template <std::size_t I, CellLike... Ts>
auto const& get(UnionCell<Ts...> const& u) {
    return u.template get<I>();
}

/**
 * @brief Free `get<S>` selecting by storage type.
 */
template <typename S, CellLike... Ts>
S& get(UnionCell<Ts...>& u) {
    return u.template get<S>();
}

template <typename S, CellLike... Ts>
S const& get(UnionCell<Ts...> const& u) {
    return u.template get<S>();
}

/**
 * @brief Free `get_if<I>` over `UnionCell<Ts...>`; returns `nullptr` on miss.
 */
template <std::size_t I, CellLike... Ts>
auto* get_if(UnionCell<Ts...>* u) noexcept {
    return u ? u->template get_if<I>() : nullptr;
}

template <std::size_t I, CellLike... Ts>
auto const* get_if(UnionCell<Ts...> const* u) noexcept {
    return u ? u->template get_if<I>() : nullptr;
}

template <typename S, CellLike... Ts>
S* get_if(UnionCell<Ts...>* u) noexcept {
    return u ? u->template get_if<S>() : nullptr;
}

template <typename S, CellLike... Ts>
S const* get_if(UnionCell<Ts...> const* u) noexcept {
    return u ? u->template get_if<S>() : nullptr;
}

/**
 * @brief Lambda-overload set helper for `parcel::visit`.
 *
 * Standard CTAD trick that turns a brace-enclosed list of lambdas into one
 * callable whose `operator()` is overloaded across them. Pair with
 * `parcel::visit` over a `UnionCell` to dispatch on the active alternative.
 *
 * @code{.cpp}
 * parcel::visit(parcel::Overload{
 *     [](std::int32_t i) { ... },
 *     [](std::string const& s) { ... },
 * }, my_union);
 * @endcode
 */
template <class... Fs>
struct Overload : Fs... {
    using Fs::operator()...;
};

template <class... Fs>
Overload(Fs...) -> Overload<Fs...>;

}  // namespace parcel
