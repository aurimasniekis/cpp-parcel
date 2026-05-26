#include <parcel/parcel.h>

#include <gtest/gtest.h>

#include <format>
#include <memory>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

TEST(parcel, version_string_matches_components) {
    EXPECT_EQ(parcel::version, "0.2.0");
    EXPECT_EQ(parcel::version_major, 0);
    EXPECT_EQ(parcel::version_minor, 2);
    EXPECT_EQ(parcel::version_patch, 0);
}

TEST(parcel, json_dependency_is_wired_up) {
    parcel::json_t j = {{"hello", "world"}, {"n", 42}};
    EXPECT_EQ(j["hello"].get<std::string>(), "world");
    EXPECT_EQ(j["n"].get<int>(), 42);
}

TEST(parcel, icell_subtype_converts_to_json_via_adl) {
    parcel::I32Cell v{42};
    parcel::json_t j = v;
    EXPECT_EQ(j["k"].get<std::string>(), "i32");
    EXPECT_EQ(j["v"].get<int>(), 42);
}

TEST(parcel, vector_of_icell_converts_to_json_via_adl) {
    std::vector<parcel::I32Cell> elems{parcel::I32Cell{1}, parcel::I32Cell{2}};
    parcel::json_t j = elems;
    ASSERT_TRUE(j.is_array());
    ASSERT_EQ(j.size(), 2u);
    EXPECT_EQ(j[0]["k"].get<std::string>(), "i32");
    EXPECT_EQ(j[0]["v"].get<int>(), 1);
    EXPECT_EQ(j[1]["v"].get<int>(), 2);
}

// ---------------------------------------------------------------------------
// shared_ptr<ICell-derived> ADL hook (cell.h)

TEST(parcel, shared_ptr_to_icell_subtype_converts_via_adl) {
    auto v = std::make_shared<parcel::I32Cell>(7);
    parcel::json_t j = v;
    EXPECT_EQ(j["k"].get<std::string>(), "i32");
    EXPECT_EQ(j["v"].get<int>(), 7);
}

TEST(parcel, shared_ptr_null_icell_converts_to_json_null) {
    std::shared_ptr<parcel::I32Cell> v;
    const parcel::json_t j = v;
    EXPECT_TRUE(j.is_null());
}

// ---------------------------------------------------------------------------
// ICell::to_formatted_string default implementation

TEST(parcel, to_formatted_string_defaults_to_to_string) {
    const parcel::I32Cell v{42};
    parcel::ICell const& iv = v;
    EXPECT_EQ(iv.to_formatted_string(), iv.to_string());
}

// ---------------------------------------------------------------------------
// std::formatter<parcel::ICell> / cell_t

TEST(parcel, formatter_default_calls_to_string) {
    const parcel::I32Cell v{42};
    EXPECT_EQ(std::format("{}", v), v.to_string());
}

TEST(parcel, formatter_alt_form_calls_to_formatted_string) {
    const parcel::I32Cell v{42};
    EXPECT_EQ(std::format("{:#}", v), v.to_formatted_string());
}

TEST(parcel, formatter_works_on_cell_t) {
    parcel::cell_t v = parcel::I32Cell::of(7);
    EXPECT_EQ(std::format("{}", v), v->to_string());
    EXPECT_EQ(std::format("{:#}", v), v->to_formatted_string());
}

TEST(parcel, formatter_null_cell_t_renders_as_null_marker) {
    const parcel::cell_t v;
    EXPECT_EQ(std::format("{}", v), "<null>");
}

TEST(parcel, formatter_rejects_unknown_spec) {
    const parcel::I32Cell v{42};
    EXPECT_THROW((void)std::vformat("{:?}", std::make_format_args(v)), std::format_error);
}

TEST(parcel, ostream_operator_renders_to_string_for_icell_ref) {
    const parcel::I32Cell v{42};
    parcel::ICell const& iv = v;
    std::ostringstream os;
    os << iv;
    EXPECT_EQ(os.str(), v.to_string());
}

TEST(parcel, ostream_operator_renders_to_string_for_cell_t) {
    const parcel::cell_t v = parcel::I32Cell::of(7);
    std::ostringstream os;
    os << v;
    EXPECT_EQ(os.str(), v->to_string());
}

TEST(parcel, ostream_operator_renders_null_marker_for_null_cell_t) {
    const parcel::cell_t v;
    std::ostringstream os;
    os << v;
    EXPECT_EQ(os.str(), "<null>");
}

// ---------------------------------------------------------------------------
// ParcelRegistry error paths

