#pragma once

/**
 * @file common.h
 * @brief Platform-detected 128-bit integer aliases, the `PARCEL_HAS_INT128`
 *        feature macro, and shared compile-time kind-id machinery.
 *
 * GCC/Clang expose `__int128_t` / `__uint128_t` via the `__SIZEOF_INT128__`
 * predefined macro. When present, `PARCEL_HAS_INT128` is defined and the
 * `parcel::i128` / `parcel::u128` aliases become available. Headers that
 * declare 128-bit cells (`primitive.h`, `builtins.h`) gate them behind this
 * macro so the library still compiles on platforms that lack 128-bit support.
 *
 * In addition, this header centralizes the shared bits used by composite
 * cells to synthesize their wire `kind_id` at compile time:
 *
 *   - `fixed_string<N>` — C++20 class-NTTP string-literal carrier.
 *   - `id_join_v<Parts...>` — concatenate `std::string_view` references.
 *   - `id_join_lit_v<Parts...>` — concatenate `fixed_string` literals.
 *   - `prefixed_id_v<Prefix, Tail>` — `fixed_string` literal prefix joined
 *     with a `std::string_view` reference tail.
 */

#include <parcel/json.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>

namespace parcel {

#if defined(__SIZEOF_INT128__)
/**
 * @brief Defined when the compiler provides 128-bit integer types.
 *
 * Headers that declare 128-bit cells gate their declarations behind this
 * macro. Doxygen sees it as always defined so the 128-bit aliases render
 * regardless of the host compiler.
 */
#define PARCEL_HAS_INT128 1

/** @brief Unsigned 128-bit integer alias for `__uint128_t`. */
using u128 = __uint128_t;

/** @brief Signed 128-bit integer alias for `__int128_t`. */
using i128 = __int128_t;
#endif

/**
 * @brief Compile-time fixed-length string usable as a non-type template
 *        parameter (C++20 class-NTTP).
 *
 * Lets a `StructCell<Derived, Payload, "person">` carry the bare struct id
 * in its template signature, and lets library authors compose kind ids out
 * of fixed string literals via `id_join_lit_v`.
 *
 * @tparam N Length of the string literal including its trailing `'\0'`.
 */
template <std::size_t N>
struct fixed_string {
    // C-style array storage is required for the C++20 structural-NTTP
    // representation of a string literal — std::array is not yet a valid
    // NTTP storage type for this use.
    char data[N]{};  // NOLINT(modernize-avoid-c-arrays)

    constexpr fixed_string(const char (&s)[N]) {  // NOLINT(modernize-avoid-c-arrays)
        for (std::size_t i = 0; i < N; ++i) {
            data[i] = s[i];
        }
    }

    /** @brief View of the string excluding the trailing `'\0'`. */
    [[nodiscard]] constexpr std::string_view view() const {
        return {data, N - 1};
    }
};

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
 * @brief Compile-time concatenation of `fixed_string` literals into a
 *        single `std::string_view` with static storage.
 *
 * Library-author-facing variant: lets a CRTP base form its own
 * `kind_id` directly from string literals plus a `fixed_string` NTTP it
 * received as a template parameter, without forcing the author to declare
 * a bridge `static constexpr std::string_view`:
 *
 * @code{.cpp}
 * template <typename Self, parcel::fixed_string EventId>
 * class BaseEvent : public parcel::SelfStructCell<Self> {
 *     static constexpr std::string_view kind_id =
 *         parcel::id_join_lit_v<"s:event:", EventId>;
 *     // ...
 * };
 * @endcode
 *
 * @tparam Parts `fixed_string` literals to concatenate in order.
 */
template <fixed_string... Parts>
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
template <fixed_string... Parts>
inline constexpr std::string_view id_join_lit_v = id_join_lit<Parts...>::value;

/**
 * @brief Compile-time concatenation of a `fixed_string` literal prefix with
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
template <fixed_string Prefix, std::string_view const& Tail>
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
template <fixed_string Prefix, std::string_view const& Tail>
inline constexpr std::string_view prefixed_id_v = prefixed_id<Prefix, Tail>::value;

}  // namespace parcel
