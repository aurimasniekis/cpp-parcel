#pragma once

/**
 * @file json.h
 * @brief `nlohmann::json` typedef and ADL serializers shared across cell types.
 *
 * Provides the `parcel::json_t` alias plus three ADL serializers that every
 * cell relies on: `std::optional<T>` (round-trips as the value or JSON
 * `null`), `__uint128_t`, and `__int128_t`. The 128-bit overloads encode
 * values as decimal strings — matching Protobuf, gRPC, and most JS APIs for
 * integers above 2^53 — because nlohmann silently narrows them through
 * `long long` and loses precision past 2^64.
 */

#include <parcel/error.h>

#include <nlohmann/json.hpp>

#include <algorithm>
#include <cstddef>
#include <optional>
#include <string>

namespace nlohmann {

/**
 * @brief ADL serializer for `std::optional<T>`.
 *
 * `nullopt` round-trips as JSON `null`; any value is serialized via the
 * inner `T`'s own serializer.
 *
 * @tparam T Wrapped value type.
 */
template <typename T>
struct adl_serializer<std::optional<T>> {
    /**
     * @brief Serialize an optional as either JSON `null` or its value.
     * @param j   Output JSON node.
     * @param opt Optional to serialize.
     */
    static void to_json(json& j, const std::optional<T>& opt) {
        if (opt.has_value()) {
            j = *opt;
        } else {
            j = nullptr;
        }
    }

    /**
     * @brief Deserialize an optional, treating JSON `null` as `nullopt`.
     * @param j   Input JSON node.
     * @param opt Optional to populate.
     */
    static void from_json(const json& j, std::optional<T>& opt) {
        if (j.is_null()) {
            opt = std::nullopt;
        } else {
            opt = j.get<T>();
        }
    }
};

// 128-bit integers don't have native JSON support — nlohmann silently narrows
// them through `long long`, losing precision above 2^64. We encode them as
// decimal strings, matching the convention used by Protobuf, gRPC, and most
// JS APIs for >53-bit integers.
#ifdef __SIZEOF_INT128__

/**
 * @brief ADL serializer for `__uint128_t` — encodes as a decimal JSON string.
 */
template <>
struct adl_serializer<__uint128_t> {
    /**
     * @brief Encode an unsigned 128-bit integer as a decimal JSON string.
     * @param j Output JSON node (becomes a string).
     * @param v Value to encode.
     */
    static void to_json(json& j, __uint128_t v) {
        if (v == 0) {
            j = "0";
            return;
        }
        std::string out;
        while (v != 0) {
            out.push_back(static_cast<char>('0' + static_cast<int>(v % 10)));
            v /= 10;
        }
        std::ranges::reverse(out);
        j = std::move(out);
    }

