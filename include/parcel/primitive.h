#pragma once

/**
 * @file primitive.h
 * @brief `PrimitiveCell<T>` plus the per-storage `PrimitiveTraits<T>` specializations.
 *
 * Defines the leaf cell type for scalar values: `bool`, `char`, the commons
 * fixed-width integers (`comms::u8`-`comms::u64`, `comms::i8`-`comms::i64`,
 * plus `comms::u128` / `comms::i128` when supported), the floats
 * (`comms::f32` / `comms::f64`), the complex numbers (`comms::cs8`-`comms::cu64`,
 * `comms::cf32` / `comms::cf64`), and `std::string`. Each specialization of
 * `PrimitiveTraits<T>` carries the wire kind id (e.g. `"i32"`, `"cf64"`,
 * `"string"`) and a default human-readable name + description used by the
 * cell's descriptor.
 *
 * The numeric storage types come from `<commons/types.hpp>`; complex numbers
 * travel on the wire as a `[real, imaginary]` array via commons'
 * `nlohmann::adl_serializer<std::complex<T>>`.
 */

#include <parcel/cell.h>
#include <parcel/common.h>
#include <parcel/descriptor.h>

#include <commons/types.hpp>

#include <algorithm>
#include <complex>
#include <memory>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>

namespace parcel::primitives {

/**
 * @brief Detects `std::complex<U>` storage (used by `PrimitiveCell::to_string`).
 */
template <typename T>
struct is_std_complex : std::false_type {};
template <typename U>
struct is_std_complex<std::complex<U>> : std::true_type {};
template <typename T>
inline constexpr bool is_std_complex_v = is_std_complex<T>::value;

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

/** @brief Traits for `comms::u8` — wire kind `"u8"`. */
template <>
struct PrimitiveTraits<comms::u8> {
    static constexpr std::string_view type_id = "u8";
    static constexpr std::string_view name = "U8";
    static constexpr std::string_view description = "Unsigned 8-bit integer.";
};

/** @brief Traits for `comms::u16` — wire kind `"u16"`. */
template <>
struct PrimitiveTraits<comms::u16> {
    static constexpr std::string_view type_id = "u16";
    static constexpr std::string_view name = "U16";
    static constexpr std::string_view description = "Unsigned 16-bit integer.";
};

/** @brief Traits for `comms::u32` — wire kind `"u32"`. */
template <>
struct PrimitiveTraits<comms::u32> {
    static constexpr std::string_view type_id = "u32";
    static constexpr std::string_view name = "U32";
    static constexpr std::string_view description = "Unsigned 32-bit integer.";
};

/** @brief Traits for `comms::u64` — wire kind `"u64"`. */
template <>
struct PrimitiveTraits<comms::u64> {
    static constexpr std::string_view type_id = "u64";
    static constexpr std::string_view name = "U64";
    static constexpr std::string_view description = "Unsigned 64-bit integer.";
};

#ifdef COMMONS_HAS_INT128
/** @brief Traits for `comms::u128` — wire kind `"u128"`. Encoded as a decimal string. */
template <>
struct PrimitiveTraits<comms::u128> {
    static constexpr std::string_view type_id = "u128";
    static constexpr std::string_view name = "U128";
    static constexpr std::string_view description = "Unsigned 128-bit integer.";
};
#endif

/** @brief Traits for `comms::i8` — wire kind `"i8"`. */
template <>
struct PrimitiveTraits<comms::i8> {
    static constexpr std::string_view type_id = "i8";
    static constexpr std::string_view name = "I8";
    static constexpr std::string_view description = "Signed 8-bit integer.";
};

/** @brief Traits for `comms::i16` — wire kind `"i16"`. */
template <>
struct PrimitiveTraits<comms::i16> {
    static constexpr std::string_view type_id = "i16";
    static constexpr std::string_view name = "I16";
    static constexpr std::string_view description = "Signed 16-bit integer.";
};

/** @brief Traits for `comms::i32` — wire kind `"i32"`. */
template <>
struct PrimitiveTraits<comms::i32> {
    static constexpr std::string_view type_id = "i32";
    static constexpr std::string_view name = "I32";
    static constexpr std::string_view description = "Signed 32-bit integer.";
};

/** @brief Traits for `comms::i64` — wire kind `"i64"`. */
template <>
struct PrimitiveTraits<comms::i64> {
    static constexpr std::string_view type_id = "i64";
    static constexpr std::string_view name = "I64";
    static constexpr std::string_view description = "Signed 64-bit integer.";
};

#ifdef COMMONS_HAS_INT128
/** @brief Traits for `comms::i128` — wire kind `"i128"`. Encoded as a decimal string. */
template <>
struct PrimitiveTraits<comms::i128> {
    static constexpr std::string_view type_id = "i128";
    static constexpr std::string_view name = "I128";
    static constexpr std::string_view description = "Signed 128-bit integer.";
};
#endif

/** @brief Traits for `comms::f32` — wire kind `"f32"`. */
template <>
struct PrimitiveTraits<comms::f32> {
    static constexpr std::string_view type_id = "f32";
    static constexpr std::string_view name = "F32";
    static constexpr std::string_view description = "32-bit floating-point number.";
};

/** @brief Traits for `comms::f64` — wire kind `"f64"`. */
template <>
struct PrimitiveTraits<comms::f64> {
    static constexpr std::string_view type_id = "f64";
    static constexpr std::string_view name = "F64";
    static constexpr std::string_view description = "64-bit floating-point number.";
};

// Complex numbers. Wire form is a `[real, imaginary]` JSON array (commons'
// adl_serializer<std::complex<T>>); the component type serializes natively.

/** @brief Traits for `comms::cs8` — wire kind `"cs8"`. */
template <>
struct PrimitiveTraits<comms::cs8> {
    static constexpr std::string_view type_id = "cs8";
    static constexpr std::string_view name = "CS8";
    static constexpr std::string_view description = "Complex with signed 8-bit components.";
};

/** @brief Traits for `comms::cs16` — wire kind `"cs16"`. */
template <>
struct PrimitiveTraits<comms::cs16> {
    static constexpr std::string_view type_id = "cs16";
    static constexpr std::string_view name = "CS16";
    static constexpr std::string_view description = "Complex with signed 16-bit components.";
};

/** @brief Traits for `comms::cs32` — wire kind `"cs32"`. */
template <>
struct PrimitiveTraits<comms::cs32> {
    static constexpr std::string_view type_id = "cs32";
    static constexpr std::string_view name = "CS32";
    static constexpr std::string_view description = "Complex with signed 32-bit components.";
};

/** @brief Traits for `comms::cs64` — wire kind `"cs64"`. */
template <>
struct PrimitiveTraits<comms::cs64> {
    static constexpr std::string_view type_id = "cs64";
    static constexpr std::string_view name = "CS64";
    static constexpr std::string_view description = "Complex with signed 64-bit components.";
};

/** @brief Traits for `comms::cu8` — wire kind `"cu8"`. */
template <>
struct PrimitiveTraits<comms::cu8> {
    static constexpr std::string_view type_id = "cu8";
    static constexpr std::string_view name = "CU8";
    static constexpr std::string_view description = "Complex with unsigned 8-bit components.";
};

/** @brief Traits for `comms::cu16` — wire kind `"cu16"`. */
template <>
struct PrimitiveTraits<comms::cu16> {
    static constexpr std::string_view type_id = "cu16";
    static constexpr std::string_view name = "CU16";
    static constexpr std::string_view description = "Complex with unsigned 16-bit components.";
};

/** @brief Traits for `comms::cu32` — wire kind `"cu32"`. */
template <>
struct PrimitiveTraits<comms::cu32> {
    static constexpr std::string_view type_id = "cu32";
    static constexpr std::string_view name = "CU32";
    static constexpr std::string_view description = "Complex with unsigned 32-bit components.";
};

/** @brief Traits for `comms::cu64` — wire kind `"cu64"`. */
template <>
struct PrimitiveTraits<comms::cu64> {
    static constexpr std::string_view type_id = "cu64";
    static constexpr std::string_view name = "CU64";
    static constexpr std::string_view description = "Complex with unsigned 64-bit components.";
};

/** @brief Traits for `comms::cf32` — wire kind `"cf32"`. */
template <>
struct PrimitiveTraits<comms::cf32> {
    static constexpr std::string_view type_id = "cf32";
    static constexpr std::string_view name = "CF32";
    static constexpr std::string_view description =
        "Complex with 32-bit floating-point components.";
};

/** @brief Traits for `comms::cf64` — wire kind `"cf64"`. */
template <>
struct PrimitiveTraits<comms::cf64> {
    static constexpr std::string_view type_id = "cf64";
    static constexpr std::string_view name = "CF64";
    static constexpr std::string_view description =
        "Complex with 64-bit floating-point components.";
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
 * Covers `char`, `bool`, the commons fixed-width integers (`comms::u8`-`u64`,
 * `comms::i8`-`i64`, plus 128-bit on supporting toolchains), the floats
 * (`comms::f32` / `comms::f64`), the complex numbers (`comms::cs8`-`cu64`,
 * `comms::cf32` / `comms::cf64`), and `std::string`.
 *
 * @tparam T Candidate storage type.
 */
template <typename T>
concept PrimitiveStorage =
    std::same_as<T, char> || std::same_as<T, bool> || std::same_as<T, comms::u8> ||
    std::same_as<T, comms::u16> || std::same_as<T, comms::u32> || std::same_as<T, comms::u64> ||
    std::same_as<T, comms::i8> || std::same_as<T, comms::i16> || std::same_as<T, comms::i32> ||
    std::same_as<T, comms::i64> || std::same_as<T, comms::f32> || std::same_as<T, comms::f64> ||
#ifdef COMMONS_HAS_INT128
    std::same_as<T, comms::u128> || std::same_as<T, comms::i128> ||
#endif
    std::same_as<T, comms::cs8> || std::same_as<T, comms::cs16> || std::same_as<T, comms::cs32> ||
    std::same_as<T, comms::cs64> || std::same_as<T, comms::cu8> || std::same_as<T, comms::cu16> ||
    std::same_as<T, comms::cu32> || std::same_as<T, comms::cu64> || std::same_as<T, comms::cf32> ||
    std::same_as<T, comms::cf64> || std::same_as<T, std::string>;

/**
 * @brief Leaf cell wrapping a single scalar (or complex) value of type @p T.
 *
 * Wire shape:
 * @code{.json}
 * {"k": "<type_id>", "v": <value>}
 * @endcode
 * with `<type_id>` taken from `PrimitiveTraits<T>::type_id` (e.g. `"i32"`,
 * `"string"`). 128-bit integers serialize their `"v"` as a decimal string and
 * complex numbers as a `[real, imaginary]` array, via the commons ADL
 * serializers pulled in through `json.h`.
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
     * `char` becomes a 1-byte string, 128-bit integers are formatted by hand
     * (since `std::to_string` has no overload), complex numbers render as
     * `"(real, imaginary)"`, and everything else routes through
     * `std::to_string`.
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
#ifdef COMMONS_HAS_INT128
        } else if constexpr (std::same_as<T, comms::u128> || std::same_as<T, comms::i128>) {
            // std::to_string has no overload for __int128_t / __uint128_t.
            // Build the digits manually, LSB-first, then reverse.
            if (this->value == 0) {
                return "0";
            }
            using U = std::conditional_t<std::same_as<T, comms::i128>, comms::u128, T>;
            std::string out;
            if constexpr (std::same_as<T, comms::i128>) {
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
        } else if constexpr (primitives::is_std_complex_v<T>) {
            return "(" + std::to_string(this->value.real()) + ", " +
                   std::to_string(this->value.imag()) + ")";
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
            std::make_shared<SimpleCellTypeDescriptor<PrimitiveCell<T>>>(DisplayInfo{
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
/** @brief `PrimitiveCell<comms::u8>`. */
using U8Cell = PrimitiveCell<comms::u8>;
/** @brief `PrimitiveCell<comms::u16>`. */
using U16Cell = PrimitiveCell<comms::u16>;
/** @brief `PrimitiveCell<comms::u32>`. */
using U32Cell = PrimitiveCell<comms::u32>;
/** @brief `PrimitiveCell<comms::u64>`. */
using U64Cell = PrimitiveCell<comms::u64>;
/** @brief `PrimitiveCell<comms::i8>`. */
using I8Cell = PrimitiveCell<comms::i8>;
/** @brief `PrimitiveCell<comms::i16>`. */
using I16Cell = PrimitiveCell<comms::i16>;
/** @brief `PrimitiveCell<comms::i32>`. */
using I32Cell = PrimitiveCell<comms::i32>;
/** @brief `PrimitiveCell<comms::i64>`. */
using I64Cell = PrimitiveCell<comms::i64>;
/** @brief `PrimitiveCell<comms::f32>`. */
using FloatCell = PrimitiveCell<comms::f32>;
/** @brief `PrimitiveCell<comms::f64>`. */
using DoubleCell = PrimitiveCell<comms::f64>;
/** @brief `PrimitiveCell<comms::cs8>`. */
using Cs8Cell = PrimitiveCell<comms::cs8>;
/** @brief `PrimitiveCell<comms::cs16>`. */
using Cs16Cell = PrimitiveCell<comms::cs16>;
/** @brief `PrimitiveCell<comms::cs32>`. */
using Cs32Cell = PrimitiveCell<comms::cs32>;
/** @brief `PrimitiveCell<comms::cs64>`. */
using Cs64Cell = PrimitiveCell<comms::cs64>;
/** @brief `PrimitiveCell<comms::cu8>`. */
using Cu8Cell = PrimitiveCell<comms::cu8>;
/** @brief `PrimitiveCell<comms::cu16>`. */
using Cu16Cell = PrimitiveCell<comms::cu16>;
/** @brief `PrimitiveCell<comms::cu32>`. */
using Cu32Cell = PrimitiveCell<comms::cu32>;
/** @brief `PrimitiveCell<comms::cu64>`. */
using Cu64Cell = PrimitiveCell<comms::cu64>;
/** @brief `PrimitiveCell<comms::cf32>`. */
using Cf32Cell = PrimitiveCell<comms::cf32>;
/** @brief `PrimitiveCell<comms::cf64>`. */
using Cf64Cell = PrimitiveCell<comms::cf64>;
/** @brief `PrimitiveCell<std::string>`. */
using StringCell = PrimitiveCell<std::string>;
#ifdef COMMONS_HAS_INT128
/** @brief `PrimitiveCell<comms::u128>` — value rendered as a decimal string. */
using U128Cell = PrimitiveCell<comms::u128>;
/** @brief `PrimitiveCell<comms::i128>` — value rendered as a decimal string. */
using I128Cell = PrimitiveCell<comms::i128>;
#endif

}  // namespace parcel
