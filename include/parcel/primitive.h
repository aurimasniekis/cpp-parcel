#pragma once

/**
 * @file primitive.h
 * @brief `PrimitiveCell<T>` plus the per-storage `PrimitiveTraits<T>` specializations.
 *
 * Defines the leaf cell type for scalar values: booleans, `char`, signed
 * and unsigned integers (8 to 64 bit, plus 128 bit when supported), the
 * IEEE-754 floats, and `std::string`. Each specialization of
 * `PrimitiveTraits<T>` carries the wire kind id (e.g. `"i32"`, `"string"`)
 * and a default human-readable name + description used by the cell's
 * descriptor.
 */

#include <parcel/cell.h>
#include <parcel/common.h>
#include <parcel/descriptor.h>

#include <algorithm>
#include <memory>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>

namespace parcel::primitives {

/**
 * @brief Trait carrying wire id and default meta for a primitive storage type.
 *
 * Specialized for every type accepted by the `PrimitiveStorage` concept.
 * Each specialization exposes:
 *   - `type_id`     — kind id used on the wire (e.g. `"i32"`, `"string"`).
 *   - `name`        — default name applied to the cell's descriptor.
 *   - `description` — default description applied to the cell's descriptor.
 *
 * @tparam T A primitive storage type.
 */
template <typename T>
struct PrimitiveTraits;

/** @brief Traits for `char` — wire kind `"char"`. */
template <>
struct PrimitiveTraits<char> {
    static constexpr std::string_view type_id = "char";
    static constexpr std::string_view name = "Char";
    static constexpr std::string_view description = "Character.";
};

/** @brief Traits for `bool` — wire kind `"bool"`. */
template <>
struct PrimitiveTraits<bool> {
    static constexpr std::string_view type_id = "bool";
    static constexpr std::string_view name = "Bool";
    static constexpr std::string_view description = "Boolean value.";
};

/** @brief Traits for `std::uint8_t` — wire kind `"u8"`. */
template <>
struct PrimitiveTraits<std::uint8_t> {
    static constexpr std::string_view type_id = "u8";
    static constexpr std::string_view name = "U8";
    static constexpr std::string_view description = "Unsigned 8-bit integer.";
};

/** @brief Traits for `std::uint16_t` — wire kind `"u16"`. */
template <>
struct PrimitiveTraits<std::uint16_t> {
    static constexpr std::string_view type_id = "u16";
    static constexpr std::string_view name = "U16";
    static constexpr std::string_view description = "Unsigned 16-bit integer.";
};

/** @brief Traits for `std::uint32_t` — wire kind `"u32"`. */
template <>
struct PrimitiveTraits<std::uint32_t> {
    static constexpr std::string_view type_id = "u32";
    static constexpr std::string_view name = "U32";
    static constexpr std::string_view description = "Unsigned 32-bit integer.";
};

/** @brief Traits for `std::uint64_t` — wire kind `"u64"`. */
template <>
struct PrimitiveTraits<std::uint64_t> {
    static constexpr std::string_view type_id = "u64";
    static constexpr std::string_view name = "U64";
    static constexpr std::string_view description = "Unsigned 64-bit integer.";
};

#ifdef PARCEL_HAS_INT128
/** @brief Traits for `u128` — wire kind `"u128"`. Encoded as a decimal string. */
template <>
struct PrimitiveTraits<u128> {
    static constexpr std::string_view type_id = "u128";
    static constexpr std::string_view name = "U128";
    static constexpr std::string_view description = "Unsigned 128-bit integer.";
};
#endif

/** @brief Traits for `std::int8_t` — wire kind `"i8"`. */
template <>
struct PrimitiveTraits<std::int8_t> {
    static constexpr std::string_view type_id = "i8";
    static constexpr std::string_view name = "I8";
    static constexpr std::string_view description = "Signed 8-bit integer.";
};

/** @brief Traits for `std::int16_t` — wire kind `"i16"`. */
template <>
struct PrimitiveTraits<std::int16_t> {
    static constexpr std::string_view type_id = "i16";
    static constexpr std::string_view name = "I16";
    static constexpr std::string_view description = "Signed 16-bit integer.";
};

/** @brief Traits for `std::int32_t` — wire kind `"i32"`. */
template <>
struct PrimitiveTraits<std::int32_t> {
    static constexpr std::string_view type_id = "i32";
    static constexpr std::string_view name = "I32";
    static constexpr std::string_view description = "Signed 32-bit integer.";
};

/** @brief Traits for `std::int64_t` — wire kind `"i64"`. */
template <>
struct PrimitiveTraits<std::int64_t> {
    static constexpr std::string_view type_id = "i64";
    static constexpr std::string_view name = "I64";
    static constexpr std::string_view description = "Signed 64-bit integer.";
};

#ifdef PARCEL_HAS_INT128
/** @brief Traits for `i128` — wire kind `"i128"`. Encoded as a decimal string. */
template <>
struct PrimitiveTraits<i128> {
    static constexpr std::string_view type_id = "i128";
    static constexpr std::string_view name = "I128";
    static constexpr std::string_view description = "Signed 128-bit integer.";
};
#endif

/** @brief Traits for `float` — wire kind `"f32"`. */
template <>
struct PrimitiveTraits<float> {
    static constexpr std::string_view type_id = "f32";
    static constexpr std::string_view name = "F32";
    static constexpr std::string_view description = "32-bit floating-point number.";
};

/** @brief Traits for `double` — wire kind `"f64"`. */
template <>
struct PrimitiveTraits<double> {
    static constexpr std::string_view type_id = "f64";
    static constexpr std::string_view name = "F64";
    static constexpr std::string_view description = "64-bit floating-point number.";
};

/** @brief Traits for `std::string` — wire kind `"string"`. */
template <>
struct PrimitiveTraits<std::string> {
    static constexpr std::string_view type_id = "string";
    static constexpr std::string_view name = "String";
    static constexpr std::string_view description = "UTF-8 encoded string.";
};

}  // namespace parcel::primitives

