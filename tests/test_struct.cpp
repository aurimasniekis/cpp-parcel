#include <parcel/parcel.h>

#include <gtest/gtest.h>

#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace {

struct Action {
    std::int32_t code{0};
    std::string name;
    bool urgent{false};
};

class ActionCell : public parcel::StructCell<ActionCell, Action, "action"> {
public:
    using StructCell::StructCell;

    [[maybe_unused]] static parcel::descriptor::MetaInfo meta_info() {
        return {.name = "Action", .description = "An action"};
    }

    [[maybe_unused]] static auto field_descriptors() {
        return parcel::FieldsBuilder<Action>{}
            .field<&Action::code>("code")
            .name("Code")
            .description("Action code")
            .field<&Action::name>("name")
            .name("Name")
            .field<&Action::urgent>("urgent")
            .name("Urgent")
            .build();
    }
};

struct Note {
    std::int32_t id{0};
    std::string body;
};

class NoteCell : public parcel::StructCell<NoteCell, Note, "note"> {
public:
    using StructCell::StructCell;

    [[maybe_unused]] static parcel::descriptor::MetaInfo meta_info() {
        return {.name = "Note"};
    }

    [[maybe_unused]] static auto field_descriptors() {
        return parcel::FieldsBuilder<Note>{}
            .field<&Note::id, parcel::I32Cell>("id")
            .name("ID")
            .field<&Note::body, parcel::StringCell>("body")
            .name("Body")
            .optional()
            .build();
    }
};

class LenientActionCell : public parcel::StructCell<LenientActionCell, Action, "lenient_action"> {
public:
    using StructCell::StructCell;

    [[maybe_unused]] static constexpr bool allow_extra_fields = true;

    [[maybe_unused]] static parcel::descriptor::MetaInfo meta_info() {
        return {.name = "LenientAction"};
    }

    [[maybe_unused]] static auto field_descriptors() {
        return parcel::FieldsBuilder<Action>{}
            .field<&Action::code, parcel::I32Cell>("code")
            .field<&Action::name, parcel::StringCell>("name")
            .field<&Action::urgent, parcel::BoolCell>("urgent")
            .build();
    }
};

parcel::ParcelRegistry const& populated_registry() {
    static const parcel::ParcelRegistry r = [] {
        parcel::ParcelRegistry reg;
        reg.register_kind(ActionCell::descriptor());
        reg.register_kind(NoteCell::descriptor());
        reg.register_kind(LenientActionCell::descriptor());
        return reg;
    }();
    return r;
}

}  // namespace

// ---------------------------------------------------------------------------
// Concept gate

static_assert(parcel::CellLike<ActionCell>);
static_assert(parcel::CellLike<NoteCell>);
static_assert(parcel::CellLike<LenientActionCell>);

// ---------------------------------------------------------------------------
// Construction

TEST(StructCell, default_constructs_with_default_payload) {
    const ActionCell v;
    EXPECT_EQ(v.value.code, 0);
    EXPECT_TRUE(v.value.name.empty());
    EXPECT_FALSE(v.value.urgent);
}

TEST(StructCell, constructs_from_payload) {
    const ActionCell v{Action{42, "go", true}};
    EXPECT_EQ(v.value.code, 42);
    EXPECT_EQ(v.value.name, "go");
    EXPECT_TRUE(v.value.urgent);
}

TEST(StructCell, copy_constructs) {
    const ActionCell a{Action{1, "x", false}};
    const ActionCell b{a};
    EXPECT_EQ(b.value.code, 1);
    EXPECT_EQ(b.value.name, "x");
}

// ---------------------------------------------------------------------------
// kind()

TEST(StructCell, kind_matches_derived_kind_id) {
    EXPECT_EQ(ActionCell{}.kind(), "s:action");
    EXPECT_EQ(NoteCell{}.kind(), "s:note");
    EXPECT_EQ(LenientActionCell{}.kind(), "s:lenient_action");
}

// ---------------------------------------------------------------------------
// to_string()

TEST(StructCell, to_string_includes_all_fields_in_declaration_order) {
    const ActionCell v{Action{42, "go", true}};
    EXPECT_EQ(v.to_string(), "{code: 42, name: go, urgent: true}");
}

// ---------------------------------------------------------------------------
// to_json() shape

TEST(StructCell, to_json_emits_kind_and_object_value) {
    ActionCell v{Action{42, "go", true}};
    const auto j = v.to_json();
    ASSERT_TRUE(j.is_object());
    EXPECT_EQ(j["k"].get<std::string>(), "s:action");
    ASSERT_TRUE(j["v"].is_object());
    ASSERT_TRUE(j["v"].contains("code"));
    ASSERT_TRUE(j["v"]["code"].is_object());
    EXPECT_EQ(j["v"]["code"]["k"].get<std::string>(), "i32");
    EXPECT_EQ(j["v"]["code"]["v"].get<int>(), 42);
    EXPECT_EQ(j["v"]["name"]["k"].get<std::string>(), "string");
    EXPECT_EQ(j["v"]["name"]["v"].get<std::string>(), "go");
    EXPECT_EQ(j["v"]["urgent"]["k"].get<std::string>(), "bool");
    EXPECT_EQ(j["v"]["urgent"]["v"].get<bool>(), true);
}

// ---------------------------------------------------------------------------
// from_json() round-trip

TEST(StructCell, from_json_round_trip) {
    const ActionCell original{Action{99, "alpha", true}};
    const auto j = original.to_json();
    const auto restored = ActionCell::from_json(j, populated_registry());
    ASSERT_NE(restored, nullptr);
    EXPECT_EQ(restored->kind(), "s:action");
    auto* typed = dynamic_cast<ActionCell*>(restored.get());
    ASSERT_NE(typed, nullptr);
    EXPECT_EQ(typed->value.code, 99);
    EXPECT_EQ(typed->value.name, "alpha");
    EXPECT_TRUE(typed->value.urgent);
}

// ---------------------------------------------------------------------------
// from_json() error paths

TEST(StructCell, from_json_rejects_non_object_root) {
    parcel::json_t arr = parcel::json_t::array({1, 2});
    EXPECT_THROW(ActionCell::from_json(arr, populated_registry()), std::runtime_error);
}

TEST(StructCell, from_json_rejects_missing_kind) {
    parcel::json_t j = {{"v", parcel::json_t::object()}};
    EXPECT_THROW(ActionCell::from_json(j, populated_registry()), std::runtime_error);
}

TEST(StructCell, from_json_rejects_wrong_kind) {
    parcel::json_t j = {{"k", "s:note"}, {"v", parcel::json_t::object()}};
    EXPECT_THROW(ActionCell::from_json(j, populated_registry()), std::runtime_error);
}

TEST(StructCell, from_json_rejects_missing_v) {
    parcel::json_t j = {{"k", "s:action"}};
    EXPECT_THROW(ActionCell::from_json(j, populated_registry()), std::runtime_error);
}

TEST(StructCell, from_json_rejects_v_not_object) {
    parcel::json_t j = {{"k", "s:action"}, {"v", parcel::json_t::array({1})}};
    EXPECT_THROW(ActionCell::from_json(j, populated_registry()), std::runtime_error);
}

TEST(StructCell, from_json_rejects_missing_required_field) {
    parcel::json_t v = parcel::json_t::object();
    v["code"] = parcel::json_t{{"k", "i32"}, {"v", 1}};
    v["urgent"] = parcel::json_t{{"k", "bool"}, {"v", true}};
    parcel::json_t j = {{"k", "s:action"}, {"v", std::move(v)}};
    EXPECT_THROW(ActionCell::from_json(j, populated_registry()), std::runtime_error);
}

TEST(StructCell, from_json_rejects_field_with_mismatched_inner_kind) {
    parcel::json_t v = parcel::json_t::object();
    v["code"] = parcel::json_t{{"k", "string"}, {"v", "not-an-int"}};
    v["name"] = parcel::json_t{{"k", "string"}, {"v", "go"}};
    v["urgent"] = parcel::json_t{{"k", "bool"}, {"v", true}};
    parcel::json_t j = {{"k", "s:action"}, {"v", std::move(v)}};
    EXPECT_THROW(ActionCell::from_json(j, populated_registry()), std::runtime_error);
}

