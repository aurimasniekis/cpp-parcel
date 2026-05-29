#include <parcel/parcel.h>

#include <gtest/gtest.h>

#include <memory>
#include <stdexcept>
#include <string>

namespace {

// Flags used by the FlagCell / FlagSetCell tests. COMMONS_DEFINE_FLAG emits a
// self-registering object so the GlobalFlagRegistry knows them before main().
COMMONS_DEFINE_FLAG(AlphaFlag, "test.alpha");
COMMONS_DEFINE_FLAG(BetaFlag, "test.beta");

}  // namespace

// ---------------------------------------------------------------------------
// Registration

TEST(CommonsCells, registered_in_default_registry) {
    const parcel::ParcelRegistry reg;
    EXPECT_TRUE(reg.contains("color"));
    EXPECT_TRUE(reg.contains("icon"));
    EXPECT_TRUE(reg.contains("display_info"));
    EXPECT_TRUE(reg.contains("flag"));
    EXPECT_TRUE(reg.contains("flag_set"));
    EXPECT_TRUE(reg.contains("semver"));
    EXPECT_TRUE(reg.contains("version_constraint"));
    EXPECT_TRUE(reg.contains("origin"));
}

TEST(CommonsCells, can_be_disabled_via_options) {
    const parcel::ParcelRegistry reg{parcel::BuiltinsOptions{.commons = false}};
    EXPECT_FALSE(reg.contains("color"));
    EXPECT_FALSE(reg.contains("flag_set"));
    EXPECT_FALSE(reg.contains("semver"));
    EXPECT_FALSE(reg.contains("version_constraint"));
    EXPECT_FALSE(reg.contains("origin"));
}

TEST(CommonsCells, cell_factory_infers_commons_wrappers) {
    EXPECT_EQ(parcel::cell(comms::Color::from_rgb_int(0x00ff00))->kind(), "color");
    EXPECT_EQ(parcel::cell(comms::Icon::from("mdi:home"))->kind(), "icon");
}

// ---------------------------------------------------------------------------
// ColorCell

TEST(CommonsCells, color_cell_basics) {
    const auto c = parcel::ColorCell::of(comms::Color::from_rgb_int(0xff0000));
    EXPECT_EQ(c->kind(), "color");
    EXPECT_EQ(c->to_string(), "#ff0000");
    EXPECT_EQ(c->to_json()["v"], "#ff0000");
}

TEST(CommonsCells, color_cell_round_trip) {
    const parcel::ParcelRegistry reg;
    const auto cell = parcel::ColorCell::of(*comms::Color::parse("#123456"));
    const auto restored = reg.cell_from_json(cell->to_json());
    EXPECT_EQ(*restored, *cell);
    EXPECT_EQ(restored->to_string(), "#123456");
}

// ---------------------------------------------------------------------------
// IconCell

TEST(CommonsCells, icon_cell_round_trip) {
    const parcel::ParcelRegistry reg;
    const auto cell = parcel::IconCell::of(comms::Icon::from("mdi:account"));
    EXPECT_EQ(cell->kind(), "icon");
    EXPECT_EQ(cell->to_string(), "mdi:account");
    EXPECT_EQ(cell->to_json()["v"], "mdi:account");
    const auto restored = reg.cell_from_json(cell->to_json());
    EXPECT_EQ(*restored, *cell);
}

TEST(CommonsCells, icon_cell_rejects_malformed_value) {
    const parcel::ParcelRegistry reg;
    const parcel::json_t j{{"k", "icon"}, {"v", "no-colon"}};
    EXPECT_ANY_THROW((void)reg.cell_from_json(j));
}

// ---------------------------------------------------------------------------
// DisplayInfoCell

TEST(CommonsCells, display_info_cell_round_trip) {
    const parcel::ParcelRegistry reg;
    const comms::DisplayInfo di{
        .name = "Widget",
        .description = "A widget",
        .icon = comms::Icon::from("mdi:widgets"),
        .color = comms::Color::parse("teal"),
    };
    const auto cell = parcel::DisplayInfoCell::of(di);
    EXPECT_EQ(cell->kind(), "display_info");

    const auto restored = reg.cell_from_json(cell->to_json());
    EXPECT_EQ(*restored, *cell);
    auto* dc = dynamic_cast<parcel::DisplayInfoCell*>(restored.get());
    ASSERT_NE(dc, nullptr);
    EXPECT_EQ(dc->value, di);
}