namespace parcel {
class ParcelRegistry;

/**
 * @brief Concept naming the set of types `PrimitiveCell` accepts as storage.
 *
 * Covers `char`, `bool`, the fixed-width integers (`u8`-`u64`, `i8`-`i64`,
 * plus 128-bit on supporting toolchains), `float`, `double`, and
 * `std::string`.
 *
 * @tparam T Candidate storage type.
 */
template <typename T>
concept PrimitiveStorage =
    std::same_as<T, char> || std::same_as<T, std::uint8_t> || std::same_as<T, std::uint16_t> ||
    std::same_as<T, std::uint32_t> || std::same_as<T, std::uint64_t> ||
    std::same_as<T, std::int8_t> || std::same_as<T, std::int16_t> ||
    std::same_as<T, std::int32_t> || std::same_as<T, std::int64_t> || std::same_as<T, float> ||
    std::same_as<T, double> || std::same_as<T, bool> ||
#ifdef PARCEL_HAS_INT128
    std::same_as<T, u128> || std::same_as<T, i128> ||
#endif
    std::same_as<T, std::string>;

/**
 * @brief Leaf cell wrapping a single scalar value of type @p T.
 *
 * Wire shape:
 * @code{.json}
 * {"k": "<type_id>", "v": <value>}
 * @endcode
 * with `<type_id>` taken from `PrimitiveTraits<T>::type_id` (e.g. `"i32"`,
 * `"string"`). 128-bit integers serialize their `"v"` as a decimal string
 * via the ADL serializers in `json.h`.
 *
 * @tparam T A type satisfying `PrimitiveStorage`.
 * @see PrimitiveTraits — wire id and default meta per @p T.
 * @see SimpleCellTypeDescriptor — descriptor used by `descriptor()`.
 */
template <PrimitiveStorage T>
class PrimitiveCell : public BaseCell<PrimitiveCell<T>, T> {
    using trait_t = primitives::PrimitiveTraits<T>;
    using base_t = BaseCell<PrimitiveCell<T>, T>;

public:
    using base_t::base_t;
    using base_t::operator=;

    /** @brief Wire kind id (taken from `PrimitiveTraits<T>::type_id`). */
    static constexpr std::string_view kind_id = trait_t::type_id;