    /**
     * @brief Decode a decimal JSON string into an unsigned 128-bit integer.
     * @param j   Input JSON node — must be a string of decimal digits.
     * @param out Result.
     * @throws std::runtime_error if `j` is not a string, is empty, or
     *         contains non-digit characters.
     */
    static void from_json(const json& j, __uint128_t& out) {
        if (!j.is_string()) {
            throw parcel::InvalidJsonException("u128 must be encoded as a JSON string", "u128");
        }
        auto const& s = j.get_ref<std::string const&>();
        if (s.empty()) {
            throw parcel::InvalidJsonException("u128 string is empty", "u128");
        }
        constexpr __uint128_t u128_max = ~static_cast<__uint128_t>(0);
        __uint128_t v = 0;
        for (const char c : s) {
            if (c < '0' || c > '9') {
                throw parcel::InvalidJsonException("non-digit character in u128 string", "u128");
            }
            const auto d = static_cast<__uint128_t>(c - '0');
            if (v > (u128_max - d) / 10) {
                throw parcel::InvalidJsonException("u128 string out of range", "u128");
            }
            v = v * 10 + d;
        }
        out = v;
    }
};

/**
 * @brief ADL serializer for `__int128_t` — encodes as a signed decimal JSON string.
 */
template <>
struct adl_serializer<__int128_t> {
    /**
     * @brief Encode a signed 128-bit integer as a decimal JSON string.
     *
     * Uses an unsigned-magnitude detour to avoid signed overflow on
     * `i128_min`, whose positive counterpart does not fit in `__int128_t`.
     *
     * @param j Output JSON node (becomes a string).
     * @param v Value to encode.
     */
    static void to_json(json& j, const __int128_t v) {
        if (v == 0) {
            j = "0";
            return;
        }
        const bool negative = v < 0;
        // Compute the unsigned magnitude without signed overflow on i128_min:
        //   i128_min has no positive counterpart in __int128_t, so we add 1 first.
        __uint128_t mag =
            negative ? static_cast<__uint128_t>(-(v + 1)) + 1 : static_cast<__uint128_t>(v);
        std::string out;
        while (mag != 0) {
            out.push_back(static_cast<char>('0' + static_cast<int>(mag % 10)));
            mag /= 10;
        }
        if (negative) {
            out.push_back('-');
        }
        std::ranges::reverse(out);
        j = std::move(out);
    }

    /**
     * @brief Decode a signed decimal JSON string into a 128-bit integer.
     *
     * Accepts an optional leading `+` or `-` sign. Negative values are
     * mapped via the magnitude path so `i128_min` round-trips correctly.
     *
     * @param j   Input JSON node — must be a string of optional sign + digits.
     * @param out Result.
     * @throws std::runtime_error if `j` is not a string, is empty,
     *         contains a sign without digits, or contains non-digit characters.
     */
    static void from_json(const json& j, __int128_t& out) {
        if (!j.is_string()) {
            throw parcel::InvalidJsonException("i128 must be encoded as a JSON string", "i128");
        }
        auto const& s = j.get_ref<std::string const&>();
        if (s.empty()) {
            throw parcel::InvalidJsonException("i128 string is empty", "i128");
        }
        std::size_t i = 0;
        bool negative = false;
        if (s[0] == '-') {
            negative = true;
            i = 1;
        } else if (s[0] == '+') {
            i = 1;
        }
        if (i == s.size()) {
            throw parcel::InvalidJsonException("i128 string has sign but no digits", "i128");
        }
        // Permissible magnitudes: 0 .. 2^127     when negative (i128_min)
        //                         0 .. 2^127 - 1 when non-negative.
        constexpr __uint128_t neg_mag_max = static_cast<__uint128_t>(1) << 127;
        constexpr __uint128_t pos_mag_max = neg_mag_max - 1;
        __uint128_t mag = 0;
        for (; i < s.size(); ++i) {
            const char c = s[i];
            if (c < '0' || c > '9') {
                throw parcel::InvalidJsonException("non-digit character in i128 string", "i128");
            }
            const auto d = static_cast<__uint128_t>(c - '0');
            if (mag > (neg_mag_max - d) / 10) {
                throw parcel::InvalidJsonException("i128 string out of range", "i128");
            }
            mag = mag * 10 + d;
        }
        if (!negative && mag > pos_mag_max) {
            throw parcel::InvalidJsonException("i128 string out of range", "i128");
        }
        if (negative) {
            // Map magnitude → negative without overflow on i128_min (mag == 2^127).
            //   out = -mag, computed as: -(mag - 1) - 1
            const __uint128_t mm1 = mag - 1;
            const __int128_t partial = -static_cast<__int128_t>(mm1);
            out = partial - 1;
        } else {
            out = static_cast<__int128_t>(mag);
        }
    }
};

#endif  // __SIZEOF_INT128__

}  // namespace nlohmann

namespace parcel {

/** @brief Project-wide alias for `nlohmann::json`. */
using json_t = nlohmann::json;

}  // namespace parcel