// ---------------------------------------------------------------------------
// FlagCell

TEST(CommonsCells, flag_cell_round_trip) {
    const parcel::ParcelRegistry reg;
    const auto cell = parcel::FlagCell::of(comms::FlagRef::of<AlphaFlag>());
    EXPECT_EQ(cell->kind(), "flag");
    EXPECT_EQ(cell->to_string(), "test.alpha");
    EXPECT_EQ(cell->to_json()["v"], "test.alpha");

    const auto restored = reg.cell_from_json(cell->to_json());
    EXPECT_EQ(*restored, *cell);
}

TEST(CommonsCells, flag_cell_unregistered_name_throws) {
    const parcel::ParcelRegistry reg;
    // Decoding resolves the name against comms::GlobalFlagRegistry; an unknown
    // flag throws (commons behavior).
    const parcel::json_t j{{"k", "flag"}, {"v", "test.never_registered"}};
    EXPECT_ANY_THROW((void)reg.cell_from_json(j));
}

// ---------------------------------------------------------------------------
// FlagSetCell

TEST(CommonsCells, flag_set_cell_round_trip) {
    const parcel::ParcelRegistry reg;
    const auto cell = parcel::FlagSetCell::of(comms::FlagSet::of<AlphaFlag, BetaFlag>());
    EXPECT_EQ(cell->kind(), "flag_set");
    EXPECT_EQ(cell->to_string(), "[test.alpha, test.beta]");

    const auto j = cell->to_json();
    ASSERT_TRUE(j["v"].is_array());
    EXPECT_EQ(j["v"].size(), 2u);

    const auto restored = reg.cell_from_json(j);
    EXPECT_EQ(*restored, *cell);
}

TEST(CommonsCells, flag_set_cell_unregistered_name_throws) {
    const parcel::ParcelRegistry reg;
    const parcel::json_t j{{"k", "flag_set"},
                           {"v", parcel::json_t::array({"test.never_registered"})}};
    EXPECT_ANY_THROW((void)reg.cell_from_json(j));
}

// ---------------------------------------------------------------------------
// SemVerCell

TEST(CommonsCells, semver_cell_round_trip) {
    const parcel::ParcelRegistry reg;
    const auto cell = parcel::SemVerCell::of(*comms::SemVer::parse("1.2.3-rc.1+build.7"));
    EXPECT_EQ(cell->kind(), "semver");
    EXPECT_EQ(cell->to_string(), "1.2.3-rc.1+build.7");
    EXPECT_EQ(cell->to_json()["v"], "1.2.3-rc.1+build.7");

    const auto restored = reg.cell_from_json(cell->to_json());
    EXPECT_EQ(*restored, *cell);
    EXPECT_EQ(restored->to_string(), "1.2.3-rc.1+build.7");
}

TEST(CommonsCells, semver_cell_rejects_malformed_value) {
    const parcel::ParcelRegistry reg;
    const parcel::json_t j{{"k", "semver"}, {"v", "not.a.version!"}};
    EXPECT_ANY_THROW((void)reg.cell_from_json(j));
}

// ---------------------------------------------------------------------------
// VersionConstraintCell

TEST(CommonsCells, version_constraint_cell_round_trip) {
    const parcel::ParcelRegistry reg;
    const auto cell =
        parcel::VersionConstraintCell::of(comms::VersionConstraint::parse(">=1.2.0 <2.0.0"));
    EXPECT_EQ(cell->kind(), "version_constraint");
    EXPECT_EQ(cell->to_string(), ">=1.2.0 <2.0.0");
    EXPECT_EQ(cell->to_json()["v"], ">=1.2.0 <2.0.0");

    const auto restored = reg.cell_from_json(cell->to_json());
    EXPECT_EQ(*restored, *cell);

    // The decoded constraint still matches as expected.
    auto* vc = dynamic_cast<parcel::VersionConstraintCell*>(restored.get());
    ASSERT_NE(vc, nullptr);
    EXPECT_TRUE(vc->value.satisfies(*comms::SemVer::parse("1.5.0")));
    EXPECT_FALSE(vc->value.satisfies(*comms::SemVer::parse("2.0.0")));
}

