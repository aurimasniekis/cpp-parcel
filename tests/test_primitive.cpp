#include <parcel/parcel.h>

#include <gtest/gtest.h>

#include <memory>
#include <string>

namespace {

// Stand-in registry — primitive deserialization doesn't consult it, so a
// default-constructed instance is enough for round-trip tests.
parcel::ParcelRegistry const& registry() {
    static const parcel::ParcelRegistry r;
    return r;
}

}  // namespace

// ---------------------------------------------------------------------------
// Concept gate

static_assert(parcel::CellLike<parcel::I32Cell>);
static_assert(parcel::CellLike<parcel::StringCell>);
static_assert(parcel::CellLike<parcel::BoolCell>);
static_assert(parcel::CellLike<parcel::DoubleCell>);

// ---------------------------------------------------------------------------
// Construction + value access

TEST(PrimitiveCell, default_constructs_to_zero_value) {
    parcel::I32Cell v;
    EXPECT_EQ(v.get(), 0);
}

TEST(PrimitiveCell, constructs_from_underlying_value) {
    parcel::I32Cell v{42};
    EXPECT_EQ(v.get(), 42);
    EXPECT_EQ(v.value, 42);
}

TEST(PrimitiveCell, constructs_string_value) {
    parcel::StringCell v{std::string("hello")};
    EXPECT_EQ(v.get(), "hello");
}

TEST(PrimitiveCell, assignment_operator_returns_derived) {
    parcel::I32Cell v;
    v = 7;
    EXPECT_EQ(v.get(), 7);
}

// ---------------------------------------------------------------------------
// kind()

TEST(PrimitiveCell, kind_matches_trait_type_id) {
    EXPECT_EQ(parcel::I32Cell{}.kind(), "i32");
    EXPECT_EQ(parcel::U64Cell{}.kind(), "u64");
    EXPECT_EQ(parcel::DoubleCell{}.kind(), "f64");
    EXPECT_EQ(parcel::FloatCell{}.kind(), "f32");
    EXPECT_EQ(parcel::BoolCell{}.kind(), "bool");
    EXPECT_EQ(parcel::CharCell{}.kind(), "char");
    EXPECT_EQ(parcel::StringCell{}.kind(), "string");
}

// ---------------------------------------------------------------------------
// to_string()

TEST(PrimitiveCell, to_string_for_arithmetic_types) {
    EXPECT_EQ(parcel::I32Cell{42}.to_string(), "42");
    EXPECT_EQ(parcel::I64Cell{-9}.to_string(), "-9");
    EXPECT_EQ(parcel::U32Cell{17u}.to_string(), "17");
    EXPECT_EQ(parcel::U64Cell{99u}.to_string(), "99");
}

TEST(PrimitiveCell, to_string_for_floats_uses_std_to_string) {
    // std::to_string on float/double produces a fixed-precision form;
    // we don't pin the exact text — just verify the integral prefix is present.
    EXPECT_EQ(parcel::FloatCell{3.5F}.to_string().substr(0, 3), "3.5");
    EXPECT_EQ(parcel::DoubleCell{2.5}.to_string().substr(0, 3), "2.5");
}

TEST(PrimitiveCell, to_string_for_bool) {
    EXPECT_EQ(parcel::BoolCell{true}.to_string(), "true");
    EXPECT_EQ(parcel::BoolCell{false}.to_string(), "false");
}

TEST(PrimitiveCell, to_string_for_char_returns_character) {
    EXPECT_EQ(parcel::CharCell{'A'}.to_string(), "A");
}

TEST(PrimitiveCell, to_string_for_string_returns_value) {
    EXPECT_EQ(parcel::StringCell{std::string("hello")}.to_string(), "hello");
}

// ---------------------------------------------------------------------------
// to_json() / from_json round-trip

TEST(PrimitiveCell, to_json_emits_kind_and_value_keys) {
    const parcel::I32Cell v{42};
    const auto j = v.to_json();
    ASSERT_TRUE(j.is_object());
    EXPECT_EQ(j["k"].get<std::string>(), "i32");
    EXPECT_EQ(j["v"].get<int>(), 42);
}

TEST(PrimitiveCell, from_json_round_trip_int) {
    const parcel::I32Cell original{123};
    const auto j = original.to_json();
    const auto restored = parcel::I32Cell::from_json(j, registry());
    ASSERT_NE(restored, nullptr);
    EXPECT_EQ(restored->kind(), "i32");
    auto* typed = dynamic_cast<parcel::I32Cell*>(restored.get());
    ASSERT_NE(typed, nullptr);
    EXPECT_EQ(typed->get(), 123);
}