// ---------------------------------------------------------------------------
// Strict vs lenient on unknown extras

TEST(StructCell, strict_mode_rejects_unknown_field) {
    parcel::json_t v = parcel::json_t::object();
    v["code"] = parcel::json_t{{"k", "i32"}, {"v", 1}};
    v["name"] = parcel::json_t{{"k", "string"}, {"v", "go"}};
    v["urgent"] = parcel::json_t{{"k", "bool"}, {"v", true}};
    v["extra"] = parcel::json_t{{"k", "i32"}, {"v", 7}};
    parcel::json_t j = {{"k", "s:action"}, {"v", std::move(v)}};
    EXPECT_THROW(ActionCell::from_json(j, populated_registry()), std::runtime_error);
}

TEST(StructCell, lenient_mode_captures_unknown_fields_in_extras) {
    parcel::json_t v = parcel::json_t::object();
    v["code"] = parcel::json_t{{"k", "i32"}, {"v", 1}};
    v["name"] = parcel::json_t{{"k", "string"}, {"v", "go"}};
    v["urgent"] = parcel::json_t{{"k", "bool"}, {"v", true}};
    v["extra_count"] = parcel::json_t{{"k", "i32"}, {"v", 7}};
    v["extra_label"] = parcel::json_t{{"k", "string"}, {"v", "tag"}};
    parcel::json_t j = {{"k", "s:lenient_action"}, {"v", std::move(v)}};

    auto restored = LenientActionCell::from_json(j, populated_registry());
    auto* typed = dynamic_cast<LenientActionCell*>(restored.get());
    ASSERT_NE(typed, nullptr);
    EXPECT_EQ(typed->value.code, 1);
    EXPECT_EQ(typed->value.name, "go");
    EXPECT_TRUE(typed->value.urgent);

    ASSERT_EQ(typed->extras.size(), 2u);
    ASSERT_TRUE(typed->extras.contains("extra_count"));
    auto* count = dynamic_cast<parcel::I32Cell*>(typed->extras["extra_count"].get());
    ASSERT_NE(count, nullptr);
    EXPECT_EQ(count->get(), 7);

    ASSERT_TRUE(typed->extras.contains("extra_label"));
    auto* label = dynamic_cast<parcel::StringCell*>(typed->extras["extra_label"].get());
    ASSERT_NE(label, nullptr);
    EXPECT_EQ(label->get(), "tag");
}

TEST(StructCell, lenient_mode_round_trips_extras_in_to_json) {
    parcel::json_t v = parcel::json_t::object();
    v["code"] = parcel::json_t{{"k", "i32"}, {"v", 1}};
    v["name"] = parcel::json_t{{"k", "string"}, {"v", "go"}};
    v["urgent"] = parcel::json_t{{"k", "bool"}, {"v", true}};
    v["extra_count"] = parcel::json_t{{"k", "i32"}, {"v", 7}};
    const parcel::json_t j = {{"k", "s:lenient_action"}, {"v", std::move(v)}};

    const auto restored = LenientActionCell::from_json(j, populated_registry());
    auto* typed = dynamic_cast<LenientActionCell*>(restored.get());
    ASSERT_NE(typed, nullptr);

    const auto j2 = typed->to_json();
    ASSERT_TRUE(j2["v"].contains("extra_count"));
    EXPECT_EQ(j2["v"]["extra_count"]["k"].get<std::string>(), "i32");
    EXPECT_EQ(j2["v"]["extra_count"]["v"].get<int>(), 7);
}

// ---------------------------------------------------------------------------
// Optional fields

TEST(StructCell, optional_field_can_be_omitted) {
    parcel::json_t v = parcel::json_t::object();
    v["id"] = parcel::json_t{{"k", "i32"}, {"v", 5}};
    const parcel::json_t j = {{"k", "s:note"}, {"v", std::move(v)}};

    const auto restored = NoteCell::from_json(j, populated_registry());
    auto* typed = dynamic_cast<NoteCell*>(restored.get());
    ASSERT_NE(typed, nullptr);
    EXPECT_EQ(typed->value.id, 5);
    EXPECT_TRUE(typed->value.body.empty());
}

TEST(StructCell, optional_field_round_trips_when_present) {
    const NoteCell original{Note{5, "hello"}};
    const auto j = original.to_json();
    const auto restored = NoteCell::from_json(j, populated_registry());
    auto* typed = dynamic_cast<NoteCell*>(restored.get());
    ASSERT_NE(typed, nullptr);
    EXPECT_EQ(typed->value.id, 5);
    EXPECT_EQ(typed->value.body, "hello");
}

// ---------------------------------------------------------------------------
// clone()

TEST(StructCell, clone_produces_independent_copy) {
    ActionCell v{Action{42, "go", true}};
    const auto cloned = v.clone();
    ASSERT_NE(cloned, nullptr);
    auto* typed = dynamic_cast<ActionCell*>(cloned.get());
    ASSERT_NE(typed, nullptr);
    EXPECT_EQ(typed->value.code, 42);

    v.value.code = 999;
    EXPECT_EQ(typed->value.code, 42);
}

TEST(StructCell, clone_deep_copies_lenient_extras) {
    parcel::json_t v = parcel::json_t::object();
    v["code"] = parcel::json_t{{"k", "i32"}, {"v", 1}};
    v["name"] = parcel::json_t{{"k", "string"}, {"v", "go"}};
    v["urgent"] = parcel::json_t{{"k", "bool"}, {"v", true}};
    v["extra_count"] = parcel::json_t{{"k", "i32"}, {"v", 7}};
    const parcel::json_t j = {{"k", "s:lenient_action"}, {"v", std::move(v)}};

    const auto restored = LenientActionCell::from_json(j, populated_registry());
    auto* original = dynamic_cast<LenientActionCell*>(restored.get());
    ASSERT_NE(original, nullptr);

    const auto cloned = original->clone();
    auto* typed = dynamic_cast<LenientActionCell*>(cloned.get());
    ASSERT_NE(typed, nullptr);
    ASSERT_EQ(typed->extras.size(), 1u);
    EXPECT_NE(typed->extras["extra_count"].get(), original->extras["extra_count"].get());

    auto* original_count = dynamic_cast<parcel::I32Cell*>(original->extras["extra_count"].get());
    ASSERT_NE(original_count, nullptr);
    original_count->value = 9999;

    auto* clone_count = dynamic_cast<parcel::I32Cell*>(typed->extras["extra_count"].get());
    ASSERT_NE(clone_count, nullptr);
    EXPECT_EQ(clone_count->get(), 7);
}

// ---------------------------------------------------------------------------
// descriptor()

TEST(StructCell, descriptor_returns_non_null_and_kind_matches) {
    const auto d = ActionCell::descriptor();
    ASSERT_NE(d, nullptr);
    EXPECT_EQ(d->kind(), "s:action");
}

TEST(StructCell, descriptor_is_cached) {
    const auto d1 = ActionCell::descriptor();
    const auto d2 = ActionCell::descriptor();
    EXPECT_EQ(d1.get(), d2.get());
}

TEST(StructCell, descriptor_to_json_includes_kind_meta_category_and_fields) {
    auto d = ActionCell::descriptor();
    const auto j = d->to_json();
    EXPECT_EQ(j["kind"].get<std::string>(), "s:action");
    ASSERT_TRUE(j.contains("meta"));
    EXPECT_EQ(j["meta"]["name"].get<std::string>(), "Action");
    EXPECT_EQ(j["category"].get<std::string>(), "struct");
    ASSERT_TRUE(j.contains("fields"));
    ASSERT_TRUE(j["fields"].is_array());
    ASSERT_EQ(j["fields"].size(), 3u);
    EXPECT_EQ(j["fields"][0]["key"].get<std::string>(), "code");
    EXPECT_EQ(j["fields"][0]["kind"].get<std::string>(), "i32");
    EXPECT_TRUE(j["fields"][0]["required"].get<bool>());
    EXPECT_EQ(j["fields"][1]["key"].get<std::string>(), "name");
    EXPECT_EQ(j["fields"][1]["kind"].get<std::string>(), "string");
    EXPECT_EQ(j["fields"][2]["key"].get<std::string>(), "urgent");
    EXPECT_EQ(j["fields"][2]["kind"].get<std::string>(), "bool");
}

