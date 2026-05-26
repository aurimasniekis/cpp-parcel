#pragma once

/**
 * @file builtins.h
 * @brief `register_primitives` / `register_collections` / `register_typed_collections` /
 *        `register_std` helpers.
 *
 * Convenience aliases for `TypedListCell<P>` / `TypedMapCell<P>` /
 * `TypedHashMapCell<P>` of every primitive `P`, plus the registration
 * helpers `ParcelRegistry`'s default constructor uses to populate the
 * registry with the standard built-in kinds. Each instantiation has a
 * distinct kind id (e.g. `"l:i32"`, `"m:string"`, `"hm:i32"`), so the
 * registry can dispatch any typed primitive collection without ambiguity.
 *
 * `register_std` registers the std-adapter cells from
 * `parcel/ext/chrono.h` and `parcel/ext/filesystem.h` so a default-
 * constructed registry handles `std::chrono` and `std::filesystem` payloads
 * out of the box.
 *
 * Once the registry is populated the aliases let you write:
 * @code{.cpp}
 * parcel::I32ListCell    nums{1, 2, 3};
 * parcel::StringMapCell  tags{{"role", "admin"}};
 * @endcode
 */

#include <parcel/commons.h>
#include <parcel/ext/chrono.h>
#include <parcel/ext/filesystem.h>
#include <parcel/list.h>
#include <parcel/map.h>
#include <parcel/primitive.h>
#include <parcel/registry.h>
#include <parcel/unordered_map.h>