    /**
     * @brief Render the value as a string.
     *
     * Strings are returned verbatim, `bool` becomes `"true"` / `"false"`,
     * `char` becomes a 1-byte string, 128-bit integers are formatted by
     * hand (since `std::to_string` has no overload), and everything else
     * routes through `std::to_string`.
     *
     * @return Compact textual form of the value.
     */
    [[nodiscard]] std::string to_string() const override {
        if constexpr (std::same_as<T, std::string>) {
            return this->value;
        } else if constexpr (std::same_as<T, bool>) {
            return this->value ? "true" : "false";
        } else if constexpr (std::same_as<T, char>) {
            return std::string(1, this->value);
#ifdef PARCEL_HAS_INT128
        } else if constexpr (std::same_as<T, u128> || std::same_as<T, i128>) {
            // std::to_string has no overload for __int128_t / __uint128_t.
            // Build the digits manually, LSB-first, then reverse.
            if (this->value == 0) {
                return "0";
            }
            using U = std::conditional_t<std::same_as<T, i128>, u128, T>;
            std::string out;
            if constexpr (std::same_as<T, i128>) {
                T v = this->value;
                const bool neg = v < 0;
                U u = neg ? static_cast<U>(-(v + 1)) + 1 : static_cast<U>(v);
                while (u != 0) {
                    out.push_back(static_cast<char>('0' + (u % 10)));
                    u /= 10;
                }
                if (neg)
                    out.push_back('-');
            } else {
                U u = this->value;
                while (u != 0) {
                    out.push_back(static_cast<char>('0' + (u % 10)));
                    u /= 10;
                }
            }
            std::ranges::reverse(out);
            return out;
#endif
        } else {
            return std::to_string(this->value);
        }
    }

    /**
     * @brief Deserialize a `PrimitiveCell<T>` from JSON.
     * @param j Input JSON object — must match `{"k": kind_id, "v": <value>}`.
     * @return Newly built cell handle.
     * @throws std::runtime_error on shape or kind mismatch.
     */
    static cell_t from_json(json_t const& j, ParcelRegistry const&) {
        auto value = base_t::template cell_from_json<T>(j, trait_t::type_id);
        auto cell = std::make_shared<PrimitiveCell<T>>(std::move(value));
        base_t::absorb_meta(j, cell);
        return cell;
    }

    /**
     * @brief Cached descriptor for this primitive type.
     * @return Shared descriptor instance.
     */
    static cell_type_descriptor_t descriptor() {
        static const auto d =
            std::make_shared<SimpleCellTypeDescriptor<PrimitiveCell<T>>>(descriptor::MetaInfo{
                .name = std::string(trait_t::name),
                .description = std::string(trait_t::description),
            });

        return d;
    }
};

/** @brief `PrimitiveCell<char>`. */
using CharCell = PrimitiveCell<char>;
/** @brief `PrimitiveCell<bool>`. */
using BoolCell = PrimitiveCell<bool>;
/** @brief `PrimitiveCell<std::uint8_t>`. */
using U8Cell = PrimitiveCell<std::uint8_t>;
/** @brief `PrimitiveCell<std::uint16_t>`. */
using U16Cell = PrimitiveCell<std::uint16_t>;
/** @brief `PrimitiveCell<std::uint32_t>`. */
using U32Cell = PrimitiveCell<std::uint32_t>;
/** @brief `PrimitiveCell<std::uint64_t>`. */
using U64Cell = PrimitiveCell<std::uint64_t>;
/** @brief `PrimitiveCell<std::int8_t>`. */
using I8Cell = PrimitiveCell<std::int8_t>;
/** @brief `PrimitiveCell<std::int16_t>`. */
using I16Cell = PrimitiveCell<std::int16_t>;
/** @brief `PrimitiveCell<std::int32_t>`. */
using I32Cell = PrimitiveCell<std::int32_t>;
/** @brief `PrimitiveCell<std::int64_t>`. */
using I64Cell = PrimitiveCell<std::int64_t>;
/** @brief `PrimitiveCell<float>`. */
using FloatCell = PrimitiveCell<float>;
/** @brief `PrimitiveCell<double>`. */
using DoubleCell = PrimitiveCell<double>;
/** @brief `PrimitiveCell<std::string>`. */
using StringCell = PrimitiveCell<std::string>;
#ifdef PARCEL_HAS_INT128
/** @brief `PrimitiveCell<u128>` — value rendered as a decimal string. */
using U128Cell = PrimitiveCell<u128>;
/** @brief `PrimitiveCell<i128>` — value rendered as a decimal string. */
using I128Cell = PrimitiveCell<i128>;
#endif

}  // namespace parcel
