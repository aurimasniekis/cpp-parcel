#include <parcel/parcel.h>

#include <gtest/gtest.h>

#include <memory>

// ---------------------------------------------------------------------------
// Registration

TEST(IdentityCell, registered_in_default_registry) {
    const parcel::ParcelRegistry reg;
    EXPECT_TRUE(reg.contains("identity"));
}

TEST(IdentityCell, can_be_disabled_via_options) {
    const parcel::ParcelRegistry reg{parcel::BuiltinsOptions{.commons = false}};
    EXPECT_FALSE(reg.contains("identity"));
}

// ---------------------------------------------------------------------------
// Identity carrying abilities round-trips

TEST(IdentityCell, user_with_abilities_round_trip) {
    const parcel::ParcelRegistry reg;
    auto user = comms::make_identity<comms::UserIdentity>("alice");
    user->add_ability(comms::make_ability<comms::RoleAbility>("admin"));
    user->add_ability(std::make_unique<comms::RecordPermissionAbility>("read", "order"));

    const auto cell = parcel::IdentityCell::of(std::move(user));
    EXPECT_EQ(cell->kind(), "identity");
    EXPECT_EQ(cell->to_string(), "user");
    ASSERT_TRUE(cell->to_json()["v"]["abilities"].is_array());
    EXPECT_EQ(cell->to_json()["v"]["abilities"].size(), 2u);

    const auto restored = reg.cell_from_json(cell->to_json());
    EXPECT_EQ(*restored, *cell);
    auto* ic = dynamic_cast<parcel::IdentityCell*>(restored.get());
    ASSERT_NE(ic, nullptr);
    ASSERT_TRUE(ic->value);
    EXPECT_EQ(ic->value->value, "alice");
    ASSERT_EQ(ic->value->abilities.size(), 2u);
    EXPECT_EQ(ic->value->abilities[0]->kind(), "role");
    EXPECT_EQ(ic->value->abilities[1]->kind(), "record_permission");
}

// ---------------------------------------------------------------------------
// satisfies() semantics survive a round-trip

TEST(IdentityCell, root_satisfies_all_after_round_trip) {
    const parcel::ParcelRegistry reg;
    const auto cell = parcel::IdentityCell::of(comms::make_identity<comms::RootIdentity>());
    const auto restored = reg.cell_from_json(cell->to_json());
    auto* ic = dynamic_cast<parcel::IdentityCell*>(restored.get());
    ASSERT_NE(ic, nullptr);
    const auto required = comms::make_ability<comms::RoleAbility>("anything");
    EXPECT_TRUE(ic->value->satisfies(*required));
}

TEST(IdentityCell, none_satisfies_nothing_after_round_trip) {
    const parcel::ParcelRegistry reg;
    const auto cell = parcel::IdentityCell::of(comms::make_identity<comms::NoIdentity>());
    const auto restored = reg.cell_from_json(cell->to_json());
    auto* ic = dynamic_cast<parcel::IdentityCell*>(restored.get());
    ASSERT_NE(ic, nullptr);
    const auto required = comms::make_ability<comms::RoleAbility>("anything");
    EXPECT_FALSE(ic->value->satisfies(*required));
}

// ---------------------------------------------------------------------------
// Errors / inference

TEST(IdentityCell, unknown_kind_throws) {
    const parcel::ParcelRegistry reg;
    const parcel::json_t j{{"k", "identity"}, {"v", {{"kind", "never_registered"}}}};
    EXPECT_ANY_THROW((void)reg.cell_from_json(j));
}

TEST(IdentityCell, cell_factory_infers_wrapper) {
    const auto handle = parcel::cell(comms::make_identity<comms::UserIdentity>("bob"));
    EXPECT_EQ(handle->kind(), "identity");
}