TEST(StructCell, descriptor_field_meta_carries_builder_values) {
    const auto d = ActionCell::descriptor();
    const auto j = d->to_json();
    EXPECT_EQ(j["fields"][0]["meta"]["name"].get<std::string>(), "Code");
    EXPECT_EQ(j["fields"][0]["meta"]["description"].get<std::string>(), "Action code");
    EXPECT_EQ(j["fields"][1]["meta"]["name"].get<std::string>(), "Name");
}

TEST(StructCell, descriptor_optional_field_marks_required_false) {
    const auto d = NoteCell::descriptor();
    const auto j = d->to_json();
    ASSERT_EQ(j["fields"].size(), 2u);
    EXPECT_TRUE(j["fields"][0]["required"].get<bool>());
    EXPECT_EQ(j["fields"][1]["key"].get<std::string>(), "body");
    EXPECT_FALSE(j["fields"][1]["required"].get<bool>());
}

TEST(StructCell, descriptor_payload_fields_exposes_typed_list) {
    const auto d = ActionCell::descriptor();
    auto* typed = dynamic_cast<parcel::StructCellTypeDescriptor<ActionCell, Action>*>(d.get());
    ASSERT_NE(typed, nullptr);
    ASSERT_EQ(typed->payload_fields().size(), 3u);
    EXPECT_EQ(typed->payload_fields()[0]->key(), "code");
    EXPECT_EQ(typed->payload_fields()[0]->kind(), "i32");
}

TEST(StructCell, descriptor_implements_IHasFields) {
    const auto d = ActionCell::descriptor();
    auto* hf = dynamic_cast<parcel::IHasFields*>(d.get());
    ASSERT_NE(hf, nullptr);
    const auto map = hf->fields();
    EXPECT_EQ(map.size(), 3u);
    ASSERT_TRUE(map.contains("code"));
    EXPECT_EQ(map.at("code")->kind(), "i32");
    ASSERT_TRUE(map.contains("name"));
    EXPECT_EQ(map.at("name")->kind(), "string");
    ASSERT_TRUE(map.contains("urgent"));
    EXPECT_EQ(map.at("urgent")->kind(), "bool");
}

// ---------------------------------------------------------------------------
// Registry round-trip

TEST(StructCell, lenient_mode_to_string_includes_extras) {
    parcel::json_t v = parcel::json_t::object();
    v["code"] = parcel::json_t{{"k", "i32"}, {"v", 1}};
    v["name"] = parcel::json_t{{"k", "string"}, {"v", "go"}};
    v["urgent"] = parcel::json_t{{"k", "bool"}, {"v", true}};
    v["tag"] = parcel::json_t{{"k", "string"}, {"v", "alpha"}};
    const parcel::json_t j = {{"k", "s:lenient_action"}, {"v", std::move(v)}};

    const auto restored = LenientActionCell::from_json(j, populated_registry());
    auto* typed = dynamic_cast<LenientActionCell*>(restored.get());
    ASSERT_NE(typed, nullptr);

    const auto s = typed->to_string();
    EXPECT_NE(s.find("tag: alpha"), std::string::npos)
        << "lenient to_string must include extras: " << s;
    EXPECT_NE(s.find("code: 1"), std::string::npos);
}

TEST(StructCell, lenient_mode_to_string_renders_null_extra_as_placeholder) {
    LenientActionCell v;
    v.extras.emplace("ghost", nullptr);
    EXPECT_NE(v.to_string().find("ghost: <null>"), std::string::npos);
}

TEST(StructCell, lenient_mode_to_json_emits_null_for_null_extra) {
    LenientActionCell v;
    v.extras.emplace("ghost", nullptr);
    const auto j = v.to_json();
    ASSERT_TRUE(j["v"].contains("ghost"));
    EXPECT_TRUE(j["v"]["ghost"].is_null());
}

TEST(StructCell, member_field_descriptor_meta_returns_builder_values) {
    const auto d = ActionCell::descriptor();
    auto* hf = dynamic_cast<parcel::IHasFields*>(d.get());
    ASSERT_NE(hf, nullptr);
    const auto map = hf->fields();

    const auto code_meta = map.at("code")->meta();
    EXPECT_EQ(code_meta.name, "Code");
    ASSERT_TRUE(code_meta.description.has_value());
    EXPECT_EQ(*code_meta.description, "Action code");

    const auto name_meta = map.at("name")->meta();
    EXPECT_EQ(name_meta.name, "Name");
}

TEST(StructCell, registry_dispatches_by_kind) {
    const ActionCell original{Action{1, "boot", false}};
    const auto j = original.to_json();
    const auto restored = populated_registry().cell_from_json(j);
    ASSERT_NE(restored, nullptr);
    auto* typed = dynamic_cast<ActionCell*>(restored.get());
    ASSERT_NE(typed, nullptr);
    EXPECT_EQ(typed->value.code, 1);
    EXPECT_EQ(typed->value.name, "boot");
    EXPECT_FALSE(typed->value.urgent);
}

// ---------------------------------------------------------------------------
// FieldsBuilder modifier-without-field guard

TEST(FieldsBuilder, modifier_before_first_field_throws) {
    parcel::FieldsBuilder<Action> b;
    EXPECT_THROW(b.name("X"), std::runtime_error);
    EXPECT_THROW(b.is_required(false), std::runtime_error);
}

// ===========================================================================
// default_cell_for inference: nested struct + vector + map
// ===========================================================================

namespace {

struct Address {
    std::string city;
    std::int32_t zip{0};
};

class AddressCell : public parcel::StructCell<AddressCell, Address, "address"> {
public:
    using StructCell::StructCell;
    [[maybe_unused]] static parcel::descriptor::MetaInfo meta_info() {
        return {.name = "Address"};
    }
    [[maybe_unused]] [[maybe_unused]] static auto field_descriptors() {
        return parcel::FieldsBuilder<Address>{}
            .field<&Address::city>("city")
            .field<&Address::zip>("zip")
            .build();
    }
};

}  // namespace

// Specialize the inference trait for the user struct.
template <>
struct parcel::default_cell_for<Address> {
    using type = AddressCell;
};

namespace {

struct Person {
    std::string name;
    Address home;
    std::vector<std::int32_t> scores;
    std::map<std::string, std::string> tags;
};

class PersonCell : public parcel::StructCell<PersonCell, Person, "person"> {
public:
    using StructCell::StructCell;
    [[maybe_unused]] static parcel::descriptor::MetaInfo meta_info() {
        return {.name = "Person"};
    }
    [[maybe_unused]] static auto field_descriptors() {
        return parcel::FieldsBuilder<Person>{}
            .field<&Person::name>("name")      // string → StringCell
            .field<&Person::home>("home")      // Address → AddressCell (user-specialized)
            .field<&Person::scores>("scores")  // vector<int32_t> → TypedListCell<I32Cell>
            .field<&Person::tags>("tags")      // map<string,string> → TypedMapCell<StringCell>
            .build();
    }
};

parcel::ParcelRegistry const& nested_registry() {
    static const parcel::ParcelRegistry r = [] {
        parcel::ParcelRegistry reg;
        reg.register_kind(AddressCell::descriptor());
        reg.register_kind(PersonCell::descriptor());
        return reg;
    }();
    return r;
}

}  // namespace

static_assert(parcel::CellLike<AddressCell>);
static_assert(parcel::CellLike<PersonCell>);