TEST(PrimitiveCell, from_json_round_trip_string) {
    const parcel::StringCell original{std::string("wire-friendly")};
    const auto j = original.to_json();
    const auto restored = parcel::StringCell::from_json(j, registry());
    auto* typed = dynamic_cast<parcel::StringCell*>(restored.get());
    ASSERT_NE(typed, nullptr);
    EXPECT_EQ(typed->get(), "wire-friendly");
}

TEST(PrimitiveCell, from_json_round_trip_i64) {
    const parcel::I64Cell original{-9'000'000'000LL};
    const auto j = original.to_json();
    const auto restored = parcel::I64Cell::from_json(j, registry());
    auto* typed = dynamic_cast<parcel::I64Cell*>(restored.get());
    ASSERT_NE(typed, nullptr);
    EXPECT_EQ(typed->get(), -9'000'000'000LL);
}

TEST(PrimitiveCell, from_json_round_trip_double) {
    const parcel::DoubleCell original{3.5};
    const auto j = original.to_json();
    const auto restored = parcel::DoubleCell::from_json(j, registry());
    auto* typed = dynamic_cast<parcel::DoubleCell*>(restored.get());
    ASSERT_NE(typed, nullptr);
    EXPECT_DOUBLE_EQ(typed->get(), 3.5);
}

TEST(PrimitiveCell, from_json_rejects_wrong_kind) {
    parcel::json_t mismatched = {{"k", "f64"}, {"v", 42}};
    EXPECT_THROW(parcel::I32Cell::from_json(mismatched, registry()), std::runtime_error);
}

TEST(PrimitiveCell, from_json_rejects_missing_kind) {
    parcel::json_t no_kind = {{"v", 42}};
    EXPECT_THROW(parcel::I32Cell::from_json(no_kind, registry()), std::runtime_error);
}

TEST(PrimitiveCell, from_json_rejects_missing_value) {
    parcel::json_t no_value = {{"k", "i32"}};
    EXPECT_THROW(parcel::I32Cell::from_json(no_value, registry()), std::runtime_error);
}

TEST(PrimitiveCell, from_json_rejects_non_object) {
    parcel::json_t arr = parcel::json_t::array({1, 2, 3});
    EXPECT_THROW(parcel::I32Cell::from_json(arr, registry()), std::runtime_error);
}

// ---------------------------------------------------------------------------
// clone()

TEST(PrimitiveCell, clone_produces_independent_copy) {
    parcel::I32Cell v{42};
    const auto cloned = v.clone();
    ASSERT_NE(cloned, nullptr);
    EXPECT_EQ(cloned->kind(), "i32");

    auto* typed = dynamic_cast<parcel::I32Cell*>(cloned.get());
    ASSERT_NE(typed, nullptr);
    EXPECT_EQ(typed->get(), 42);

    // Mutating the original must not touch the clone.
    v.value = 99;
    EXPECT_EQ(typed->get(), 42);
}

// ---------------------------------------------------------------------------
// descriptor()

TEST(PrimitiveCell, descriptor_returns_non_null_shared_ptr) {
    const auto d = parcel::I32Cell::descriptor();
    ASSERT_NE(d, nullptr);
    EXPECT_EQ(d->kind(), "i32");
}

TEST(PrimitiveCell, descriptor_is_cached_per_type) {
    const auto d1 = parcel::I32Cell::descriptor();
    const auto d2 = parcel::I32Cell::descriptor();
    EXPECT_EQ(d1.get(), d2.get()) << "Meyers-static cache should return the same instance per type";
}

TEST(PrimitiveCell, descriptors_per_type_differ) {
    const auto i32 = parcel::I32Cell::descriptor();
    const auto i64 = parcel::I64Cell::descriptor();
    EXPECT_NE(i32.get(), static_cast<void const*>(i64.get()));
    EXPECT_EQ(i32->kind(), "i32");
    EXPECT_EQ(i64->kind(), "i64");
}

TEST(PrimitiveCell, descriptor_display_info_uses_trait_display_info) {
    const auto d = parcel::DoubleCell::descriptor();
    const auto info = d->display_info();
    EXPECT_EQ(info.name, "F64");
    ASSERT_TRUE(info.description.has_value());
    EXPECT_EQ(*info.description, "64-bit floating-point number.");
}

TEST(PrimitiveCell, descriptor_cell_from_json_round_trip) {
    const parcel::I32Cell original{42};
    const auto j = original.to_json();

    const auto desc = parcel::I32Cell::descriptor();
    const auto restored = desc->cell_from_json(j, registry());

    ASSERT_NE(restored, nullptr);
    EXPECT_EQ(restored->kind(), "i32");
    auto* typed = dynamic_cast<parcel::I32Cell*>(restored.get());
    ASSERT_NE(typed, nullptr);
    EXPECT_EQ(typed->get(), 42);
}

TEST(PrimitiveCell, descriptor_to_json_includes_kind_and_display_info) {
    const auto d = parcel::BoolCell::descriptor();
    const auto j = d->to_json();
    EXPECT_EQ(j["kind"].get<std::string>(), "bool");
    ASSERT_TRUE(j.contains("display_info"));
    EXPECT_EQ(j["display_info"]["name"].get<std::string>(), "Bool");
}

// ---------------------------------------------------------------------------
// 128-bit integer support (compiler-dependent; gated by COMMONS_HAS_INT128).

#ifdef COMMONS_HAS_INT128

static_assert(parcel::CellLike<parcel::U128Cell>);
static_assert(parcel::CellLike<parcel::I128Cell>);

TEST(PrimitiveCell128, kind_matches_trait_type_id) {
    EXPECT_EQ(parcel::U128Cell{}.kind(), "u128");
    EXPECT_EQ(parcel::I128Cell{}.kind(), "i128");
}

TEST(PrimitiveCell128, kind_id_static_constexpr) {
    static_assert(parcel::U128Cell::kind_id == "u128");
    static_assert(parcel::I128Cell::kind_id == "i128");
    SUCCEED();
}

TEST(PrimitiveCell128, default_value_is_zero) {
    EXPECT_EQ(parcel::U128Cell{}.get(), parcel::u128{0});
    EXPECT_EQ(parcel::I128Cell{}.get(), parcel::i128{0});
}

TEST(PrimitiveCell128, holds_full_64bit_range) {
    // A value that fits in 64 bits — sanity check the storage round-trips.
    parcel::u128 const big = (parcel::u128{1} << 63);
    parcel::U128Cell v{big};
    EXPECT_EQ(v.get(), big);
}

TEST(PrimitiveCell128, holds_above_64bit_range) {
    // 2^100 — only representable in 128-bit storage.
    parcel::u128 const big = parcel::u128{1} << 100;
    parcel::U128Cell v{big};
    EXPECT_EQ(v.get(), big);
    // Sanity: lower 64 bits are zero, upper bits are non-zero.
    EXPECT_EQ(static_cast<std::uint64_t>(v.get()), 0u);
    EXPECT_NE(static_cast<std::uint64_t>(v.get() >> 64), 0u);
}

TEST(PrimitiveCell128, signed_holds_negative_value) {
    parcel::I128Cell v{parcel::i128{-1}};
    EXPECT_EQ(v.get(), parcel::i128{-1});
}

TEST(PrimitiveCell128, to_string_zero) {
    EXPECT_EQ(parcel::U128Cell{parcel::u128{0}}.to_string(), "0");
    EXPECT_EQ(parcel::I128Cell{parcel::i128{0}}.to_string(), "0");
}

TEST(PrimitiveCell128, to_string_small_unsigned) {
    EXPECT_EQ(parcel::U128Cell{parcel::u128{42}}.to_string(), "42");
}

TEST(PrimitiveCell128, to_string_large_unsigned_above_64bit) {
    // 2^100 = 1267650600228229401496703205376
    parcel::u128 const v = parcel::u128{1} << 100;
    EXPECT_EQ(parcel::U128Cell{v}.to_string(), "1267650600228229401496703205376");
}

TEST(PrimitiveCell128, to_string_max_unsigned) {
    // 2^128 - 1 = 340282366920938463463374607431768211455
    auto const v = static_cast<parcel::u128>(-1);
    EXPECT_EQ(parcel::U128Cell{v}.to_string(), "340282366920938463463374607431768211455");
}

TEST(PrimitiveCell128, to_string_signed_positive) {
    EXPECT_EQ(parcel::I128Cell{parcel::i128{12345}}.to_string(), "12345");
}

TEST(PrimitiveCell128, to_string_signed_negative) {
    EXPECT_EQ(parcel::I128Cell{parcel::i128{-1}}.to_string(), "-1");
    EXPECT_EQ(parcel::I128Cell{parcel::i128{-12345}}.to_string(), "-12345");
}

TEST(PrimitiveCell128, to_string_signed_min) {
    // i128 min = -(2^127). Build it without UB: shift unsigned, reinterpret.
    parcel::u128 const u_min = parcel::u128{1} << 127;
    auto const i_min = static_cast<parcel::i128>(u_min);
    EXPECT_EQ(parcel::I128Cell{i_min}.to_string(), "-170141183460469231731687303715884105728");
}

TEST(PrimitiveCell128, clone_produces_independent_copy) {
    parcel::u128 const big = parcel::u128{1} << 100;
    parcel::U128Cell v{big};
    const auto cloned = v.clone();
    ASSERT_NE(cloned, nullptr);
    EXPECT_EQ(cloned->kind(), "u128");

    auto* typed = dynamic_cast<parcel::U128Cell*>(cloned.get());
    ASSERT_NE(typed, nullptr);
    EXPECT_EQ(typed->get(), big);

    v.value = parcel::u128{0};
    EXPECT_EQ(typed->get(), big);
}

TEST(PrimitiveCell128, descriptor_is_cached_per_type) {
    const auto a = parcel::U128Cell::descriptor();
    const auto b = parcel::U128Cell::descriptor();
    EXPECT_EQ(a.get(), b.get());
    EXPECT_EQ(a->kind(), "u128");
}

TEST(PrimitiveCell128, descriptor_display_info_uses_trait_display_info) {
    const auto d = parcel::I128Cell::descriptor();
    const auto info = d->display_info();
    EXPECT_EQ(info.name, "I128");
    ASSERT_TRUE(info.description.has_value());
    EXPECT_EQ(*info.description, "Signed 128-bit integer.");
}

TEST(PrimitiveCell128, to_json_emits_kind_and_value_keys_u128) {
    const parcel::U128Cell v{parcel::u128{42}};
    const auto j = v.to_json();
    ASSERT_TRUE(j.is_object());
    EXPECT_EQ(j["k"].get<std::string>(), "u128");
}

TEST(PrimitiveCell128, from_json_round_trip_u128_small) {
    const parcel::U128Cell original{parcel::u128{42}};
    const auto j = original.to_json();
    const auto restored = parcel::U128Cell::from_json(j, registry());
    auto* typed = dynamic_cast<parcel::U128Cell*>(restored.get());
    ASSERT_NE(typed, nullptr);
    EXPECT_EQ(typed->get(), parcel::u128{42});
}

TEST(PrimitiveCell128, from_json_round_trip_u128_above_64bit) {
    parcel::u128 const big = parcel::u128{1} << 100;
    const parcel::U128Cell original{big};
    const auto j = original.to_json();
    const auto restored = parcel::U128Cell::from_json(j, registry());
    auto* typed = dynamic_cast<parcel::U128Cell*>(restored.get());
    ASSERT_NE(typed, nullptr);
    EXPECT_EQ(typed->get(), big);
}

TEST(PrimitiveCell128, from_json_round_trip_i128_positive) {
    const parcel::I128Cell original{parcel::i128{12345}};
    const auto j = original.to_json();
    const auto restored = parcel::I128Cell::from_json(j, registry());
    auto* typed = dynamic_cast<parcel::I128Cell*>(restored.get());
    ASSERT_NE(typed, nullptr);
    EXPECT_EQ(typed->get(), parcel::i128{12345});
}

TEST(PrimitiveCell128, from_json_round_trip_i128_negative) {
    const parcel::I128Cell original{parcel::i128{-12345}};
    const auto j = original.to_json();
    const auto restored = parcel::I128Cell::from_json(j, registry());
    auto* typed = dynamic_cast<parcel::I128Cell*>(restored.get());
    ASSERT_NE(typed, nullptr);
    EXPECT_EQ(typed->get(), parcel::i128{-12345});
}

TEST(PrimitiveCell128, to_json_wire_format_u128_is_decimal_string) {
    parcel::u128 const big = parcel::u128{1} << 100;
    const parcel::U128Cell v{big};
    const auto j = v.to_json();
    ASSERT_TRUE(j["v"].is_string())
        << "u128 must serialize as a JSON string to avoid 64-bit narrowing";
    EXPECT_EQ(j["v"].get<std::string>(), "1267650600228229401496703205376");
}

TEST(PrimitiveCell128, to_json_wire_format_i128_negative_is_signed_string) {
    const parcel::I128Cell v{parcel::i128{-12345}};
    const auto j = v.to_json();
    ASSERT_TRUE(j["v"].is_string());
    EXPECT_EQ(j["v"].get<std::string>(), "-12345");
}

TEST(PrimitiveCell128, from_json_round_trip_u128_max) {
    auto const max_u128 = static_cast<parcel::u128>(-1);
    const parcel::U128Cell original{max_u128};
    const auto j = original.to_json();
    EXPECT_EQ(j["v"].get<std::string>(), "340282366920938463463374607431768211455");
    const auto restored = parcel::U128Cell::from_json(j, registry());
    auto* typed = dynamic_cast<parcel::U128Cell*>(restored.get());
    ASSERT_NE(typed, nullptr);
    EXPECT_EQ(typed->get(), max_u128);
}

TEST(PrimitiveCell128, from_json_round_trip_i128_min) {
    parcel::u128 const u_min = parcel::u128{1} << 127;
    auto const i_min = static_cast<parcel::i128>(u_min);
    const parcel::I128Cell original{i_min};
    const auto j = original.to_json();
    EXPECT_EQ(j["v"].get<std::string>(), "-170141183460469231731687303715884105728");
    const auto restored = parcel::I128Cell::from_json(j, registry());
    auto* typed = dynamic_cast<parcel::I128Cell*>(restored.get());
    ASSERT_NE(typed, nullptr);
    EXPECT_EQ(typed->get(), i_min);
}

// commons' 128-bit decoder (v0.1.3+) validates and range-checks, surfacing
// every failure as an nlohmann JSON error (other_error 502) — the same
// rejection parcel performed before, now delegated to commons. The error type
// is therefore `parcel::json_t::exception` (nlohmann's base), not a parcel one.
TEST(PrimitiveCell128, from_json_rejects_non_string_v_for_u128) {
    parcel::json_t j = {{"k", "u128"}, {"v", 42}};  // number, not string
    EXPECT_THROW(parcel::U128Cell::from_json(j, registry()), parcel::json_t::exception);
}

TEST(PrimitiveCell128, from_json_rejects_non_digit_in_string) {
    parcel::json_t j = {{"k", "u128"}, {"v", "12x4"}};
    EXPECT_THROW(parcel::U128Cell::from_json(j, registry()), parcel::json_t::exception);
}

TEST(PrimitiveCell128, from_json_rejects_empty_string) {
    parcel::json_t j = {{"k", "u128"}, {"v", ""}};
    EXPECT_THROW(parcel::U128Cell::from_json(j, registry()), parcel::json_t::exception);
}

TEST(PrimitiveCell128, from_json_rejects_lone_minus_for_i128) {
    parcel::json_t j = {{"k", "i128"}, {"v", "-"}};
    EXPECT_THROW(parcel::I128Cell::from_json(j, registry()), parcel::json_t::exception);
}

TEST(PrimitiveCell128, from_json_rejects_u128_overflow) {
    // 2^128 = u128_max + 1 — one over the addressable range.
    parcel::json_t j = {{"k", "u128"}, {"v", "340282366920938463463374607431768211456"}};
    EXPECT_THROW(parcel::U128Cell::from_json(j, registry()), parcel::json_t::exception);
}

TEST(PrimitiveCell128, from_json_rejects_u128_far_overflow) {
    // Way above u128 range — earlier digits already overflow the accumulator.
    parcel::json_t j = {{"k", "u128"}, {"v", "1000000000000000000000000000000000000000"}};
    EXPECT_THROW(parcel::U128Cell::from_json(j, registry()), parcel::json_t::exception);
}

TEST(PrimitiveCell128, from_json_rejects_i128_positive_overflow) {
    // i128_max + 1 = 2^127. Just over the positive bound.
    parcel::json_t j = {{"k", "i128"}, {"v", "170141183460469231731687303715884105728"}};
    EXPECT_THROW(parcel::I128Cell::from_json(j, registry()), parcel::json_t::exception);
}

TEST(PrimitiveCell128, from_json_accepts_i128_min_boundary) {
    // i128_min = -(2^127) is the largest-magnitude valid negative.
    parcel::json_t j = {{"k", "i128"}, {"v", "-170141183460469231731687303715884105728"}};
    EXPECT_NO_THROW({ (void)parcel::I128Cell::from_json(j, registry()); });
}

TEST(PrimitiveCell128, from_json_rejects_i128_negative_overflow) {
    // |i128_min| + 1 = 2^127 + 1 — one past the negative bound.
    parcel::json_t j = {{"k", "i128"}, {"v", "-170141183460469231731687303715884105729"}};
    EXPECT_THROW(parcel::I128Cell::from_json(j, registry()), parcel::json_t::exception);
}

#endif  // COMMONS_HAS_INT128

// ---------------------------------------------------------------------------
// Complex cells (commons cs8..cu64 / cf32 / cf64). Wire form is a
// [real, imaginary] JSON array via commons' adl_serializer<std::complex<T>>.

static_assert(parcel::CellLike<parcel::Cf32Cell>);
static_assert(parcel::CellLike<parcel::Cf64Cell>);
static_assert(parcel::CellLike<parcel::Cs8Cell>);
static_assert(parcel::CellLike<parcel::Cu64Cell>);

TEST(PrimitiveCellComplex, kind_ids_and_to_string) {
    EXPECT_EQ(parcel::Cf32Cell::kind_id, "cf32");
    EXPECT_EQ(parcel::Cf64Cell::kind_id, "cf64");
    EXPECT_EQ(parcel::Cs8Cell::kind_id, "cs8");
    EXPECT_EQ(parcel::Cu16Cell::kind_id, "cu16");

    const parcel::Cs32Cell c{comms::cs32{3, -4}};
    EXPECT_EQ(c.to_string(), "(3, -4)");
}

TEST(PrimitiveCellComplex, to_json_is_real_imag_array) {
    const parcel::Cf64Cell c{comms::cf64{1.5, -2.5}};
    const auto j = c.to_json();
    EXPECT_EQ(j["k"], "cf64");
    ASSERT_TRUE(j["v"].is_array());
    ASSERT_EQ(j["v"].size(), 2u);
    EXPECT_DOUBLE_EQ(j["v"][0].get<double>(), 1.5);
    EXPECT_DOUBLE_EQ(j["v"][1].get<double>(), -2.5);
}

TEST(PrimitiveCellComplex, float_round_trip_via_registry) {
    const auto cell = parcel::Cf32Cell::of(comms::cf32{1.25F, -3.5F});
    const auto restored = registry().cell_from_json(cell->to_json());
    EXPECT_EQ(*restored, *cell);
    auto* typed = dynamic_cast<parcel::Cf32Cell*>(restored.get());
    ASSERT_NE(typed, nullptr);
    EXPECT_EQ(typed->get(), (comms::cf32{1.25F, -3.5F}));
}

TEST(PrimitiveCellComplex, signed_and_unsigned_int_round_trip_via_registry) {
    const auto cs = parcel::Cs8Cell::of(comms::cs8{-5, 6});
    const auto cu = parcel::Cu64Cell::of(comms::cu64{7, 8});
    EXPECT_EQ(*registry().cell_from_json(cs->to_json()), *cs);
    EXPECT_EQ(*registry().cell_from_json(cu->to_json()), *cu);
}

TEST(PrimitiveCellComplex, typed_list_round_trip_via_registry) {
    parcel::Cf64ListCell list{comms::cf64{1.0, 2.0}, comms::cf64{3.0, 4.0}};
    const auto j = list.to_json();
    EXPECT_EQ(j["k"], "l:cf64");
    const auto restored = registry().cell_from_json(j);
    EXPECT_EQ(*restored, list);
}

TEST(PrimitiveCellComplex, typed_map_round_trip_via_registry) {
    parcel::Cf32MapCell m;
    m.emplace("re", comms::cf32{0.5F, 1.5F});
    const auto j = m.to_json();
    EXPECT_EQ(j["k"], "m:cf32");
    const auto restored = registry().cell_from_json(j);
    EXPECT_EQ(*restored, m);
}

TEST(PrimitiveCellComplex, registered_in_default_registry) {
    const auto& reg = registry();
    for (const auto* kind : {"cs8",
                             "cs16",
                             "cs32",
                             "cs64",
                             "cu8",
                             "cu16",
                             "cu32",
                             "cu64",
                             "cf32",
                             "cf64",
                             "l:cf64",
                             "m:cf32",
                             "hm:cu16"}) {
        EXPECT_TRUE(reg.contains(kind)) << "missing kind: " << kind;
    }
}
