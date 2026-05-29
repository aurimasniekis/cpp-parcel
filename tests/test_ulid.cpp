#include <parcel/parcel.h>

#include <gtest/gtest.h>

// Inert when the ULID dependency is off (default). The body only compiles when
// PARCEL_HAS_ULID is defined — i.e. the PARCEL_WITH_ULID / Meson `ulid` option
// pulled in aurimasniekis/cpp-ulid.
#if PARCEL_HAS_ULID

#include <string>

namespace {

// A fixed, known ULID string (26-char Crockford base32).
constexpr std::string_view kSample = "01ARZ3NDEKTSV4RRFFQ69G5FAV";

}  // namespace

TEST(UlidCell, registered_in_default_registry) {
    const parcel::ParcelRegistry reg;
    EXPECT_TRUE(reg.contains("ulid"));
}

TEST(UlidCell, can_be_disabled_via_options) {
    const parcel::ParcelRegistry reg{parcel::BuiltinsOptions{.ulid = false}};
    EXPECT_FALSE(reg.contains("ulid"));
}

TEST(UlidCell, to_string_is_crockford_base32) {
    const auto u = ulid::Ulid::from_string(kSample);
    ASSERT_TRUE(u.has_value());
    const auto cell = parcel::UlidCell::of(*u);
    EXPECT_EQ(cell->kind(), "ulid");
    EXPECT_EQ(cell->to_string(), std::string{kSample});
    EXPECT_EQ(cell->to_json()["v"], std::string{kSample});
}

TEST(UlidCell, json_round_trip_via_registry) {
    const parcel::ParcelRegistry reg;
    const auto u = ulid::Ulid::from_string(kSample);
    ASSERT_TRUE(u.has_value());
    const auto cell = parcel::UlidCell::of(*u);

    const auto restored = reg.cell_from_json(cell->to_json());
    EXPECT_EQ(*restored, *cell);
    EXPECT_EQ(restored->to_string(), std::string{kSample});
}

TEST(UlidCell, rejects_malformed_value) {
    const parcel::ParcelRegistry reg;
    const parcel::json_t j{{"k", "ulid"}, {"v", "not-a-valid-ulid"}};
    EXPECT_ANY_THROW((void)reg.cell_from_json(j));
}

#endif  // PARCEL_HAS_ULID