TEST(parcel, registry_register_kind_rejects_null) {
    parcel::ParcelRegistry reg;
    EXPECT_THROW(reg.register_kind(nullptr), std::runtime_error);
}

TEST(parcel, registry_cell_from_json_rejects_non_object) {
    parcel::ParcelRegistry reg;
    parcel::json_t arr = parcel::json_t::array({1});
    EXPECT_THROW((void)reg.cell_from_json(arr), std::runtime_error);
}

TEST(parcel, registry_cell_from_json_rejects_missing_kind) {
    parcel::ParcelRegistry reg;
    parcel::json_t j = {{"v", 1}};
    EXPECT_THROW((void)reg.cell_from_json(j), std::runtime_error);
}

TEST(parcel, registry_find_returns_nullptr_for_unknown_kind) {
    const parcel::ParcelRegistry reg;
    EXPECT_EQ(reg.find("no_such_kind"), nullptr);
}

// ---------------------------------------------------------------------------
// BaseCell::to_json runtime error for non-JSON-convertible Storage

namespace {

struct OpaquePayload {
    [[maybe_unused]] int x = 0;
};

class OpaqueCell : public parcel::BaseCell<OpaqueCell, OpaquePayload> {
public:
    using BaseCell::BaseCell;
    [[maybe_unused]] static constexpr std::string_view kind_id = "opaque";
    [[nodiscard]] std::string to_string() const override {
        return "opaque";
    }
};

}  // namespace

TEST(parcel, base_cell_to_json_throws_when_storage_not_json_convertible) {
    const OpaqueCell v;
    EXPECT_THROW((void)v.to_json(), std::runtime_error);
}

// ---------------------------------------------------------------------------
// nlohmann::adl_serializer<std::optional<T>>::from_json

TEST(parcel, optional_from_json_null_yields_nullopt) {
    const parcel::json_t j = nullptr;
    const auto opt = j.get<std::optional<int>>();
    EXPECT_FALSE(opt.has_value());
}

TEST(parcel, optional_from_json_value_yields_some) {
    const parcel::json_t j = 42;
    const auto opt = j.get<std::optional<int>>();
    ASSERT_TRUE(opt.has_value());
    EXPECT_EQ(*opt, 42);
}

// ---------------------------------------------------------------------------
// 128-bit zero serialization (json.h)

#ifdef COMMONS_HAS_INT128

TEST(parcel, u128_zero_serializes_to_zero_string) {
    const parcel::json_t j = parcel::u128{0};
    ASSERT_TRUE(j.is_string());
    EXPECT_EQ(j.get<std::string>(), "0");
}

TEST(parcel, i128_zero_serializes_to_zero_string) {
    const parcel::json_t j = parcel::i128{0};
    ASSERT_TRUE(j.is_string());
    EXPECT_EQ(j.get<std::string>(), "0");
}

TEST(parcel, i128_from_json_rejects_non_string) {
    parcel::json_t j = 42;
    parcel::i128 out;
    // commons' decoder surfaces failures as an nlohmann JSON error (other_error).
    EXPECT_THROW(j.get_to(out), parcel::json_t::exception);
}

TEST(parcel, i128_from_json_rejects_empty_string) {
    parcel::json_t j = "";
    parcel::i128 out;
    EXPECT_THROW(j.get_to(out), parcel::json_t::exception);
}

TEST(parcel, i128_from_json_accepts_positive_sign) {
    const parcel::json_t j = "+12345";
    parcel::i128 out{0};
    j.get_to(out);
    EXPECT_EQ(out, parcel::i128{12345});
}

TEST(parcel, i128_from_json_rejects_non_digit) {
    parcel::json_t j = "12x4";
    parcel::i128 out;
    EXPECT_THROW(j.get_to(out), parcel::json_t::exception);
}

#endif  // COMMONS_HAS_INT128

// ---------------------------------------------------------------------------
// DisplayInfo round-trip through nlohmann::adl

TEST(parcel, display_info_from_json_round_trips_all_fields) {
    const parcel::DisplayInfo original{
        .name = "Demo",
        .description = "desc",
        .icon = comms::Icon::from("mdi:information"),
        .color = comms::Color::parse("red"),
    };
    // DisplayInfo has no member to_json(); it serializes via free ADL functions.
    const auto j = parcel::json_t(original);

    const auto restored = j.get<parcel::DisplayInfo>();

    EXPECT_EQ(restored.name, "Demo");
    ASSERT_TRUE(restored.description.has_value());
    EXPECT_EQ(*restored.description, "desc");
    ASSERT_TRUE(restored.icon.has_value());
    EXPECT_EQ(restored.icon, comms::Icon::from("mdi:information"));
    ASSERT_TRUE(restored.color.has_value());
    EXPECT_EQ(restored.color, comms::Color::parse("red"));
}