static_assert(std::is_same_v<parcel::default_cell_for_t<Address>, AddressCell>);
static_assert(std::is_same_v<parcel::default_cell_for_t<std::vector<std::int32_t>>,
                             parcel::TypedListCell<parcel::I32Cell>>);
static_assert(std::is_same_v<parcel::default_cell_for_t<std::map<std::string, std::string>>,
                             parcel::TypedMapCell<parcel::StringCell>>);

TEST(StructCellInference, nested_struct_descriptor_kind_is_inferred) {
    const auto d = PersonCell::descriptor();
    const auto j = d->to_json();
    ASSERT_TRUE(j.contains("fields"));
    ASSERT_EQ(j["fields"].size(), 4u);
    EXPECT_EQ(j["fields"][1]["key"].get<std::string>(), "home");
    EXPECT_EQ(j["fields"][1]["kind"].get<std::string>(), "s:address");
    EXPECT_EQ(j["fields"][2]["key"].get<std::string>(), "scores");
    EXPECT_EQ(j["fields"][2]["kind"].get<std::string>(), "l:i32");
    EXPECT_EQ(j["fields"][3]["key"].get<std::string>(), "tags");
    EXPECT_EQ(j["fields"][3]["kind"].get<std::string>(), "m:string");
}

TEST(StructCellInference, nested_struct_round_trip) {
    Person p;
    p.name = "alice";
    p.home = Address{"NYC", 10001};
    p.scores = {1, 2, 3};
    p.tags["role"] = "admin";
    p.tags["env"] = "prod";

    PersonCell original{p};
    const auto j = original.to_json();

    EXPECT_EQ(j["v"]["home"]["k"].get<std::string>(), "s:address");
    EXPECT_EQ(j["v"]["home"]["v"]["city"]["v"].get<std::string>(), "NYC");
    EXPECT_EQ(j["v"]["scores"]["k"].get<std::string>(), "l:i32");
    EXPECT_EQ(j["v"]["tags"]["k"].get<std::string>(), "m:string");

    auto restored = PersonCell::from_json(j, nested_registry());
    auto* typed = dynamic_cast<PersonCell*>(restored.get());
    ASSERT_NE(typed, nullptr);
    EXPECT_EQ(typed->value.name, "alice");
    EXPECT_EQ(typed->value.home.city, "NYC");
    EXPECT_EQ(typed->value.home.zip, 10001);
    ASSERT_EQ(typed->value.scores.size(), 3u);
    EXPECT_EQ(typed->value.scores[0], 1);
    EXPECT_EQ(typed->value.scores[2], 3);
    ASSERT_EQ(typed->value.tags.size(), 2u);
    EXPECT_EQ(typed->value.tags.at("role"), "admin");
    EXPECT_EQ(typed->value.tags.at("env"), "prod");
}

TEST(StructCellInference, registry_dispatches_nested_struct) {
    Person p;
    p.name = "bob";
    p.home = Address{"SF", 94016};
    p.scores = {7};

    PersonCell original{p};
    auto restored = nested_registry().cell_from_json(original.to_json());
    auto* typed = dynamic_cast<PersonCell*>(restored.get());
    ASSERT_NE(typed, nullptr);
    EXPECT_EQ(typed->value.home.city, "SF");
    EXPECT_EQ(typed->value.home.zip, 94016);
    ASSERT_EQ(typed->value.scores.size(), 1u);
    EXPECT_EQ(typed->value.scores[0], 7);
}

// ===========================================================================
// std::optional<T> field inference
// ===========================================================================

static_assert(std::is_same_v<parcel::default_cell_for_t<std::optional<std::int32_t>>,
                             parcel::PrimitiveCell<std::int32_t>>);
static_assert(std::is_same_v<parcel::default_cell_for_t<std::optional<std::vector<std::int32_t>>>,
                             parcel::TypedListCell<parcel::PrimitiveCell<std::int32_t>>>);

namespace {

struct Profile {
    std::optional<std::string> nickname;
    std::optional<std::int32_t> age;
};

class ProfileCell : public parcel::StructCell<ProfileCell, Profile, "profile"> {
public:
    using StructCell::StructCell;
    [[maybe_unused]] static parcel::descriptor::MetaInfo meta_info() {
        return {.name = "Profile"};
    }
    [[maybe_unused]] static auto field_descriptors() {
        return parcel::FieldsBuilder<Profile>{}
            .field<&Profile::nickname>("nickname")
            .field<&Profile::age>("age")
            .build();
    }
};

struct ProfileRequired {
    std::optional<std::string> nickname;
};

class ProfileRequiredCell
    : public parcel::StructCell<ProfileRequiredCell, ProfileRequired, "profile_required"> {
public:
    using StructCell::StructCell;
    [[maybe_unused]] static parcel::descriptor::MetaInfo meta_info() {
        return {.name = "ProfileRequired"};
    }
    [[maybe_unused]] static auto field_descriptors() {
        return parcel::FieldsBuilder<ProfileRequired>{}
            .field<&ProfileRequired::nickname>("nickname")
            .is_required(true)
            .build();
    }
};

struct OptScores {
    std::optional<std::vector<std::int32_t>> scores;
};

class OptScoresCell : public parcel::StructCell<OptScoresCell, OptScores, "opt_scores"> {
public:
    using StructCell::StructCell;
    [[maybe_unused]] static parcel::descriptor::MetaInfo meta_info() {
        return {.name = "OptScores"};
    }
    [[maybe_unused]] static auto field_descriptors() {
        return parcel::FieldsBuilder<OptScores>{}.field<&OptScores::scores>("scores").build();
    }
};

parcel::ParcelRegistry const& optional_registry() {
    static const parcel::ParcelRegistry r = [] {
        parcel::ParcelRegistry reg;
        reg.register_kind(ProfileCell::descriptor());
        reg.register_kind(ProfileRequiredCell::descriptor());
        reg.register_kind(OptScoresCell::descriptor());
        return reg;
    }();
    return r;
}

}  // namespace

static_assert(parcel::CellLike<ProfileCell>);

TEST(StructCellOptional, descriptor_marks_optional_fields_not_required) {
    const auto d = ProfileCell::descriptor();
    const auto j = d->to_json();
    ASSERT_EQ(j["fields"].size(), 2u);
    EXPECT_EQ(j["fields"][0]["key"].get<std::string>(), "nickname");
    EXPECT_EQ(j["fields"][0]["kind"].get<std::string>(), "string");
    EXPECT_FALSE(j["fields"][0]["required"].get<bool>());
    EXPECT_EQ(j["fields"][1]["key"].get<std::string>(), "age");
    EXPECT_EQ(j["fields"][1]["kind"].get<std::string>(), "i32");
    EXPECT_FALSE(j["fields"][1]["required"].get<bool>());
}

TEST(StructCellOptional, round_trip_with_both_present) {
    Profile p;
    p.nickname = "alice";
    p.age = 30;
    ProfileCell original{p};
    const auto j = original.to_json();

    ASSERT_TRUE(j["v"].contains("nickname"));
    EXPECT_EQ(j["v"]["nickname"]["k"].get<std::string>(), "string");
    EXPECT_EQ(j["v"]["nickname"]["v"].get<std::string>(), "alice");
    ASSERT_TRUE(j["v"].contains("age"));
    EXPECT_EQ(j["v"]["age"]["k"].get<std::string>(), "i32");
    EXPECT_EQ(j["v"]["age"]["v"].get<int>(), 30);

    auto restored = ProfileCell::from_json(j, optional_registry());
    auto* typed = dynamic_cast<ProfileCell*>(restored.get());
    ASSERT_NE(typed, nullptr);
    ASSERT_TRUE(typed->value.nickname.has_value());
    EXPECT_EQ(*typed->value.nickname, "alice");
    ASSERT_TRUE(typed->value.age.has_value());
    EXPECT_EQ(*typed->value.age, 30);
}

