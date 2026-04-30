#pragma once

/**
 * @file defaults.h
 * @brief The `default_cell_for<T>` trait that drives `FieldsBuilder` field-type inference.
 *
 * `default_cell_for<T>` maps a raw field type to the cell wrapper used for
 * it on the wire. Out of the box every `PrimitiveStorage` type maps to
 * `PrimitiveCell<T>`, `std::vector<T>` to `TypedListCell`, `std::map<std::string, T>`
 * to `TypedMapCell`, `std::variant<Ts...>` to `UnionCell`, and `std::optional<T>`
 * to the inner wrapper. User code can specialize the trait for custom field
 * types so the parameter-less `FieldsBuilder::field<MemberPtr>(key)` works.
 * See the README "Default cell wrappers" section.
 */

#include <parcel/list.h>
#include <parcel/map.h>
#include <parcel/primitive.h>
#include <parcel/union.h>

#include <array>
#include <cstddef>
#include <deque>
#include <list>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <variant>
#include <vector>

namespace parcel {

/**
 * @brief Maps a raw field type to its default `ICell` wrapper.
 *
 * Specialize this for custom field types (e.g. user structs) to enable
 * wrapper inference in `FieldsBuilder::field<MemberPtr>(key)` without an
 * explicit `CellT`. Primary template intentionally has no `type` member
 * so unsupported types fail SFINAE / concept checks loudly.
 *
 * @tparam T Field type to map.
 * @see FieldsBuilder
 * @see HasDefaultCellWrapper
 */
template <typename T, typename = void>
struct default_cell_for {};

/** @brief Any `PrimitiveStorage` type → `PrimitiveCell<T>`. */
template <PrimitiveStorage T>
struct default_cell_for<T> {
    using type = PrimitiveCell<T>;
};

/** @brief Convenience alias for `default_cell_for<T>::type`. */
template <typename T>
using default_cell_for_t = default_cell_for<T>::type;

/**
 * @brief Concept matching types for which a `default_cell_for` mapping exists.
 *
 * @tparam T Candidate type.
 */
template <typename T>
concept HasDefaultCellWrapper = requires { typename default_cell_for<T>::type; };

/**
 * @brief `std::vector<T>` (with a default-wrappable @p T) → `TypedListCell<wrapper>`.
 *
 * `TypedListCell`'s storage is `std::vector<wrapper::storage_t>`, which
 * equals `std::vector<T>` for primitive wrappers and `std::vector<Payload>`
 * for struct wrappers — so `MemberFieldDescriptor`'s direct-construction
 * path Just Works.
 */
template <typename T>
    requires HasDefaultCellWrapper<T>
struct default_cell_for<std::vector<T>> {
    using type = TypedListCell<default_cell_for_t<T>>;
};

/** @brief `std::map<std::string, T>` (with a default-wrappable @p T) → `TypedMapCell<wrapper>`. */
template <typename T>
    requires HasDefaultCellWrapper<T>
struct default_cell_for<std::map<std::string, T>> {
    using type = TypedMapCell<default_cell_for_t<T>>;
};

/** @brief `std::unordered_map<std::string, T>` → `TypedMapCell<wrapper>` (sorted on the wire). */
template <typename T>
    requires HasDefaultCellWrapper<T>
struct default_cell_for<std::unordered_map<std::string, T>> {
    using type = TypedMapCell<default_cell_for_t<T>>;
};

/** @brief `std::array<T, N>` → `TypedListCell<wrapper>` (size erased on the wire). */
template <typename T, std::size_t N>
    requires HasDefaultCellWrapper<T>
struct default_cell_for<std::array<T, N>> {
    using type = TypedListCell<default_cell_for_t<T>>;
};

/** @brief `std::deque<T>` → `TypedListCell<wrapper>`. */
template <typename T>
    requires HasDefaultCellWrapper<T>
struct default_cell_for<std::deque<T>> {
    using type = TypedListCell<default_cell_for_t<T>>;
};

/** @brief `std::list<T>` → `TypedListCell<wrapper>`. */
template <typename T>
    requires HasDefaultCellWrapper<T>
struct default_cell_for<std::list<T>> {
    using type = TypedListCell<default_cell_for_t<T>>;
};

/** @brief `std::set<T>` → `TypedListCell<wrapper>` (the wire form is a list — sort/unique is the
 * user's responsibility). */
template <typename T>
    requires HasDefaultCellWrapper<T>
struct default_cell_for<std::set<T>> {
    using type = TypedListCell<default_cell_for_t<T>>;
};

/** @brief `std::variant<Ts...>` (with default-wrappable @p Ts) → `UnionCell<wrappers...>`. */
template <typename... Ts>
    requires(HasDefaultCellWrapper<Ts> && ...)
struct default_cell_for<std::variant<Ts...>> {
    using type = UnionCell<default_cell_for_t<Ts>...>;
};

/**
 * @brief `std::optional<T>` (with a default-wrappable @p T) → the inner wrapper itself.
 *
 * The "optional-ness" is represented by the field being not-required and
 * by `nullopt` round-tripping as the JSON key being absent — not by a
 * separate wrapper layer.
 */
template <typename T>
    requires HasDefaultCellWrapper<T>
struct default_cell_for<std::optional<T>> {
    using type = default_cell_for_t<T>;
};

namespace detail {
// Normalize raw call-site types into the canonical storage type that
// default_cell_for is keyed on. Without this, string literals (`const char[N]`)
// and `const char*` would not resolve to StringCell.
template <typename T>
struct cell_arg {
    using type = T;
};
template <std::size_t N>
struct cell_arg<char[N]> {  // NOLINT(modernize-avoid-c-arrays) — must match decay-from string
                            // literal
    using type = std::string;
};
template <std::size_t N>
struct cell_arg<const char[N]> {  // NOLINT(modernize-avoid-c-arrays) — must match decay-from string
                                  // literal
    using type = std::string;
};
template <>
struct cell_arg<char*> {
    using type = std::string;
};
template <>
struct cell_arg<const char*> {
    using type = std::string;
};

template <typename T>
using cell_arg_t = typename cell_arg<std::remove_cvref_t<T>>::type;
}  // namespace detail

/**
 * @brief Wrap a raw value into its default cell, returning a `shared_ptr` to the cell.
 *
 * Picks the wrapper via `default_cell_for_t`, with a small normalization
 * layer so `cell("hi")` resolves to `StringCell` rather than tripping over
 * `const char*`.
 *
 * @tparam T Argument type — its decayed form must satisfy `HasDefaultCellWrapper`.
 * @param  v Raw value.
 * @return `std::shared_ptr<Cell>` owning the new cell.
 */
template <typename T>
    requires HasDefaultCellWrapper<detail::cell_arg_t<T>>
auto cell(T&& v) {
    using Cell = default_cell_for_t<detail::cell_arg_t<T>>;
    return Cell::of(std::forward<T>(v));
}

/**
 * @brief Build a heterogeneous `ListCell` from raw values, wrapping each via
 *        `parcel::cell(...)`.
 *
 * @code{.cpp}
 * auto l = parcel::make_list(1, std::string("hi"), true);
 * @endcode
 *
 * @tparam Ts Argument types — each must satisfy `HasDefaultCellWrapper` after
 *            `cell_arg` normalization.
 * @return `std::shared_ptr<ListCell>` owning the new list.
 */
template <typename... Ts>
    requires(HasDefaultCellWrapper<detail::cell_arg_t<Ts>> && ...)
[[nodiscard]] std::shared_ptr<ListCell> make_list(Ts&&... xs) {
    std::vector<cell_t> elems;
    elems.reserve(sizeof...(Ts));
    (elems.push_back(cell(std::forward<Ts>(xs))), ...);
    return ListCell::of(std::move(elems));
}

/**
 * @brief Build a heterogeneous `MapCell` from a brace list of `{key, cell}` pairs.
 *
 * For homogeneous maps prefer `TypedMapCell<T>` directly. This helper is for
 * the heterogeneous case where each value is already a `cell_t` (typically
 * built via `parcel::cell(value)`).
 *
 * @code{.cpp}
 * auto m = parcel::make_map({{"x", parcel::cell(1)},
 *                            {"y", parcel::cell(std::string("hi"))}});
 * @endcode
 */
[[nodiscard]] inline std::shared_ptr<MapCell>
make_map(const std::initializer_list<std::pair<const std::string, cell_t>> entries) {
    std::map<std::string, cell_t> data(entries.begin(), entries.end());
    return MapCell::of(std::move(data));
}

}  // namespace parcel