TEST(parcel, display_info_from_json_partial_input_leaves_others_default) {
    const parcel::json_t j = {{"name", "Only"}};
    const auto restored = j.get<parcel::DisplayInfo>();
    EXPECT_EQ(restored.name, "Only");
    EXPECT_FALSE(restored.description.has_value());
    EXPECT_FALSE(restored.icon.has_value());
    EXPECT_FALSE(restored.color.has_value());
}

TEST(ParcelError, to_string_invalid_json_renders_code_prefix) {
    const auto e = parcel::ParcelError::make(parcel::ParcelError::Code::InvalidJson, "boom");
    EXPECT_EQ(e.to_string(), "InvalidJson: boom");
}

TEST(ParcelError, to_string_kind_mismatch_renders_code_prefix) {
    const auto e = parcel::ParcelError::make(parcel::ParcelError::Code::KindMismatch, "boom");
    EXPECT_EQ(e.to_string(), "KindMismatch: boom");
}

TEST(ParcelError, to_string_unknown_kind_renders_code_prefix) {
    const auto e = parcel::ParcelError::make(parcel::ParcelError::Code::UnknownKind, "boom");
    EXPECT_EQ(e.to_string(), "UnknownKind: boom");
}

TEST(ParcelError, to_string_missing_field_renders_code_prefix) {
    const auto e = parcel::ParcelError::make(parcel::ParcelError::Code::MissingField, "boom");
    EXPECT_EQ(e.to_string(), "MissingField: boom");
}

TEST(ParcelError, to_string_type_error_renders_code_prefix) {
    const auto e = parcel::ParcelError::make(parcel::ParcelError::Code::TypeError, "boom");
    EXPECT_EQ(e.to_string(), "TypeError: boom");
}

TEST(ParcelError, to_string_omits_kind_and_field_when_empty) {
    const auto e = parcel::ParcelError::make(parcel::ParcelError::Code::InvalidJson, "boom");
    const auto out = e.to_string();
    EXPECT_EQ(out.find("kind="), std::string::npos);
    EXPECT_EQ(out.find("field="), std::string::npos);
}

TEST(ParcelError, to_string_appends_kind_suffix) {
    const auto e =
        parcel::ParcelError::make(parcel::ParcelError::Code::KindMismatch, "boom", "i32");
    EXPECT_EQ(e.to_string(), "KindMismatch: boom [kind=i32]");
}

TEST(ParcelError, to_string_appends_field_suffix) {
    const auto e =
        parcel::ParcelError::make(parcel::ParcelError::Code::MissingField, "boom", "", "name");
    EXPECT_EQ(e.to_string(), "MissingField: boom [field=name]");
}

TEST(ParcelError, to_string_appends_both_kind_and_field_suffix) {
    const auto e =
        parcel::ParcelError::make(parcel::ParcelError::Code::TypeError, "boom", "i32", "name");
    EXPECT_EQ(e.to_string(), "TypeError: boom [kind=i32] [field=name]");
}

TEST(ParcelError, make_with_code_and_message_only) {
    const auto [code, message, kind, field] =
        parcel::ParcelError::make(parcel::ParcelError::Code::InvalidJson, "msg");
    EXPECT_EQ(code, parcel::ParcelError::Code::InvalidJson);
    EXPECT_EQ(message, "msg");
    EXPECT_TRUE(kind.empty());
    EXPECT_TRUE(field.empty());
}

TEST(ParcelError, make_with_code_message_and_kind) {
    const auto [code, message, kind, field] =
        parcel::ParcelError::make(parcel::ParcelError::Code::UnknownKind, "msg", "i32");
    EXPECT_EQ(code, parcel::ParcelError::Code::UnknownKind);
    EXPECT_EQ(message, "msg");
    EXPECT_EQ(kind, "i32");
    EXPECT_TRUE(field.empty());
}

TEST(ParcelError, make_with_all_four_arguments) {
    const auto [code, message, kind, field] = parcel::ParcelError::make(
        parcel::ParcelError::Code::MissingField, "msg", "struct:Foo", "name");
    EXPECT_EQ(code, parcel::ParcelError::Code::MissingField);
    EXPECT_EQ(message, "msg");
    EXPECT_EQ(kind, "struct:Foo");
    EXPECT_EQ(field, "name");
}