TEST(StructCellOptional, round_trip_with_both_nullopt_omits_keys) {
    const ProfileCell original{Profile{}};
    const auto j = original.to_json();

    ASSERT_TRUE(j["v"].is_object());
    EXPECT_FALSE(j["v"].contains("nickname"));
    EXPECT_FALSE(j["v"].contains("age"));

    const auto restored = ProfileCell::from_json(j, optional_registry());
    auto* typed = dynamic_cast<ProfileCell*>(restored.get());
    ASSERT_NE(typed, nullptr);
    EXPECT_FALSE(typed->value.nickname.has_value());
    EXPECT_FALSE(typed->value.age.has_value());
}

TEST(StructCellOptional, round_trip_with_mixed_presence) {
    Profile p;
    p.nickname = "bob";
    const ProfileCell original{p};
    const auto j = original.to_json();

    EXPECT_TRUE(j["v"].contains("nickname"));
    EXPECT_FALSE(j["v"].contains("age"));

    const auto restored = ProfileCell::from_json(j, optional_registry());
    auto* typed = dynamic_cast<ProfileCell*>(restored.get());
    ASSERT_NE(typed, nullptr);
    ASSERT_TRUE(typed->value.nickname.has_value());
    EXPECT_EQ(*typed->value.nickname, "bob");
    EXPECT_FALSE(typed->value.age.has_value());
}

TEST(StructCellOptional, to_string_renders_nullopt_placeholder) {
    Profile p;
    p.nickname = "x";
    const ProfileCell v{p};
    EXPECT_EQ(v.to_string(), "{nickname: x, age: <nullopt>}");
}

TEST(StructCellOptional, to_string_renders_present_value) {
    Profile p;
    p.nickname = "x";
    p.age = 7;
    const ProfileCell v{p};
    EXPECT_EQ(v.to_string(), "{nickname: x, age: 7}");
}

TEST(StructCellOptional, is_required_override_marks_descriptor_required) {
    const auto d = ProfileRequiredCell::descriptor();
    const auto j = d->to_json();
    ASSERT_EQ(j["fields"].size(), 1u);
    EXPECT_EQ(j["fields"][0]["key"].get<std::string>(), "nickname");
    EXPECT_TRUE(j["fields"][0]["required"].get<bool>());
}

TEST(StructCellOptional, is_required_override_makes_missing_field_throw) {
    parcel::json_t j = {{"k", "s:profile_required"}, {"v", parcel::json_t::object()}};
    EXPECT_THROW(ProfileRequiredCell::from_json(j, optional_registry()), std::runtime_error);
}

TEST(StructCellOptional, optional_of_vector_round_trips) {
    OptScores s;
    s.scores = std::vector<std::int32_t>{1, 2, 3};
    const OptScoresCell original{s};
    const auto j = original.to_json();

    ASSERT_TRUE(j["v"].contains("scores"));
    EXPECT_EQ(j["v"]["scores"]["k"].get<std::string>(), "l:i32");

    const auto restored = OptScoresCell::from_json(j, optional_registry());
    auto* typed = dynamic_cast<OptScoresCell*>(restored.get());
    ASSERT_NE(typed, nullptr);
    ASSERT_TRUE(typed->value.scores.has_value());
    ASSERT_EQ(typed->value.scores->size(), 3u);
    EXPECT_EQ((*typed->value.scores)[0], 1);
    EXPECT_EQ((*typed->value.scores)[2], 3);
}

TEST(StructCellOptional, optional_of_vector_nullopt_omits_key) {
    const OptScoresCell original{OptScores{}};
    const auto j = original.to_json();
    EXPECT_FALSE(j["v"].contains("scores"));

    const auto restored = OptScoresCell::from_json(j, optional_registry());
    auto* typed = dynamic_cast<OptScoresCell*>(restored.get());
    ASSERT_NE(typed, nullptr);
    EXPECT_FALSE(typed->value.scores.has_value());
}

// ---------------------------------------------------------------------------
// Defensive dynamic_cast guard in MemberFieldDescriptor::from_json_into

namespace {

// LiarCell claims to be a CellLike with kind_id "liar_s", but its from_json
// returns an I32Cell instead. Used to trigger the
// "field 'x' from_json did not return CellT" guard.
class LiarCell : public parcel::BaseCell<LiarCell, std::int32_t> {
    using base_t = parcel::BaseCell<LiarCell, std::int32_t>;

public:
    using base_t::base_t;
    using base_t::operator=;

    [[maybe_unused]] static constexpr std::string_view kind_id = "liar_s";

    [[nodiscard]] std::string to_string() const override {
        return std::to_string(this->value);
    }

    [[maybe_unused]] static parcel::cell_t from_json(parcel::json_t const&,
                                                     parcel::ParcelRegistry const&) {
        return std::make_shared<parcel::I32Cell>(0);  // wrong concrete type
    }

    [[maybe_unused]] static parcel::cell_type_descriptor_t descriptor() {
        static const auto d = std::make_shared<parcel::SimpleCellTypeDescriptor<LiarCell>>(
            parcel::descriptor::MetaInfo{.name = "LiarS"});
        return d;
    }
};

struct LyingPayload {
    std::int32_t x{0};
};

class LyingStructCell : public parcel::StructCell<LyingStructCell, LyingPayload, "lying_struct"> {
public:
    using StructCell::StructCell;

    [[maybe_unused]] static parcel::descriptor::MetaInfo meta_info() {
        return {.name = "LyingStruct"};
    }

    [[maybe_unused]] static auto field_descriptors() {
        return parcel::FieldsBuilder<LyingPayload>{}.field<&LyingPayload::x, LiarCell>("x").build();
    }
};

}  // namespace

TEST(StructCell, from_json_throws_when_field_dynamic_cast_fails) {
    parcel::ParcelRegistry reg;
    parcel::json_t j = {
        {"k", "s:lying_struct"},
        {"v", {{"x", {{"k", "liar_s"}, {"v", 0}}}}},
    };
    EXPECT_THROW((void)LyingStructCell::from_json(j, reg), std::runtime_error);
}

// ===========================================================================
// FieldsBuilder::extend, last-wins field<>, and remove_field
// ===========================================================================

namespace {

struct StreetAddress {
    std::string street;
    std::string city;
};

class StreetAddressCell
    : public parcel::StructCell<StreetAddressCell, StreetAddress, "street_address"> {
public:
    using StructCell::StructCell;
    [[maybe_unused]] static parcel::descriptor::MetaInfo meta_info() {
        return {.name = "StreetAddress"};
    }
    [[maybe_unused]] static auto field_descriptors() {
        return parcel::FieldsBuilder<StreetAddress>{}
            .field<&StreetAddress::street>("street")
            .name("Street")
            .field<&StreetAddress::city>("city")
            .name("City")
            .build();
    }
};

struct HomeAddress : StreetAddress {
    std::string label;
};

class HomeAddressCell : public parcel::StructCell<HomeAddressCell, HomeAddress, "home_address"> {
public:
    using StructCell::StructCell;
    [[maybe_unused]] static parcel::descriptor::MetaInfo meta_info() {
        return {.name = "HomeAddress"};
    }
    [[maybe_unused]] static auto field_descriptors() {
        return parcel::FieldsBuilder<HomeAddress>{}
            .extend<StreetAddressCell>()
            .field<&HomeAddress::label>("label")
            .name("Label")
            .build();
    }
};

// Used to verify that extend<> with an unrelated payload does not compile.
struct UnrelatedPayload {
    std::int32_t n{0};
};

class UnrelatedCell : public parcel::StructCell<UnrelatedCell, UnrelatedPayload, "unrelated"> {
public:
    using StructCell::StructCell;
    [[maybe_unused]] static parcel::descriptor::MetaInfo meta_info() {
        return {.name = "Unrelated"};
    }
    [[maybe_unused]] static auto field_descriptors() {
        return parcel::FieldsBuilder<UnrelatedPayload>{}.field<&UnrelatedPayload::n>("n").build();
    }
};

template <typename Builder, typename ParentCell>
concept HasExtend = requires(Builder b) { b.template extend<ParentCell>(); };

static_assert(HasExtend<parcel::FieldsBuilder<HomeAddress>, StreetAddressCell>,
              "extend<> must be available when parent payload is a base of Payload");
static_assert(!HasExtend<parcel::FieldsBuilder<HomeAddress>, UnrelatedCell>,
              "extend<> must not compile for an unrelated parent payload");
static_assert(!HasExtend<parcel::FieldsBuilder<StreetAddress>, HomeAddressCell>,
              "extend<> must not compile when parent payload is derived from Payload");

parcel::ParcelRegistry const& home_address_registry() {
    static const parcel::ParcelRegistry r = [] {
        parcel::ParcelRegistry reg;
        reg.register_kind(StreetAddressCell::descriptor());
        reg.register_kind(HomeAddressCell::descriptor());
        return reg;
    }();
    return r;
}

}  // namespace

