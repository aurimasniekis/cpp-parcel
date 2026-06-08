#include <parcel/parcel.h>

#include <gtest/gtest.h>

#include <memory>
#include <string>

// ---------------------------------------------------------------------------
// Registration

TEST(AbilityCell, registered_in_default_registry) {
    const parcel::ParcelRegistry reg;
    EXPECT_TRUE(reg.contains("ability"));
}

TEST(AbilityCell, can_be_disabled_via_options) {
    const parcel::ParcelRegistry reg{parcel::BuiltinsOptions{.commons = false}};
    EXPECT_FALSE(reg.contains("ability"));
}

// ---------------------------------------------------------------------------
// RoleAbility

TEST(AbilityCell, role_round_trip) {
    const parcel::ParcelRegistry reg;
    const auto cell = parcel::AbilityCell::of(comms::make_ability<comms::RoleAbility>("admin"));
    EXPECT_EQ(cell->kind(), "ability");
    EXPECT_EQ(cell->to_string(), "role");
    EXPECT_EQ(cell->to_json()["v"]["kind"], "role");
    EXPECT_EQ(cell->to_json()["v"]["value"], "admin");

    const auto restored = reg.cell_from_json(cell->to_json());
    EXPECT_EQ(*restored, *cell);
    auto* ac = dynamic_cast<parcel::AbilityCell*>(restored.get());
    ASSERT_NE(ac, nullptr);
    ASSERT_TRUE(ac->value);
    EXPECT_EQ(ac->value->value, "admin");
}

// allowed() semantics: a required role is satisfied only by the same role name.
TEST(AbilityCell, role_allowed_semantics_survive_round_trip) {
    const parcel::ParcelRegistry reg;
    const auto required = comms::make_ability<comms::RoleAbility>("admin");

    const auto cell = parcel::AbilityCell::of(comms::make_ability<comms::RoleAbility>("admin"));
    const auto restored = reg.cell_from_json(cell->to_json());
    auto* ac = dynamic_cast<parcel::AbilityCell*>(restored.get());
    ASSERT_NE(ac, nullptr);
    EXPECT_TRUE(required->allowed(*ac->value));

    const auto other = comms::make_ability<comms::RoleAbility>("editor");
    EXPECT_FALSE(required->allowed(*other));
}

// ---------------------------------------------------------------------------
// RecordPermissionAbility (extra fields + wildcard allowed())

TEST(AbilityCell, record_permission_round_trip) {
    const parcel::ParcelRegistry reg;
    auto perm = std::make_unique<comms::RecordPermissionAbility>("read", "order");
    const auto cell = parcel::AbilityCell::of(std::move(perm));
    EXPECT_EQ(cell->to_json()["v"]["action"], "read");
    EXPECT_EQ(cell->to_json()["v"]["resource"], "order");

    const auto restored = reg.cell_from_json(cell->to_json());
    EXPECT_EQ(*restored, *cell);
    auto* ac = dynamic_cast<parcel::AbilityCell*>(restored.get());
    ASSERT_NE(ac, nullptr);
    auto* rp = dynamic_cast<comms::RecordPermissionAbility*>(ac->value.get());
    ASSERT_NE(rp, nullptr);
    EXPECT_EQ(rp->action, "read");
    EXPECT_EQ(rp->resource, "order");
}

TEST(AbilityCell, record_permission_wildcard_allowed_survives) {
    const parcel::ParcelRegistry reg;
    // A required "*"/"*" permission is satisfied by any concrete one.
    const auto required = std::make_unique<comms::RecordPermissionAbility>("*", "*");

    const auto cell = parcel::AbilityCell::of(
        std::make_unique<comms::RecordPermissionAbility>("write", "invoice"));
    const auto restored = reg.cell_from_json(cell->to_json());
    auto* ac = dynamic_cast<parcel::AbilityCell*>(restored.get());
    ASSERT_NE(ac, nullptr);
    EXPECT_TRUE(required->allowed(*ac->value));
}

// ---------------------------------------------------------------------------
// Errors / inference

TEST(AbilityCell, unknown_kind_throws) {
    const parcel::ParcelRegistry reg;
    const parcel::json_t j{{"k", "ability"}, {"v", {{"kind", "never_registered"}}}};
    EXPECT_ANY_THROW((void)reg.cell_from_json(j));
}

TEST(AbilityCell, cell_factory_infers_wrapper) {
    const auto handle = parcel::cell(comms::make_ability<comms::RoleAbility>("admin"));
    EXPECT_EQ(handle->kind(), "ability");
}
