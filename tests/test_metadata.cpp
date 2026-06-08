#include <parcel/parcel.h>

#include <gtest/gtest.h>

#include <string_view>

using comms::md::Array;
using comms::md::Object;
using comms::md::Value;
using parcel::ArrayCell;
using parcel::ObjectCell;
using parcel::ValueCell;

// ---------------------------------------------------------------------------
// Registration

TEST(MetadataCell, registered_in_default_registry) {
    const parcel::ParcelRegistry reg;
    EXPECT_TRUE(reg.contains("md:v"));
    EXPECT_TRUE(reg.contains("md:o"));
    EXPECT_TRUE(reg.contains("md:a"));
}

TEST(MetadataCell, can_be_disabled_via_options) {
    const parcel::ParcelRegistry reg{parcel::BuiltinsOptions{.commons = false}};
    EXPECT_FALSE(reg.contains("md:v"));
    EXPECT_FALSE(reg.contains("md:o"));
    EXPECT_FALSE(reg.contains("md:a"));
}

TEST(MetadataCell, kind_ids) {
    EXPECT_EQ(ValueCell::kind_id, std::string_view{"md:v"});
    EXPECT_EQ(ObjectCell::kind_id, std::string_view{"md:o"});
    EXPECT_EQ(ArrayCell::kind_id, std::string_view{"md:a"});
}

// ---------------------------------------------------------------------------
// Round-trips

TEST(MetadataCell, value_round_trip) {
    const parcel::ParcelRegistry reg;
    const ValueCell cell{Value{42}};
    const auto j = cell.to_json();
    EXPECT_EQ(j.at("k").get<std::string>(), "md:v");

    const auto restored = reg.cell_from_json(j);
    const auto* typed = dynamic_cast<ValueCell*>(restored.get());
    ASSERT_NE(typed, nullptr);
    EXPECT_TRUE(typed->value.is_int());
    EXPECT_EQ(typed->value.as_int(), 42);
}

TEST(MetadataCell, object_round_trip) {
    const parcel::ParcelRegistry reg;
    const Object payload{
        {"name", "sensor"},
        {"count", 7},
        {"readings", Array{1.0, 2.5, 3.75}},
    };
    const ObjectCell cell{payload};
    const auto j = cell.to_json();
    EXPECT_EQ(j.at("k").get<std::string>(), "md:o");

    const auto restored = reg.cell_from_json(j);
    const auto* typed = dynamic_cast<ObjectCell*>(restored.get());
    ASSERT_NE(typed, nullptr);
    EXPECT_EQ(typed->value, payload);
}

TEST(MetadataCell, array_round_trip) {
    const parcel::ParcelRegistry reg;
    const Array payload{Value{1}, Value{"x"}, Value{true}};
    const ArrayCell cell{payload};
    const auto j = cell.to_json();
    EXPECT_EQ(j.at("k").get<std::string>(), "md:a");

    const auto restored = reg.cell_from_json(j);
    const auto* typed = dynamic_cast<ArrayCell*>(restored.get());
    ASSERT_NE(typed, nullptr);
    ASSERT_EQ(typed->value.size(), payload.size());
    EXPECT_EQ(typed->value, payload);
}

// ---------------------------------------------------------------------------
// Errors / brace-init / descriptors / inference

TEST(MetadataCell, kind_mismatch_rejected) {
    const parcel::ParcelRegistry reg;
    parcel::json_t wrong = {{"k", "md:o"}, {"v", parcel::json_t::object({{"k", 1}})}};
    // Feeding an object envelope into ValueCell::from_json must fail the kind check.
    EXPECT_THROW((void)ValueCell::from_json(wrong, reg), parcel::ParcelException);
}

TEST(MetadataCell, descriptor_carries_icon_and_color) {
    const auto vd = ValueCell::descriptor()->display_info();
    EXPECT_TRUE(vd.icon.has_value());
    EXPECT_TRUE(vd.color.has_value());
    EXPECT_EQ(*vd.icon, comms::Icons::mdi::variable);
    EXPECT_EQ(*vd.color, comms::Colors::mui::deep_purple[500]);

    const auto od = ObjectCell::descriptor()->display_info();
    EXPECT_EQ(*od.icon, comms::Icons::mdi::code_braces);
    EXPECT_EQ(*od.color, comms::Colors::mui::amber[700]);

    const auto ad = ArrayCell::descriptor()->display_info();
    EXPECT_EQ(*ad.icon, comms::Icons::mdi::code_brackets);
    EXPECT_EQ(*ad.color, comms::Colors::mui::green[600]);
}

TEST(MetadataCell, brace_init_passthrough) {
    const ArrayCell a{"a", "b", "c"};
    ASSERT_EQ(a.value.size(), 3u);
    EXPECT_EQ(a.value[0].as_string(), "a");
    EXPECT_EQ(a.value[2].as_string(), "c");

    const ObjectCell o{{"name", "x"}, {"count", 7}};
    EXPECT_EQ(o.value.size(), 2u);
    EXPECT_EQ(o.value.at("name").as_string(), "x");
    EXPECT_EQ(o.value.at("count").as_int(), 7);
}

TEST(MetadataCell, default_cell_inference) {
    const parcel::cell_t v_handle = parcel::cell(Value{1});
    const parcel::cell_t o_handle = parcel::cell(Object{{"k", 1}});
    const parcel::cell_t a_handle = parcel::cell(Array{Value{1}});

    EXPECT_EQ(v_handle->kind(), std::string_view{"md:v"});
    EXPECT_EQ(o_handle->kind(), std::string_view{"md:o"});
    EXPECT_EQ(a_handle->kind(), std::string_view{"md:a"});
}