static_assert(parcel::CellLike<StreetAddressCell>);
static_assert(parcel::CellLike<HomeAddressCell>);

TEST(FieldsBuilderExtend, inherited_fields_appear_in_declaration_order) {
    const auto d = HomeAddressCell::descriptor();
    const auto j = d->to_json();
    ASSERT_TRUE(j.contains("fields"));
    ASSERT_EQ(j["fields"].size(), 3u);
    EXPECT_EQ(j["fields"][0]["key"].get<std::string>(), "street");
    EXPECT_EQ(j["fields"][1]["key"].get<std::string>(), "city");
    EXPECT_EQ(j["fields"][2]["key"].get<std::string>(), "label");
    EXPECT_EQ(j["fields"][0]["kind"].get<std::string>(), "string");
    EXPECT_EQ(j["fields"][2]["kind"].get<std::string>(), "string");
}

TEST(FieldsBuilderExtend, inherited_field_meta_carries_through) {
    const auto d = HomeAddressCell::descriptor();
    const auto j = d->to_json();
    EXPECT_EQ(j["fields"][0]["meta"]["name"].get<std::string>(), "Street");
    EXPECT_EQ(j["fields"][1]["meta"]["name"].get<std::string>(), "City");
    EXPECT_EQ(j["fields"][2]["meta"]["name"].get<std::string>(), "Label");
}

TEST(FieldsBuilderExtend, round_trips_inherited_and_own_fields) {
    HomeAddress h;
    h.street = "1 Main";
    h.city = "Springfield";
    h.label = "primary";

    const HomeAddressCell original{h};
    const auto j = original.to_json();

    EXPECT_EQ(j["k"].get<std::string>(), "s:home_address");
    EXPECT_EQ(j["v"]["street"]["v"].get<std::string>(), "1 Main");
    EXPECT_EQ(j["v"]["city"]["v"].get<std::string>(), "Springfield");
    EXPECT_EQ(j["v"]["label"]["v"].get<std::string>(), "primary");

    const auto restored = HomeAddressCell::from_json(j, home_address_registry());
    auto* typed = dynamic_cast<HomeAddressCell*>(restored.get());
    ASSERT_NE(typed, nullptr);
    EXPECT_EQ(typed->value.street, "1 Main");
    EXPECT_EQ(typed->value.city, "Springfield");
    EXPECT_EQ(typed->value.label, "primary");
}

TEST(FieldsBuilderExtend, registry_dispatches_inherited_struct) {
    HomeAddress h;
    h.street = "12 Oak";
    h.city = "Shelbyville";
    h.label = "work";

    const HomeAddressCell original{h};
    const auto restored = home_address_registry().cell_from_json(original.to_json());
    auto* typed = dynamic_cast<HomeAddressCell*>(restored.get());
    ASSERT_NE(typed, nullptr);
    EXPECT_EQ(typed->value.city, "Shelbyville");
    EXPECT_EQ(typed->value.label, "work");
}

TEST(FieldsBuilderExtend, missing_required_inherited_field_throws) {
    parcel::json_t j = {
        {"k", "s:home_address"},
        {"v",
         {
             {"city", {{"k", "string"}, {"v", "Shelbyville"}}},
             {"label", {{"k", "string"}, {"v", "work"}}},
         }},
    };
    EXPECT_THROW((void)HomeAddressCell::from_json(j, home_address_registry()), std::runtime_error);
}

namespace {

// Override an inherited field by re-declaring it with the same key after extend<>.
class HomeAddressWithRequiredCityOverrideCell
    : public parcel::
          StructCell<HomeAddressWithRequiredCityOverrideCell, HomeAddress, "home_address_v2"> {
public:
    using StructCell::StructCell;
    [[maybe_unused]] static parcel::descriptor::MetaInfo meta_info() {
        return {.name = "HomeAddressV2"};
    }
    [[maybe_unused]] static auto field_descriptors() {
        return parcel::FieldsBuilder<HomeAddress>{}
            .extend<StreetAddressCell>()
            .field<&HomeAddress::city>("city")  // overrides inherited "city"
            .name("CityOverridden")
            .field<&HomeAddress::label>("label")
            .build();
    }
};

// Drop an inherited field via remove_field.
class HomeAddressWithoutCityCell
    : public parcel::StructCell<HomeAddressWithoutCityCell, HomeAddress, "home_address_no_city"> {
public:
    using StructCell::StructCell;
    [[maybe_unused]] static parcel::descriptor::MetaInfo meta_info() {
        return {.name = "HomeAddressNoCity"};
    }
    [[maybe_unused]] static auto field_descriptors() {
        return parcel::FieldsBuilder<HomeAddress>{}
            .extend<StreetAddressCell>()
            .remove_field("city")
            .field<&HomeAddress::label>("label")
            .build();
    }
};

}  // namespace

TEST(FieldsBuilderExtend, field_after_extend_overrides_inherited_in_place) {
    const auto d = HomeAddressWithRequiredCityOverrideCell::descriptor();
    const auto j = d->to_json();
    ASSERT_EQ(j["fields"].size(), 3u);
    EXPECT_EQ(j["fields"][0]["key"].get<std::string>(), "street");
    EXPECT_EQ(j["fields"][1]["key"].get<std::string>(), "city");
    EXPECT_EQ(j["fields"][1]["meta"]["name"].get<std::string>(), "CityOverridden");
    EXPECT_EQ(j["fields"][2]["key"].get<std::string>(), "label");
}

TEST(FieldsBuilderExtend, remove_field_drops_inherited_field) {
    const auto d = HomeAddressWithoutCityCell::descriptor();
    const auto j = d->to_json();
    ASSERT_EQ(j["fields"].size(), 2u);
    EXPECT_EQ(j["fields"][0]["key"].get<std::string>(), "street");
    EXPECT_EQ(j["fields"][1]["key"].get<std::string>(), "label");

    HomeAddress h;
    h.street = "1 Main";
    h.city = "ignored-on-wire";
    h.label = "primary";
    const HomeAddressWithoutCityCell original{h};
    const auto wire = original.to_json();
    EXPECT_FALSE(wire["v"].contains("city"));
    EXPECT_TRUE(wire["v"].contains("street"));
    EXPECT_TRUE(wire["v"].contains("label"));
}

TEST(FieldsBuilderExtend, modifier_after_remove_field_throws) {
    parcel::FieldsBuilder<HomeAddress> b;
    b.extend<StreetAddressCell>().remove_field("city");
    EXPECT_THROW(b.name("X"), std::runtime_error);
}

// ===========================================================================
// SelfStructCell — direct usage and CRTP intermediate
// ===========================================================================

namespace {

// (1) Direct SelfStructCell user — declares its own `kind_id` and data members,
// no intermediate base.
class MyDirectCell : public parcel::SelfStructCell<MyDirectCell> {
public:
    std::int32_t code{0};
    std::string label;

    [[maybe_unused]] static constexpr std::string_view kind_id = "s:my_direct";

    [[maybe_unused]] static parcel::descriptor::MetaInfo meta_info() {
        return {.name = "MyDirect"};
    }