/**
 * @brief Register a cell type as the default wrapper for its payload type.
 *
 * Expands to a `parcel::default_cell_for` specialization mapping
 * `CellT::storage_t` to `CellT`, so `FieldsBuilder::field<MemberPtr>(key)`
 * can infer the wrapper from the field type alone (and the free
 * `parcel::cell(value)` factory picks it up too).
 *
 * Use once per user-defined cell, at namespace scope (typically just below
 * the class definition, outside any namespace):
 *
 * @code{.cpp}
 * struct Address { ... };
 * class AddressCell : public parcel::StructCell<AddressCell, Address, "address"> { ... };
 * PARCEL_DEFAULT_CELL(AddressCell);
 * @endcode
 *
 * Equivalent hand-rolled form:
 *
 * @code{.cpp}
 * template <>
 * struct parcel::default_cell_for<Address> {
 *     using type = AddressCell;
 * };
 * @endcode
 *
 * The cell type must be complete and expose a `storage_t` typedef — every
 * cell derived from `parcel::BaseCell` (including `StructCell`) does.
 *
 * @param CellT Fully-qualified cell type (e.g. `demo::AddressCell`).
 */
#define PARCEL_DEFAULT_CELL(CellT)                                                                 \
    template <>                                                                                    \
    struct parcel::default_cell_for<typename CellT::storage_t> {                                   \
        using type = CellT;                                                                        \
    }
