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
}

TEST(CommonsCells, can_be_disabled_via_options) {
    const parcel::ParcelRegistry reg{parcel::BuiltinsOptions{.commons = false}};
    EXPECT_FALSE(reg.contains("color"));
    EXPECT_FALSE(reg.contains("flag_set"));
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