    [[maybe_unused]] static auto field_descriptors() {
        return parcel::FieldsBuilder<MyDirectCell>{}
            .field<&MyDirectCell::code>("code")
            .field<&MyDirectCell::label>("label")
            .build();
    }
};

// (2) Library-style CRTP intermediate. Mirrors the BaseEvent example —
// declares its own kind_id namespace ("s:event:") via id_join_lit_v,
// owns a protected timestamp_ field, and delegates field declaration to a
// concrete-side hook.
template <typename Self, parcel::fixed_string EventId>
class BaseEvent : public parcel::SelfStructCell<Self> {
public:
    static constexpr std::string_view kind_id = parcel::id_join_lit_v<"s:event:", EventId>;

protected:
    std::int64_t timestamp_{};

public:
    [[nodiscard]] std::int64_t timestamp() const {
        return timestamp_;
    }
    void set_timestamp(const std::int64_t v) {
        timestamp_ = v;
    }

    [[maybe_unused]] static parcel::descriptor::MetaInfo meta_info() {
        return {.name = "Event"};
    }

    [[maybe_unused]] static auto field_descriptors() {
        auto builder = parcel::FieldsBuilder<Self>{};
        return Self::event_field_descriptors(builder)
            .template field<&BaseEvent::timestamp_>("timestamp")
            .build();
    }
};

class SomethingEvent : public BaseEvent<SomethingEvent, "something"> {
public:
    std::string action;
    std::int32_t weight{0};

    [[maybe_unused]] static auto&
    event_field_descriptors(parcel::FieldsBuilder<SomethingEvent>& b) {
        return b.field<&SomethingEvent::action>("action").field<&SomethingEvent::weight>("weight");
    }
};

// (3) SelfStructCell with allow_extra_fields = true.
class LenientSelfCell : public parcel::SelfStructCell<LenientSelfCell> {
public:
    std::int32_t code{0};

    [[maybe_unused]] static constexpr std::string_view kind_id = "s:lenient_self";
    [[maybe_unused]] static constexpr bool allow_extra_fields = true;

    [[maybe_unused]] static parcel::descriptor::MetaInfo meta_info() {
        return {.name = "LenientSelf"};
    }

    [[maybe_unused]] static auto field_descriptors() {
        return parcel::FieldsBuilder<LenientSelfCell>{}
            .field<&LenientSelfCell::code>("code")
            .build();
    }
};

// (4) Cell whose fields live in protected scope — the descriptor-forming
// pointer-to-member is captured inside a static member of the same class,
// so access is permitted at *formation* time even though no public setter
// exists.
class ProtectedFieldsCell : public parcel::SelfStructCell<ProtectedFieldsCell> {
protected:
    std::int32_t hidden_count_{0};
    std::string hidden_label_;

public:
    [[maybe_unused]] static constexpr std::string_view kind_id = "s:protected_fields";

    [[nodiscard]] std::int32_t hidden_count() const {
        return hidden_count_;
    }
    [[nodiscard]] std::string const& hidden_label() const {
        return hidden_label_;
    }

    [[maybe_unused]] static parcel::descriptor::MetaInfo meta_info() {
        return {.name = "ProtectedFields"};
    }

    [[maybe_unused]] static auto field_descriptors() {
        return parcel::FieldsBuilder<ProtectedFieldsCell>{}
            .field<&ProtectedFieldsCell::hidden_count_>("hidden_count")
            .field<&ProtectedFieldsCell::hidden_label_>("hidden_label")
            .build();
    }
};

parcel::ParcelRegistry const& self_struct_registry() {
    static const parcel::ParcelRegistry r = [] {
        parcel::ParcelRegistry reg;
        reg.register_kind(MyDirectCell::descriptor());
        reg.register_kind(SomethingEvent::descriptor());
        reg.register_kind(LenientSelfCell::descriptor());
        reg.register_kind(ProtectedFieldsCell::descriptor());
        return reg;
    }();
    return r;
}

}  // namespace

static_assert(parcel::CellLike<MyDirectCell>);
static_assert(parcel::CellLike<SomethingEvent>);
static_assert(parcel::CellLike<LenientSelfCell>);
static_assert(parcel::CellLike<ProtectedFieldsCell>);

// ---------------------------------------------------------------------------
// (1) Direct SelfStructCell user

TEST(SelfStructCell, direct_user_round_trips) {
    MyDirectCell original;
    original.code = 7;
    original.label = "alpha";

    const auto j = original.to_json();
    EXPECT_EQ(j["k"].get<std::string>(), "s:my_direct");
    EXPECT_EQ(j["v"]["code"]["k"].get<std::string>(), "i32");
    EXPECT_EQ(j["v"]["code"]["v"].get<int>(), 7);
    EXPECT_EQ(j["v"]["label"]["v"].get<std::string>(), "alpha");

    const auto restored = MyDirectCell::from_json(j, self_struct_registry());
    auto* typed = dynamic_cast<MyDirectCell*>(restored.get());
    ASSERT_NE(typed, nullptr);
    EXPECT_EQ(typed->code, 7);
    EXPECT_EQ(typed->label, "alpha");
}

TEST(SelfStructCell, direct_user_descriptor_exposes_fields) {
    const auto d = MyDirectCell::descriptor();
    ASSERT_NE(d, nullptr);
    EXPECT_EQ(d->kind(), "s:my_direct");
    const auto j = d->to_json();
    ASSERT_TRUE(j.contains("fields"));
    ASSERT_EQ(j["fields"].size(), 2u);
    EXPECT_EQ(j["fields"][0]["key"].get<std::string>(), "code");
    EXPECT_EQ(j["fields"][1]["key"].get<std::string>(), "label");
}

TEST(SelfStructCell, direct_user_clone_is_independent) {
    MyDirectCell original;
    original.code = 7;
    original.label = "alpha";
    const auto cloned = original.clone();
    auto* typed = dynamic_cast<MyDirectCell*>(cloned.get());
    ASSERT_NE(typed, nullptr);
    typed->code = 999;
    EXPECT_EQ(original.code, 7);
}

// ---------------------------------------------------------------------------
// (2) CRTP chain via library-style BaseEvent

TEST(SelfStructCell, crtp_event_kind_id_is_carved_namespace) {
    static_assert(SomethingEvent::kind_id == "s:event:something");
    EXPECT_EQ(SomethingEvent{}.kind(), "s:event:something");
}

TEST(SelfStructCell, crtp_event_round_trips_concrete_and_base_fields) {
    SomethingEvent ev;
    ev.action = "click";
    ev.weight = 5;
    ev.set_timestamp(1700000000);

    const auto j = ev.to_json();
    EXPECT_EQ(j["k"].get<std::string>(), "s:event:something");
    ASSERT_TRUE(j["v"].contains("action"));
    ASSERT_TRUE(j["v"].contains("weight"));
    ASSERT_TRUE(j["v"].contains("timestamp"));
    EXPECT_EQ(j["v"]["action"]["v"].get<std::string>(), "click");
    EXPECT_EQ(j["v"]["weight"]["v"].get<int>(), 5);
    EXPECT_EQ(j["v"]["timestamp"]["v"].get<std::int64_t>(), 1700000000);

    const auto restored = SomethingEvent::from_json(j, self_struct_registry());
    auto* typed = dynamic_cast<SomethingEvent*>(restored.get());
    ASSERT_NE(typed, nullptr);
    EXPECT_EQ(typed->action, "click");
    EXPECT_EQ(typed->weight, 5);
    EXPECT_EQ(typed->timestamp(), 1700000000);
}

TEST(SelfStructCell, crtp_event_field_order_matches_hook_then_base) {
    const auto d = SomethingEvent::descriptor();
    const auto j = d->to_json();
    ASSERT_EQ(j["fields"].size(), 3u);
    EXPECT_EQ(j["fields"][0]["key"].get<std::string>(), "action");
    EXPECT_EQ(j["fields"][1]["key"].get<std::string>(), "weight");
    EXPECT_EQ(j["fields"][2]["key"].get<std::string>(), "timestamp");
}

