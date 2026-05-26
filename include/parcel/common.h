#pragma once

/**
 * @file common.h
 * @brief The shared vocabulary aliases re-exported from commons and the
 *        compile-time kind-id machinery.
 *
 * The `parcel::i128` / `parcel::u128` aliases are re-exported from
 * `comms::i128` / `comms::u128` and become available when 128-bit integers
 * exist on the toolchain. Headers that declare 128-bit cells (`primitive.h`,
 * `builtins.h`) gate them behind commons' own `COMMONS_HAS_INT128` feature
 * macro (defined in `<commons/types.hpp>`), so the gate always matches the
 * source of the types.
 *
 * This header also re-exports `parcel::FixedString` and `parcel::DisplayInfo`
 * from commons, and centralizes the shared bits used by composite cells to
 * synthesize their wire `kind_id` at compile time:
 *
 *   - `FixedString<N>` — C++20 class-NTTP string-literal carrier.
 *   - `id_join_v<Parts...>` — concatenate `std::string_view` references.
 *   - `id_join_lit_v<Parts...>` — concatenate `FixedString` literals.
 *   - `prefixed_id_v<Prefix, Tail>` — `FixedString` literal prefix joined
 *     with a `std::string_view` reference tail.
 */

#include <parcel/json.h>

#include <commons/display_info.hpp>
#include <commons/fixed_string.hpp>
#include <commons/types.hpp>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>

namespace parcel {

#if defined(COMMONS_HAS_INT128)
/** @brief Unsigned 128-bit integer — re-exported from `comms::u128`. */
using comms::u128;

/** @brief Signed 128-bit integer — re-exported from `comms::i128`. */
using comms::i128;
#endif

/**
 * @brief Compile-time fixed-length string usable as a non-type template
 *        parameter (C++20 class-NTTP).
 *
 * Parcel reuses commons' `comms::FixedString` rather than maintaining its own
 * carrier: it is API-compatible for the way parcel uses it (`.view()` only)
 * and is itself used by commons as a class-NTTP placeholder, so it is valid in
 * that role here too.
 *
 * Exposed unqualified as `parcel::FixedString`; it lets a
 * `StructCell<Derived, Payload, "person">` carry the bare struct id in its
 * template signature and lets library authors compose kind ids out of fixed
 * string literals via `id_join_lit_v`.
 */
using comms::FixedString;

/**
 * @brief Descriptive metadata attached to a cell or descriptor — re-exported
 *        from `comms::DisplayInfo`.
 *
 * Every `ICell` may carry an optional `DisplayInfo` (name / description / typed
 * `comms::Icon` / typed `comms::Color`), serialized under the JSON key `"d"`.
 * It is purely descriptive metadata for UI/tooling use and never affects the
 * wire identity (`"k"`) or value (`"v"`) of a cell. It serializes via commons'
 * free ADL `to_json` / `from_json` (there is no member `to_json()`).
 */
using DisplayInfo = comms::DisplayInfo;

/**
 * @brief Compile-time concatenation of `std::string_view` references into a
 *        single `std::string_view` with static storage.
 *
 * Used internally by composite cells (typed list, typed map, struct) to join
 * a wire prefix with a nested cell's static `kind_id` without per-class
 * lambda boilerplate. The result has the same lifetime as a static
 * constexpr — it is safe to expose as `static constexpr std::string_view
 * kind_id`.
 *
 * @tparam Parts References to `std::string_view` constants to concatenate
 *               in order.
 */
template <std::string_view const&... Parts>
struct id_join {
    static constexpr std::size_t total_size = (Parts.size() + ... + 0);
    static constexpr auto buf = []() {
        std::array<char, total_size> r{};
        std::size_t off = 0;
        ((std::ranges::copy(Parts, r.begin() + off), off += Parts.size()), ...);
        return r;
    }();
    static constexpr std::string_view value{buf.data(), buf.size()};
};

/** @brief Convenience alias yielding the joined `std::string_view`. */
template <std::string_view const&... Parts>
inline constexpr std::string_view id_join_v = id_join<Parts...>::value;

/**
 * @brief Compile-time concatenation of `FixedString` literals into a
 *        single `std::string_view` with static storage.
 *
 * Library-author-facing variant: lets a CRTP base form its own
 * `kind_id` directly from string literals plus a `FixedString` NTTP it
 * received as a template parameter, without forcing the author to declare
 * a bridge `static constexpr std::string_view`:
 *
 * @code{.cpp}
 * template <typename Self, parcel::FixedString EventId>
 * class BaseEvent : public parcel::SelfStructCell<Self> {
 *     static constexpr std::string_view kind_id =
 *         parcel::id_join_lit_v<"s:event:", EventId>;
 *     // ...
 * };
 * @endcode
 *
 * @tparam Parts `FixedString` literals to concatenate in order.
 */
template <FixedString... Parts>
struct id_join_lit {
    static constexpr std::size_t total_size = (Parts.view().size() + ... + 0);
    static constexpr auto buf = []() {
        std::array<char, total_size> r{};
        std::size_t off = 0;
        ((std::ranges::copy(Parts.view(), r.begin() + off), off += Parts.view().size()), ...);
        return r;
    }();
    static constexpr std::string_view value{buf.data(), buf.size()};
};

/** @brief Convenience alias yielding the joined `std::string_view`. */
template <FixedString... Parts>
inline constexpr std::string_view id_join_lit_v = id_join_lit<Parts...>::value;

/**
 * @brief Compile-time concatenation of a `FixedString` literal prefix with
 *        a `std::string_view` reference tail into one `std::string_view`
 *        with static storage.
 *
 * Hybrid bridge between the two `id_join` flavors: the prefix is a literal
 * (so callers can hardcode `"l:"` / `"m:"` / `"s:"` at the use site without
 * declaring a bridge `string_view`), while the tail is a runtime `string_view`
 * constant (so it can name another cell's `static constexpr std::string_view
 * kind_id`).
 *
 * @code{.cpp}
 * static constexpr std::string_view kind_id = parcel::prefixed_id_v<"l:", T::kind_id>;
 * @endcode
 *
 * @tparam Prefix Literal prefix.
 * @tparam Tail   Reference to a `std::string_view` constant appended after
 *                @p Prefix.
 */
template <FixedString Prefix, std::string_view const& Tail>
struct prefixed_id {
    static constexpr std::size_t total_size = Prefix.view().size() + Tail.size();
    static constexpr auto buf = []() {
        std::array<char, total_size> r{};
        std::ranges::copy(Prefix.view(), r.begin());
        std::ranges::copy(Tail, r.begin() + Prefix.view().size());
        return r;
    }();
    static constexpr std::string_view value{buf.data(), buf.size()};
};

/** @brief Convenience alias yielding the joined `std::string_view`. */
template <FixedString Prefix, std::string_view const& Tail>
inline constexpr std::string_view prefixed_id_v = prefixed_id<Prefix, Tail>::value;

}  // namespace parcel