TEST(CommonsCells, version_constraint_cell_rejects_malformed_range) {
    const parcel::ParcelRegistry reg;
    // commons' VersionConstraint::parse throws on a malformed sub-version.
    const parcel::json_t j{{"k", "version_constraint"}, {"v", ">=not-a-version"}};
    EXPECT_ANY_THROW((void)reg.cell_from_json(j));
}

// ---------------------------------------------------------------------------
// OriginCell

TEST(CommonsCells, origin_cell_basics) {
    const auto cell = parcel::OriginCell::of(std::make_unique<comms::CoreOrigin>());
    EXPECT_EQ(cell->kind(), "origin");
    EXPECT_EQ(cell->to_string(), "core");
    EXPECT_EQ(cell->to_json()["v"]["kind"], "core");
}

TEST(CommonsCells, origin_cell_null_value) {
    const auto cell = parcel::OriginCell::of(comms::OriginPtr{});
    EXPECT_EQ(cell->to_string(), "null");
    EXPECT_TRUE(cell->to_json()["v"].is_null());
}

TEST(CommonsCells, origin_cell_round_trip_builtin_kinds) {
    const parcel::ParcelRegistry reg;
    for (const auto factory :
         {+[]() -> comms::OriginPtr { return std::make_unique<comms::CoreOrigin>(); },
          +[]() -> comms::OriginPtr { return std::make_unique<comms::InternalOrigin>(); },
          +[]() -> comms::OriginPtr { return std::make_unique<comms::UnknownOrigin>(); }}) {
        const auto cell = parcel::OriginCell::of(factory());
        const auto restored = reg.cell_from_json(cell->to_json());
        EXPECT_EQ(*restored, *cell);
        EXPECT_EQ(restored->to_string(), cell->to_string());
    }
}

TEST(CommonsCells, origin_cell_external_round_trips_source) {
    const parcel::ParcelRegistry reg;
    const auto cell =
        parcel::OriginCell::of(std::make_unique<comms::ExternalOrigin>("marketplace"));
    EXPECT_EQ(cell->to_string(), "external");
    EXPECT_EQ(cell->to_json()["v"]["source"], "marketplace");

    const auto restored = reg.cell_from_json(cell->to_json());
    EXPECT_EQ(*restored, *cell);
    auto* oc = dynamic_cast<parcel::OriginCell*>(restored.get());
    ASSERT_NE(oc, nullptr);
    ASSERT_TRUE(oc->value);
    EXPECT_EQ(oc->value->kind(), "external");
    EXPECT_EQ(dynamic_cast<comms::ExternalOrigin&>(*oc->value).source, "marketplace");
}

TEST(CommonsCells, origin_cell_clone_is_deep) {
    const auto cell = parcel::OriginCell::of(std::make_unique<comms::ExternalOrigin>("src"));
    const auto cloned = cell->clone();
    EXPECT_EQ(*cloned, *cell);
    auto* oc = dynamic_cast<parcel::OriginCell*>(cloned.get());
    ASSERT_NE(oc, nullptr);
    ASSERT_TRUE(oc->value);
    // Distinct heap object (deep clone), equal content.
    EXPECT_NE(oc->value.get(), dynamic_cast<parcel::OriginCell*>(cell.get())->value.get());
}

TEST(CommonsCells, origin_cell_unknown_kind_throws) {
    const parcel::ParcelRegistry reg;
    // Decoding resolves "kind" against comms::GlobalOriginRegistry; an
    // unregistered kind throws (commons behavior).
    const parcel::json_t j{{"k", "origin"}, {"v", {{"kind", "never_registered"}}}};
    EXPECT_ANY_THROW((void)reg.cell_from_json(j));
}