namespace parcel {

/** @brief `TypedListCell<CharCell>`. */
using CharListCell = TypedListCell<CharCell>;
/** @brief `TypedListCell<BoolCell>`. */
using BoolListCell = TypedListCell<BoolCell>;
/** @brief `TypedListCell<U8Cell>`. */
using U8ListCell = TypedListCell<U8Cell>;
/** @brief `TypedListCell<U16Cell>`. */
using U16ListCell = TypedListCell<U16Cell>;
/** @brief `TypedListCell<U32Cell>`. */
using U32ListCell = TypedListCell<U32Cell>;
/** @brief `TypedListCell<U64Cell>`. */
using U64ListCell = TypedListCell<U64Cell>;
/** @brief `TypedListCell<I8Cell>`. */
using I8ListCell = TypedListCell<I8Cell>;
/** @brief `TypedListCell<I16Cell>`. */
using I16ListCell = TypedListCell<I16Cell>;
/** @brief `TypedListCell<I32Cell>`. */
using I32ListCell = TypedListCell<I32Cell>;
/** @brief `TypedListCell<I64Cell>`. */
using I64ListCell = TypedListCell<I64Cell>;
/** @brief `TypedListCell<FloatCell>`. */
using FloatListCell = TypedListCell<FloatCell>;
/** @brief `TypedListCell<DoubleCell>`. */
using DoubleListCell = TypedListCell<DoubleCell>;
/** @brief `TypedListCell<StringCell>`. */
using StringListCell = TypedListCell<StringCell>;
#ifdef COMMONS_HAS_INT128
/** @brief `TypedListCell<U128Cell>`. */
using U128ListCell = TypedListCell<U128Cell>;
/** @brief `TypedListCell<I128Cell>`. */
using I128ListCell = TypedListCell<I128Cell>;
#endif

/** @brief `TypedMapCell<CharCell>`. */
using CharMapCell = TypedMapCell<CharCell>;
/** @brief `TypedMapCell<BoolCell>`. */
using BoolMapCell = TypedMapCell<BoolCell>;
/** @brief `TypedMapCell<U8Cell>`. */
using U8MapCell = TypedMapCell<U8Cell>;
/** @brief `TypedMapCell<U16Cell>`. */
using U16MapCell = TypedMapCell<U16Cell>;
/** @brief `TypedMapCell<U32Cell>`. */
using U32MapCell = TypedMapCell<U32Cell>;
/** @brief `TypedMapCell<U64Cell>`. */
using U64MapCell = TypedMapCell<U64Cell>;
/** @brief `TypedMapCell<I8Cell>`. */
using I8MapCell = TypedMapCell<I8Cell>;
/** @brief `TypedMapCell<I16Cell>`. */
using I16MapCell = TypedMapCell<I16Cell>;
/** @brief `TypedMapCell<I32Cell>`. */
using I32MapCell = TypedMapCell<I32Cell>;
/** @brief `TypedMapCell<I64Cell>`. */
using I64MapCell = TypedMapCell<I64Cell>;
/** @brief `TypedMapCell<FloatCell>`. */
using FloatMapCell = TypedMapCell<FloatCell>;
/** @brief `TypedMapCell<DoubleCell>`. */
using DoubleMapCell = TypedMapCell<DoubleCell>;
/** @brief `TypedMapCell<StringCell>`. */
using StringMapCell = TypedMapCell<StringCell>;
#ifdef COMMONS_HAS_INT128
/** @brief `TypedMapCell<U128Cell>`. */
using U128MapCell = TypedMapCell<U128Cell>;
/** @brief `TypedMapCell<I128Cell>`. */
using I128MapCell = TypedMapCell<I128Cell>;
#endif

/** @brief `TypedHashMapCell<CharCell>`. */
using CharHashMapCell = TypedHashMapCell<CharCell>;
/** @brief `TypedHashMapCell<BoolCell>`. */
using BoolHashMapCell = TypedHashMapCell<BoolCell>;
/** @brief `TypedHashMapCell<U8Cell>`. */
using U8HashMapCell = TypedHashMapCell<U8Cell>;
/** @brief `TypedHashMapCell<U16Cell>`. */
using U16HashMapCell = TypedHashMapCell<U16Cell>;
/** @brief `TypedHashMapCell<U32Cell>`. */
using U32HashMapCell = TypedHashMapCell<U32Cell>;
/** @brief `TypedHashMapCell<U64Cell>`. */
using U64HashMapCell = TypedHashMapCell<U64Cell>;
/** @brief `TypedHashMapCell<I8Cell>`. */
using I8HashMapCell = TypedHashMapCell<I8Cell>;
/** @brief `TypedHashMapCell<I16Cell>`. */
using I16HashMapCell = TypedHashMapCell<I16Cell>;
/** @brief `TypedHashMapCell<I32Cell>`. */
using I32HashMapCell = TypedHashMapCell<I32Cell>;
/** @brief `TypedHashMapCell<I64Cell>`. */
using I64HashMapCell = TypedHashMapCell<I64Cell>;
/** @brief `TypedHashMapCell<FloatCell>`. */
using FloatHashMapCell = TypedHashMapCell<FloatCell>;
/** @brief `TypedHashMapCell<DoubleCell>`. */
using DoubleHashMapCell = TypedHashMapCell<DoubleCell>;
/** @brief `TypedHashMapCell<StringCell>`. */
using StringHashMapCell = TypedHashMapCell<StringCell>;
#ifdef COMMONS_HAS_INT128
/** @brief `TypedHashMapCell<U128Cell>`. */
using U128HashMapCell = TypedHashMapCell<U128Cell>;
/** @brief `TypedHashMapCell<I128Cell>`. */
using I128HashMapCell = TypedHashMapCell<I128Cell>;
#endif

// Complex typed collections (wire kinds e.g. "l:cf64", "m:cs8", "hm:cu32").

/** @brief `TypedListCell<Cs8Cell>`. */
using Cs8ListCell = TypedListCell<Cs8Cell>;
/** @brief `TypedListCell<Cs16Cell>`. */
using Cs16ListCell = TypedListCell<Cs16Cell>;
/** @brief `TypedListCell<Cs32Cell>`. */
using Cs32ListCell = TypedListCell<Cs32Cell>;
/** @brief `TypedListCell<Cs64Cell>`. */
using Cs64ListCell = TypedListCell<Cs64Cell>;
/** @brief `TypedListCell<Cu8Cell>`. */
using Cu8ListCell = TypedListCell<Cu8Cell>;
/** @brief `TypedListCell<Cu16Cell>`. */
using Cu16ListCell = TypedListCell<Cu16Cell>;
/** @brief `TypedListCell<Cu32Cell>`. */
using Cu32ListCell = TypedListCell<Cu32Cell>;
/** @brief `TypedListCell<Cu64Cell>`. */
using Cu64ListCell = TypedListCell<Cu64Cell>;
/** @brief `TypedListCell<Cf32Cell>`. */
using Cf32ListCell = TypedListCell<Cf32Cell>;
/** @brief `TypedListCell<Cf64Cell>`. */
using Cf64ListCell = TypedListCell<Cf64Cell>;

/** @brief `TypedMapCell<Cs8Cell>`. */
using Cs8MapCell = TypedMapCell<Cs8Cell>;
/** @brief `TypedMapCell<Cs16Cell>`. */
using Cs16MapCell = TypedMapCell<Cs16Cell>;
/** @brief `TypedMapCell<Cs32Cell>`. */
using Cs32MapCell = TypedMapCell<Cs32Cell>;
/** @brief `TypedMapCell<Cs64Cell>`. */
using Cs64MapCell = TypedMapCell<Cs64Cell>;
/** @brief `TypedMapCell<Cu8Cell>`. */
using Cu8MapCell = TypedMapCell<Cu8Cell>;
/** @brief `TypedMapCell<Cu16Cell>`. */
using Cu16MapCell = TypedMapCell<Cu16Cell>;
/** @brief `TypedMapCell<Cu32Cell>`. */
using Cu32MapCell = TypedMapCell<Cu32Cell>;
/** @brief `TypedMapCell<Cu64Cell>`. */
using Cu64MapCell = TypedMapCell<Cu64Cell>;
/** @brief `TypedMapCell<Cf32Cell>`. */
using Cf32MapCell = TypedMapCell<Cf32Cell>;
/** @brief `TypedMapCell<Cf64Cell>`. */
using Cf64MapCell = TypedMapCell<Cf64Cell>;

/** @brief `TypedHashMapCell<Cs8Cell>`. */
using Cs8HashMapCell = TypedHashMapCell<Cs8Cell>;
/** @brief `TypedHashMapCell<Cs16Cell>`. */
using Cs16HashMapCell = TypedHashMapCell<Cs16Cell>;
/** @brief `TypedHashMapCell<Cs32Cell>`. */
using Cs32HashMapCell = TypedHashMapCell<Cs32Cell>;
/** @brief `TypedHashMapCell<Cs64Cell>`. */
using Cs64HashMapCell = TypedHashMapCell<Cs64Cell>;
/** @brief `TypedHashMapCell<Cu8Cell>`. */
using Cu8HashMapCell = TypedHashMapCell<Cu8Cell>;
/** @brief `TypedHashMapCell<Cu16Cell>`. */
using Cu16HashMapCell = TypedHashMapCell<Cu16Cell>;
/** @brief `TypedHashMapCell<Cu32Cell>`. */
using Cu32HashMapCell = TypedHashMapCell<Cu32Cell>;
/** @brief `TypedHashMapCell<Cu64Cell>`. */
using Cu64HashMapCell = TypedHashMapCell<Cu64Cell>;
/** @brief `TypedHashMapCell<Cf32Cell>`. */
using Cf32HashMapCell = TypedHashMapCell<Cf32Cell>;
/** @brief `TypedHashMapCell<Cf64Cell>`. */
using Cf64HashMapCell = TypedHashMapCell<Cf64Cell>;

/**
 * @brief Register every `PrimitiveCell<T>` into @p reg.
 * @param reg Registry to populate.
 */
inline void register_primitives(ParcelRegistry& reg) {
    reg.register_kind(CharCell::descriptor());
    reg.register_kind(BoolCell::descriptor());
    reg.register_kind(U8Cell::descriptor());
    reg.register_kind(U16Cell::descriptor());
    reg.register_kind(U32Cell::descriptor());
    reg.register_kind(U64Cell::descriptor());
    reg.register_kind(I8Cell::descriptor());
    reg.register_kind(I16Cell::descriptor());
    reg.register_kind(I32Cell::descriptor());
    reg.register_kind(I64Cell::descriptor());
    reg.register_kind(FloatCell::descriptor());
    reg.register_kind(DoubleCell::descriptor());
    reg.register_kind(Cs8Cell::descriptor());
    reg.register_kind(Cs16Cell::descriptor());
    reg.register_kind(Cs32Cell::descriptor());
    reg.register_kind(Cs64Cell::descriptor());
    reg.register_kind(Cu8Cell::descriptor());
    reg.register_kind(Cu16Cell::descriptor());
    reg.register_kind(Cu32Cell::descriptor());
    reg.register_kind(Cu64Cell::descriptor());
    reg.register_kind(Cf32Cell::descriptor());
    reg.register_kind(Cf64Cell::descriptor());
    reg.register_kind(StringCell::descriptor());
#ifdef COMMONS_HAS_INT128
    reg.register_kind(U128Cell::descriptor());
    reg.register_kind(I128Cell::descriptor());
#endif
}

/**
 * @brief Register the heterogeneous `ListCell`, `MapCell`, and `HashMapCell` into @p reg.
 * @param reg Registry to populate.
 */
inline void register_collections(ParcelRegistry& reg) {
    reg.register_kind(ListCell::descriptor());
    reg.register_kind(MapCell::descriptor());
    reg.register_kind(HashMapCell::descriptor());
}

/**
 * @brief Register `TypedListCell<P>`, `TypedMapCell<P>`, and `TypedHashMapCell<P>`
 *        for every primitive `P`.
 *
 * Each instantiation has a distinct kind id (e.g. `"l:i32"`,
 * `"m:string"`, `"hm:i32"`), so the registry can dispatch any typed
 * primitive collection without ambiguity.
 *
 * @param reg Registry to populate.
 */
inline void register_typed_collections(ParcelRegistry& reg) {
    reg.register_kind(CharListCell::descriptor());
    reg.register_kind(BoolListCell::descriptor());
    reg.register_kind(U8ListCell::descriptor());
    reg.register_kind(U16ListCell::descriptor());
    reg.register_kind(U32ListCell::descriptor());
    reg.register_kind(U64ListCell::descriptor());
    reg.register_kind(I8ListCell::descriptor());
    reg.register_kind(I16ListCell::descriptor());
    reg.register_kind(I32ListCell::descriptor());
    reg.register_kind(I64ListCell::descriptor());
    reg.register_kind(FloatListCell::descriptor());
    reg.register_kind(DoubleListCell::descriptor());
    reg.register_kind(StringListCell::descriptor());
#ifdef COMMONS_HAS_INT128
    reg.register_kind(U128ListCell::descriptor());
    reg.register_kind(I128ListCell::descriptor());
#endif
    reg.register_kind(Cs8ListCell::descriptor());
    reg.register_kind(Cs16ListCell::descriptor());
    reg.register_kind(Cs32ListCell::descriptor());
    reg.register_kind(Cs64ListCell::descriptor());
    reg.register_kind(Cu8ListCell::descriptor());
    reg.register_kind(Cu16ListCell::descriptor());
    reg.register_kind(Cu32ListCell::descriptor());
    reg.register_kind(Cu64ListCell::descriptor());
    reg.register_kind(Cf32ListCell::descriptor());
    reg.register_kind(Cf64ListCell::descriptor());

    reg.register_kind(CharMapCell::descriptor());
    reg.register_kind(BoolMapCell::descriptor());
    reg.register_kind(U8MapCell::descriptor());
    reg.register_kind(U16MapCell::descriptor());
    reg.register_kind(U32MapCell::descriptor());
    reg.register_kind(U64MapCell::descriptor());
    reg.register_kind(I8MapCell::descriptor());
    reg.register_kind(I16MapCell::descriptor());
    reg.register_kind(I32MapCell::descriptor());
    reg.register_kind(I64MapCell::descriptor());
    reg.register_kind(FloatMapCell::descriptor());
    reg.register_kind(DoubleMapCell::descriptor());
    reg.register_kind(StringMapCell::descriptor());
#ifdef COMMONS_HAS_INT128
    reg.register_kind(U128MapCell::descriptor());
    reg.register_kind(I128MapCell::descriptor());
#endif
    reg.register_kind(Cs8MapCell::descriptor());
    reg.register_kind(Cs16MapCell::descriptor());
    reg.register_kind(Cs32MapCell::descriptor());
    reg.register_kind(Cs64MapCell::descriptor());
    reg.register_kind(Cu8MapCell::descriptor());
    reg.register_kind(Cu16MapCell::descriptor());
    reg.register_kind(Cu32MapCell::descriptor());
    reg.register_kind(Cu64MapCell::descriptor());
    reg.register_kind(Cf32MapCell::descriptor());
    reg.register_kind(Cf64MapCell::descriptor());

    reg.register_kind(CharHashMapCell::descriptor());
    reg.register_kind(BoolHashMapCell::descriptor());
    reg.register_kind(U8HashMapCell::descriptor());
    reg.register_kind(U16HashMapCell::descriptor());
    reg.register_kind(U32HashMapCell::descriptor());
    reg.register_kind(U64HashMapCell::descriptor());
    reg.register_kind(I8HashMapCell::descriptor());
    reg.register_kind(I16HashMapCell::descriptor());
    reg.register_kind(I32HashMapCell::descriptor());
    reg.register_kind(I64HashMapCell::descriptor());
    reg.register_kind(FloatHashMapCell::descriptor());
    reg.register_kind(DoubleHashMapCell::descriptor());
    reg.register_kind(StringHashMapCell::descriptor());
#ifdef COMMONS_HAS_INT128
    reg.register_kind(U128HashMapCell::descriptor());
    reg.register_kind(I128HashMapCell::descriptor());
#endif
    reg.register_kind(Cs8HashMapCell::descriptor());
    reg.register_kind(Cs16HashMapCell::descriptor());
    reg.register_kind(Cs32HashMapCell::descriptor());
    reg.register_kind(Cs64HashMapCell::descriptor());
    reg.register_kind(Cu8HashMapCell::descriptor());
    reg.register_kind(Cu16HashMapCell::descriptor());
    reg.register_kind(Cu32HashMapCell::descriptor());
    reg.register_kind(Cu64HashMapCell::descriptor());
    reg.register_kind(Cf32HashMapCell::descriptor());
    reg.register_kind(Cf64HashMapCell::descriptor());
}

/**
 * @brief Register the std-adapter cells into @p reg.
 *
 * Registers `SystemTimePointCell`, `UnixMillisCell`, `DurationMsCell`, and
 * `YmdCell` from `parcel/ext/chrono.h`, and `PathCell` from
 * `parcel/ext/filesystem.h`. (`TimestampMsCell` is an alias of
 * `UnixMillisCell` and shares its kind id.)
 *
 * @param reg Registry to populate.
 */
inline void register_std(ParcelRegistry& reg) {
    reg.register_kind(SystemTimePointCell::descriptor());
    reg.register_kind(UnixMillisCell::descriptor());
    reg.register_kind(DurationMsCell::descriptor());
    reg.register_kind(YmdCell::descriptor());
    reg.register_kind(PathCell::descriptor());
}

/**
 * @brief Register the commons-adapter cells into @p reg.
 *
 * Registers `ColorCell`, `IconCell`, `DisplayInfoCell`, `FlagCell`, and
 * `FlagSetCell` from `parcel/commons.h` so a default-constructed registry
 * dispatches the `aurimasniekis/cpp-commons` vocabulary types out of the box.
 *
 * @param reg Registry to populate.
 */
inline void register_commons(ParcelRegistry& reg) {
    reg.register_kind(ColorCell::descriptor());
    reg.register_kind(IconCell::descriptor());
    reg.register_kind(DisplayInfoCell::descriptor());
    reg.register_kind(FlagCell::descriptor());
    reg.register_kind(FlagSetCell::descriptor());
}

/**
 * @brief Apply the @p opts toggles to populate @p reg with built-in cells.
 * @param reg  Registry to populate.
 * @param opts Toggles for which built-ins to register.
 */
inline void register_builtins(ParcelRegistry& reg, const BuiltinsOptions opts = {}) {
    if (opts.primitives) {
        register_primitives(reg);
    }
    if (opts.collections) {
        register_collections(reg);
    }
    if (opts.typed_collections) {
        register_typed_collections(reg);
    }
    if (opts.std) {
        register_std(reg);
    }
    if (opts.commons) {
        register_commons(reg);
    }
}

// Defined here (rather than in registry.h) so the registry header doesn't
// pull in every primitive / collection cell. The declaration lives on
// ParcelRegistry; this is just the inline body.
inline ParcelRegistry::ParcelRegistry(const BuiltinsOptions opts) {
    register_builtins(*this, opts);
}

}  // namespace parcel