// ---------------------------------------------------------------------------
// (3) Strict / lenient extras

TEST(SelfStructCell, strict_mode_rejects_unknown_field) {
    parcel::json_t v = parcel::json_t::object();
    v["code"] = parcel::json_t{{"k", "i32"}, {"v", 1}};
    v["label"] = parcel::json_t{{"k", "string"}, {"v", "hi"}};
    v["extra"] = parcel::json_t{{"k", "i32"}, {"v", 7}};
    parcel::json_t j = {{"k", "s:my_direct"}, {"v", std::move(v)}};
    EXPECT_THROW(MyDirectCell::from_json(j, self_struct_registry()), std::runtime_error);
}

TEST(SelfStructCell, lenient_mode_captures_extras) {
    parcel::json_t v = parcel::json_t::object();
    v["code"] = parcel::json_t{{"k", "i32"}, {"v", 1}};
    v["tag"] = parcel::json_t{{"k", "string"}, {"v", "x"}};
    const parcel::json_t j = {{"k", "s:lenient_self"}, {"v", std::move(v)}};

    const auto restored = LenientSelfCell::from_json(j, self_struct_registry());
    auto* typed = dynamic_cast<LenientSelfCell*>(restored.get());
    ASSERT_NE(typed, nullptr);
    EXPECT_EQ(typed->code, 1);
    const auto& extras = typed->cell_extras();
    ASSERT_EQ(extras.size(), 1u);
    ASSERT_TRUE(extras.contains("tag"));
    auto* tag = dynamic_cast<parcel::StringCell*>(extras.at("tag").get());
    ASSERT_NE(tag, nullptr);
    EXPECT_EQ(tag->get(), "x");
}

// ---------------------------------------------------------------------------
// (4) Registry round-trip

TEST(SelfStructCell, registry_dispatches_by_kind) {
    SomethingEvent ev;
    ev.action = "boot";
    ev.weight = 1;
    ev.set_timestamp(42);

    const auto restored = self_struct_registry().cell_from_json(ev.to_json());
    auto* typed = dynamic_cast<SomethingEvent*>(restored.get());
    ASSERT_NE(typed, nullptr);
    EXPECT_EQ(typed->action, "boot");
    EXPECT_EQ(typed->weight, 1);
    EXPECT_EQ(typed->timestamp(), 42);
}

// ---------------------------------------------------------------------------
// (5) Protected data members

TEST(SelfStructCell, protected_fields_round_trip_through_json) {
    parcel::json_t v = parcel::json_t::object();
    v["hidden_count"] = parcel::json_t{{"k", "i32"}, {"v", 13}};
    v["hidden_label"] = parcel::json_t{{"k", "string"}, {"v", "shh"}};
    const parcel::json_t j = {{"k", "s:protected_fields"}, {"v", std::move(v)}};

    const auto restored = ProtectedFieldsCell::from_json(j, self_struct_registry());
    auto* typed = dynamic_cast<ProtectedFieldsCell*>(restored.get());
    ASSERT_NE(typed, nullptr);
    EXPECT_EQ(typed->hidden_count(), 13);
    EXPECT_EQ(typed->hidden_label(), "shh");

    const auto j2 = typed->to_json();
    EXPECT_EQ(j2["v"]["hidden_count"]["v"].get<int>(), 13);
    EXPECT_EQ(j2["v"]["hidden_label"]["v"].get<std::string>(), "shh");
}

// ---------------------------------------------------------------------------
// E: FieldsBuilder icon/color/description require_last + replace_or_append
// last-wins, and StructCell lenient compare branches.

namespace {

struct IconColorPayload {
    std::int32_t x{0};
};

class IconColorCell : public parcel::StructCell<IconColorCell, IconColorPayload, "icon_color"> {
public:
    using StructCell::StructCell;

    [[maybe_unused]] static parcel::descriptor::MetaInfo meta_info() {
        return {.name = "IconColor"};
    }

    [[maybe_unused]] static auto field_descriptors() {
        return parcel::FieldsBuilder<IconColorPayload>{}
            .field<&IconColorPayload::x, parcel::I32Cell>("x")
            .icon("star")
            .color("red")
            .build();
    }
};

struct DupKeyPayload {
    std::int32_t a{0};
    std::string b;
};

class DupKeyCell : public parcel::StructCell<DupKeyCell, DupKeyPayload, "dup_key"> {
public:
    using StructCell::StructCell;

    [[maybe_unused]] static parcel::descriptor::MetaInfo meta_info() {
        return {.name = "DupKey"};
    }

    [[maybe_unused]] static auto field_descriptors() {
        return parcel::FieldsBuilder<DupKeyPayload>{}
            .field<&DupKeyPayload::a, parcel::I32Cell>("k")
            .field<&DupKeyPayload::b, parcel::StringCell>("k")
            .build();
    }
};

// Sibling cell sharing the same kind id as LenientActionCell so the
// kind() comparison passes but the dynamic_cast in compare() fails.
class TwinLenientActionCell
    : public parcel::StructCell<TwinLenientActionCell, Action, "lenient_action"> {
public:
    using StructCell::StructCell;

    [[maybe_unused]] static constexpr bool allow_extra_fields = true;

    [[maybe_unused]] static parcel::descriptor::MetaInfo meta_info() {
        return {.name = "TwinLenientAction"};
    }

    [[maybe_unused]] static auto field_descriptors() {
        return parcel::FieldsBuilder<Action>{}
            .field<&Action::code, parcel::I32Cell>("code")
            .field<&Action::name, parcel::StringCell>("name")
            .field<&Action::urgent, parcel::BoolCell>("urgent")
            .build();
    }
};

}  // namespace

TEST(FieldsBuilder, icon_sets_field_meta_icon) {
    const auto d = IconColorCell::descriptor();
    const auto j = d->to_json();
    ASSERT_TRUE(j["fields"].is_array());
    ASSERT_EQ(j["fields"].size(), 1u);
    EXPECT_EQ(j["fields"][0]["meta"]["icon"].get<std::string>(), "star");
}

TEST(FieldsBuilder, color_sets_field_meta_color) {
    const auto d = IconColorCell::descriptor();
    const auto j = d->to_json();
    EXPECT_EQ(j["fields"][0]["meta"]["color"].get<std::string>(), "red");
}

TEST(FieldsBuilder, description_before_field_throws) {
    EXPECT_THROW(
        { parcel::FieldsBuilder<IconColorPayload>{}.description("x"); }, std::runtime_error);
}

TEST(FieldsBuilder, icon_before_field_throws) {
    EXPECT_THROW({ parcel::FieldsBuilder<IconColorPayload>{}.icon("x"); }, std::runtime_error);
}

TEST(FieldsBuilder, color_before_field_throws) {
    EXPECT_THROW({ parcel::FieldsBuilder<IconColorPayload>{}.color("x"); }, std::runtime_error);
}

TEST(FieldsBuilder, replace_or_append_last_wins_on_duplicate_key) {
    const auto d = DupKeyCell::descriptor();
    const auto j = d->to_json();
    ASSERT_TRUE(j["fields"].is_array());
    ASSERT_EQ(j["fields"].size(), 1u);
    EXPECT_EQ(j["fields"][0]["key"].get<std::string>(), "k");
    EXPECT_EQ(j["fields"][0]["kind"].get<std::string>(), "string");
}

TEST(StructCell, lenient_compare_with_null_extra_value) {
    LenientActionCell a;
    a.extras["ghost"] = nullptr;
    LenientActionCell b;
    b.extras["ghost"] = nullptr;
    EXPECT_EQ(a.compare(b), std::partial_ordering::equivalent);

    b.extras["ghost"] = parcel::I32Cell::of(0);
    EXPECT_NE(a.compare(b), std::partial_ordering::equivalent);
}

TEST(StructCell, lenient_compare_returns_unordered_for_unrelated_type) {
    const LenientActionCell a;
    const TwinLenientActionCell twin;
    EXPECT_EQ(a.compare(twin), std::partial_ordering::unordered);
}
