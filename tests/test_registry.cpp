#include <parcel/parcel.h>

#include <gtest/gtest.h>

#include <algorithm>
#include <map>
#include <set>
#include <string>
#include <string_view>
#include <typeindex>
#include <typeinfo>
#include <variant>
#include <vector>

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
    [[maybe_unused]] static auto field_descriptors() {
        return parcel::FieldsBuilder<Address>{}
            .field<&Address::city, parcel::StringCell>("city")
            .field<&Address::zip, parcel::I32Cell>("zip")
            .build();
    }
};

}  // namespace

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
            .field<&Person::name>("name")
            .field<&Person::home>("home")
            .field<&Person::scores>("scores")
            .field<&Person::tags>("tags")
            .build();
    }
};

parcel::ParcelRegistry basic_registry() {
    // Opt out of auto-populated builtins so this fixture's count_and_contains
    // assertions stay tight against the four kinds it explicitly registers.
    parcel::ParcelRegistry reg(
        {.primitives = false, .collections = false, .typed_collections = false, .std = false});
    reg.register_kind(parcel::I32Cell::descriptor());
    reg.register_kind(parcel::StringCell::descriptor());
    reg.register_kind(parcel::U8Cell::descriptor());
    reg.register_kind(parcel::TypedListCell<parcel::I32Cell>::descriptor());
    return reg;
}

parcel::ParcelRegistry person_registry() {
    // Primitive typed list/map are auto-populated by the default ctor.
    parcel::ParcelRegistry reg;
    reg.register_kind(AddressCell::descriptor());
    reg.register_kind(PersonCell::descriptor());
    return reg;
}

parcel::ParcelRegistry composite_registry() {
    // typed_collections=false so find_by_category(TypedList/TypedMap) tests
    // can assert exactly the manually-registered instantiations.
    parcel::ParcelRegistry reg({.typed_collections = false});
    reg.register_kind(parcel::TypedListCell<parcel::I32Cell>::descriptor());
    reg.register_kind(parcel::TypedMapCell<parcel::StringCell>::descriptor());
    reg.register_kind((parcel::UnionCell<parcel::I32Cell, parcel::StringCell>::descriptor()));
    reg.register_kind(AddressCell::descriptor());
    return reg;
}

struct Pair {
    std::int32_t a{0};
    std::int32_t b{0};
};

class PairCell : public parcel::StructCell<PairCell, Pair, "pair"> {
public:
    using StructCell::StructCell;
    [[maybe_unused]] static parcel::descriptor::MetaInfo meta_info() {
        return {.name = "Pair"};
    }
    [[maybe_unused]] static auto field_descriptors() {
        return parcel::FieldsBuilder<Pair>{}
            .field<&Pair::a, parcel::I32Cell>("a")
            .field<&Pair::b, parcel::I32Cell>("b")
            .build();
    }
};

bool contains_kind(std::vector<parcel::cell_type_descriptor_t> const& v, std::string_view k) {
    return std::ranges::any_of(v, [&](auto const& d) { return d->kind() == k; });
}

}  // namespace

// ---------------------------------------------------------------------------
// Enumeration

TEST(Registry, all_returns_all_registered_kinds) {
    const auto reg = basic_registry();
    const auto descs = reg.all();
    EXPECT_EQ(descs.size(), 4u);

    std::set<std::string> kinds;
    for (auto const& d : descs) {
        kinds.emplace(d->kind());
    }
    EXPECT_TRUE(kinds.contains("i32"));
    EXPECT_TRUE(kinds.contains("string"));
    EXPECT_TRUE(kinds.contains("u8"));
    EXPECT_TRUE(kinds.contains("l:i32"));
}

TEST(Registry, kinds_matches_all) {
    const auto reg = basic_registry();
    const auto descs = reg.all();
    const auto ks = reg.kinds();
    ASSERT_EQ(descs.size(), ks.size());
    for (std::size_t i = 0; i < descs.size(); ++i) {
        EXPECT_EQ(descs[i]->kind(), ks[i]);
    }
}

TEST(Registry, count_and_contains) {
    const auto reg = basic_registry();
    EXPECT_EQ(reg.count(), 4u);
    EXPECT_TRUE(reg.contains("i32"));
    EXPECT_TRUE(reg.contains("string"));
    EXPECT_FALSE(reg.contains("nope"));
    EXPECT_FALSE(reg.contains(""));
}

TEST(Registry, duplicate_register_overwrites_without_growing) {
    parcel::ParcelRegistry reg(
        {.primitives = false, .collections = false, .typed_collections = false, .std = false});
    reg.register_kind(parcel::I32Cell::descriptor());
    reg.register_kind(parcel::StringCell::descriptor());
    EXPECT_EQ(reg.count(), 2u);

    reg.register_kind(parcel::I32Cell::descriptor());
    EXPECT_EQ(reg.count(), 2u);
    EXPECT_EQ(reg.all().size(), 2u);
}

TEST(Registry, find_returns_nullptr_for_unknown_kind) {
    const auto reg = basic_registry();
    EXPECT_EQ(reg.find("nope"), nullptr);
    EXPECT_EQ(reg.find(""), nullptr);
}

TEST(Registry, kinds_returned_in_sorted_order) {
    parcel::ParcelRegistry reg(
        {.primitives = false, .collections = false, .typed_collections = false, .std = false});
    reg.register_kind(parcel::U8Cell::descriptor());
    reg.register_kind(parcel::StringCell::descriptor());
    reg.register_kind(parcel::I32Cell::descriptor());
    const auto ks = reg.kinds();
    ASSERT_EQ(ks.size(), 3u);
    EXPECT_EQ(ks[0], "i32");
    EXPECT_EQ(ks[1], "string");
    EXPECT_EQ(ks[2], "u8");
}

// ---------------------------------------------------------------------------
// ISubTypes

TEST(Registry, sub_kinds_on_typed_list) {
    const auto d = parcel::TypedListCell<parcel::I32Cell>::descriptor();
    auto const* st = dynamic_cast<parcel::ISubTypes const*>(d.get());
    ASSERT_NE(st, nullptr);
    const auto subs = st->sub_kinds();
    ASSERT_EQ(subs.size(), 1u);
    EXPECT_EQ(subs[0], "i32");
}

TEST(Registry, sub_kinds_on_typed_map) {
    const auto d = parcel::TypedMapCell<parcel::StringCell>::descriptor();
    auto const* st = dynamic_cast<parcel::ISubTypes const*>(d.get());
    ASSERT_NE(st, nullptr);
    const auto subs = st->sub_kinds();
    ASSERT_EQ(subs.size(), 1u);
    EXPECT_EQ(subs[0], "string");
}

TEST(Registry, sub_kinds_on_union_in_template_order) {
    const auto d = parcel::UnionCell<parcel::I32Cell, parcel::StringCell>::descriptor();
    auto const* st = dynamic_cast<parcel::ISubTypes const*>(d.get());
    ASSERT_NE(st, nullptr);
    const auto subs = st->sub_kinds();
    ASSERT_EQ(subs.size(), 2u);
    EXPECT_EQ(subs[0], "i32");
    EXPECT_EQ(subs[1], "string");
}

TEST(Registry, sub_kinds_on_struct_in_field_order) {
    const auto d = PersonCell::descriptor();
    auto const* st = dynamic_cast<parcel::ISubTypes const*>(d.get());
    ASSERT_NE(st, nullptr);
    const auto subs = st->sub_kinds();
    ASSERT_EQ(subs.size(), 4u);
    EXPECT_EQ(subs[0], "string");
    EXPECT_EQ(subs[1], "s:address");
    EXPECT_EQ(subs[2], "l:i32");
    EXPECT_EQ(subs[3], "m:string");
}

TEST(Registry, primitive_descriptor_does_not_implement_isubtypes) {
    const auto d = parcel::I32Cell::descriptor();
    EXPECT_EQ(dynamic_cast<parcel::ISubTypes const*>(d.get()), nullptr);
}

TEST(Registry, isubtypes_absent_on_heterogeneous_list_and_dict) {
    EXPECT_EQ(dynamic_cast<parcel::ISubTypes const*>(parcel::ListCell::descriptor().get()),
              nullptr);
    EXPECT_EQ(dynamic_cast<parcel::ISubTypes const*>(parcel::MapCell::descriptor().get()), nullptr);
}

// ---------------------------------------------------------------------------
// Category & storage

TEST(Registry, category_virtual_returns_expected_enum) {
    EXPECT_EQ(parcel::I32Cell::descriptor()->category(),
              parcel::descriptor::CellCategory::Primitive);
    EXPECT_EQ(parcel::TypedListCell<parcel::I32Cell>::descriptor()->category(),
              parcel::descriptor::CellCategory::TypedList);
    EXPECT_EQ(parcel::TypedMapCell<parcel::StringCell>::descriptor()->category(),
              parcel::descriptor::CellCategory::TypedMap);
    EXPECT_EQ((parcel::UnionCell<parcel::I32Cell, parcel::StringCell>::descriptor()->category()),
              parcel::descriptor::CellCategory::Union);
    EXPECT_EQ(AddressCell::descriptor()->category(), parcel::descriptor::CellCategory::Struct);
}

TEST(Registry, storage_type_for_primitives) {
    EXPECT_EQ(parcel::I32Cell::descriptor()->storage_type(), std::type_index(typeid(std::int32_t)));
    EXPECT_EQ(parcel::U8Cell::descriptor()->storage_type(), std::type_index(typeid(std::uint8_t)));
    EXPECT_EQ(parcel::StringCell::descriptor()->storage_type(),
              std::type_index(typeid(std::string)));
}

TEST(Registry, find_by_category_primitive) {
    const auto reg = basic_registry();
    const auto prims = reg.find_by_category(parcel::descriptor::CellCategory::Primitive);
    EXPECT_EQ(prims.size(), 3u);
    EXPECT_TRUE(contains_kind(prims, "i32"));
    EXPECT_TRUE(contains_kind(prims, "string"));
    EXPECT_TRUE(contains_kind(prims, "u8"));
    EXPECT_FALSE(contains_kind(prims, "l:i32"));
}

TEST(Registry, find_by_category_for_each_composite) {
    auto reg = composite_registry();

    auto lists = reg.find_by_category(parcel::descriptor::CellCategory::TypedList);
    ASSERT_EQ(lists.size(), 1u);
    EXPECT_EQ(lists[0]->kind(), "l:i32");

    auto dicts = reg.find_by_category(parcel::descriptor::CellCategory::TypedMap);
    ASSERT_EQ(dicts.size(), 1u);
    EXPECT_EQ(dicts[0]->kind(), "m:string");

    auto unions = reg.find_by_category(parcel::descriptor::CellCategory::Union);
    ASSERT_EQ(unions.size(), 1u);
    EXPECT_EQ(unions[0]->kind(), "u:i32,string");

    auto structs = reg.find_by_category(parcel::descriptor::CellCategory::Struct);
    ASSERT_EQ(structs.size(), 1u);
    EXPECT_EQ(structs[0]->kind(), "s:address");
}

TEST(Registry, storage_type_for_composites) {
    EXPECT_EQ(parcel::TypedListCell<parcel::I32Cell>::descriptor()->storage_type(),
              std::type_index(typeid(std::vector<std::int32_t>)));
    EXPECT_EQ(parcel::TypedMapCell<parcel::StringCell>::descriptor()->storage_type(),
              std::type_index(typeid(std::map<std::string, std::string>)));
    EXPECT_EQ(
        (parcel::UnionCell<parcel::I32Cell, parcel::StringCell>::descriptor()->storage_type()),
        std::type_index(typeid(std::variant<std::int32_t, std::string>)));
    EXPECT_EQ(AddressCell::descriptor()->storage_type(), std::type_index(typeid(Address)));
}

TEST(Registry, descriptor_to_json_includes_category_for_composites) {
    auto prim_j = parcel::I32Cell::descriptor()->to_json();
    ASSERT_TRUE(prim_j.contains("category"));
    EXPECT_EQ(prim_j["category"].get<std::string>(), "primitive");

    auto list_j = parcel::TypedListCell<parcel::I32Cell>::descriptor()->to_json();
    ASSERT_TRUE(list_j.contains("category"));
    EXPECT_EQ(list_j["category"].get<std::string>(), "typed_list");

    auto dict_j = parcel::TypedMapCell<parcel::StringCell>::descriptor()->to_json();
    ASSERT_TRUE(dict_j.contains("category"));
    EXPECT_EQ(dict_j["category"].get<std::string>(), "typed_map");

    auto union_j = parcel::UnionCell<parcel::I32Cell, parcel::StringCell>::descriptor()->to_json();
    ASSERT_TRUE(union_j.contains("category"));
    EXPECT_EQ(union_j["category"].get<std::string>(), "union");
}

TEST(Registry, find_by_storage_template_for_uint8) {
    const auto reg = basic_registry();
    const auto matches = reg.find_by_storage<std::uint8_t>();
    ASSERT_EQ(matches.size(), 1u);
    EXPECT_EQ(matches[0]->kind(), "u8");
}

TEST(Registry, find_by_storage_template_for_string) {
    const auto reg = basic_registry();
    const auto matches = reg.find_by_storage<std::string>();
    ASSERT_EQ(matches.size(), 1u);
    EXPECT_EQ(matches[0]->kind(), "string");
}

TEST(Registry, find_by_storage_non_template_form) {
    const auto reg = basic_registry();
    const auto matches = reg.find_by_storage(std::type_index(typeid(std::int32_t)));
    ASSERT_EQ(matches.size(), 1u);
    EXPECT_EQ(matches[0]->kind(), "i32");
}

TEST(Registry, find_by_storage_with_no_matches_returns_empty) {
    const auto reg = basic_registry();
    const auto matches =
        reg.find_by_storage<double>();  // no double-storage primitive registered here
    EXPECT_TRUE(matches.empty());
}

// ---------------------------------------------------------------------------
// cell_from_json dispatch

TEST(Registry, cell_from_json_dispatches_to_registered_kind) {
    const auto reg = basic_registry();
    const parcel::I32Cell original{42};
    const auto restored = reg.cell_from_json(original.to_json());
    ASSERT_NE(restored, nullptr);
    auto* typed = dynamic_cast<parcel::I32Cell*>(restored.get());
    ASSERT_NE(typed, nullptr);
    EXPECT_EQ(typed->value, 42);
}

TEST(Registry, cell_from_json_rejects_non_object) {
    auto reg = basic_registry();
    parcel::json_t arr = parcel::json_t::array();
    EXPECT_THROW((void)reg.cell_from_json(arr), std::runtime_error);
}

TEST(Registry, cell_from_json_rejects_non_string_kind) {
    auto reg = basic_registry();
    parcel::json_t j = {{"k", 42}, {"v", 0}};
    EXPECT_THROW((void)reg.cell_from_json(j), std::runtime_error);
}

TEST(Registry, cell_from_json_rejects_unknown_kind) {
    auto reg = basic_registry();
    parcel::json_t j = {{"k", "nope"}, {"v", 0}};
    EXPECT_THROW((void)reg.cell_from_json(j), std::runtime_error);
}

// ---------------------------------------------------------------------------
// define (transitive closure)

TEST(Registry, define_on_primitive_has_empty_referenced) {
    const auto reg = basic_registry();
    const auto [root, referenced] = reg.define("i32");
    ASSERT_NE(root, nullptr);
    EXPECT_EQ(root->kind(), "i32");
    EXPECT_TRUE(referenced.empty());
}

TEST(Registry, define_on_typed_list_includes_only_element_kind) {
    const auto reg = basic_registry();
    const auto [root, referenced] = reg.define("l:i32");
    ASSERT_NE(root, nullptr);
    EXPECT_EQ(root->kind(), "l:i32");
    ASSERT_EQ(referenced.size(), 1u);
    EXPECT_TRUE(referenced.contains("i32"));
}

TEST(Registry, define_on_typed_map_includes_only_element_kind) {
    const parcel::ParcelRegistry reg;
    const auto [root, referenced] = reg.define("m:string");
    ASSERT_NE(root, nullptr);
    EXPECT_EQ(root->kind(), "m:string");
    ASSERT_EQ(referenced.size(), 1u);
    EXPECT_TRUE(referenced.contains("string"));
}

TEST(Registry, define_on_union_includes_alternative_kinds) {
    parcel::ParcelRegistry reg;
    reg.register_kind((parcel::UnionCell<parcel::I32Cell, parcel::StringCell>::descriptor()));
    const auto [root, referenced] = reg.define("u:i32,string");
    ASSERT_NE(root, nullptr);
    EXPECT_EQ(root->kind(), "u:i32,string");
    ASSERT_EQ(referenced.size(), 2u);
    EXPECT_TRUE(referenced.contains("i32"));
    EXPECT_TRUE(referenced.contains("string"));
}

TEST(Registry, define_dedups_when_struct_has_two_fields_of_same_kind) {
    parcel::ParcelRegistry reg;
    reg.register_kind(PairCell::descriptor());
    const auto [root, referenced] = reg.define("s:pair");
    ASSERT_NE(root, nullptr);
    EXPECT_EQ(root->kind(), "s:pair");
    ASSERT_EQ(referenced.size(), 1u);
    EXPECT_TRUE(referenced.contains("i32"));
}

TEST(Registry, define_on_struct_includes_transitive_closure) {
    const auto reg = person_registry();
    auto [root, referenced] = reg.define("s:person");
    ASSERT_NE(root, nullptr);
    EXPECT_EQ(root->kind(), "s:person");

    std::set<std::string> ref_keys;
    for (const auto& k : referenced | std::views::keys) {
        ref_keys.insert(k);
    }
    EXPECT_EQ(ref_keys, (std::set<std::string>{"s:address", "string", "i32", "l:i32", "m:string"}));
}

TEST(Registry, define_excludes_root_from_referenced) {
    const auto reg = person_registry();
    const auto [root, referenced] = reg.define("s:person");
    EXPECT_FALSE(referenced.contains("s:person"));
}

namespace {

// Diamond schema: HouseCell references AddressCell via two separate paths
// (a direct field + a TypedListCell field). The transitive walk must visit
// "s:address" exactly once. This pins the `referenced.contains(k)` guard at
// registry.h that prevents the queue from re-adding kinds already resolved
// — the realistic stand-in for a true cycle, since C++'s type system rules
// out genuinely self-referential typed-cell schemas without indirection.
struct House {
    Address primary{};
    std::vector<Address> alternates;
};

class HouseCell : public parcel::StructCell<HouseCell, House, "house"> {
public:
    using StructCell::StructCell;
    [[maybe_unused]] static auto field_descriptors() {
        return parcel::FieldsBuilder<House>{}
            .field<&House::primary>("primary")
            .field<&House::alternates>("alternates")
            .build();
    }
};

}  // namespace

TEST(Registry, define_diamond_visits_each_referenced_kind_once) {
    parcel::ParcelRegistry reg(
        {.primitives = true, .collections = false, .typed_collections = false, .std = false});
    reg.register_kind(AddressCell::descriptor());
    reg.register_kind(HouseCell::descriptor());
    reg.register_kind(parcel::TypedListCell<AddressCell>::descriptor());

    const auto [root, referenced] = reg.define("s:house");
    ASSERT_NE(root, nullptr);
    EXPECT_EQ(root->kind(), "s:house");

    // Both paths to AddressCell collapse to a single entry in `referenced`.
    EXPECT_EQ(referenced.count("s:address"), 1u);

    // And the typed-list-of-address kind appears once even though its sub
    // (s:address) re-enqueues it.
    EXPECT_EQ(referenced.count("l:s:address"), 1u);
}

// ----- cyclic graph: synthesize descriptors directly to fabricate a cycle ---

namespace {

class FakeDescriptor : public parcel::ICellTypeDescriptor, public parcel::ISubTypes {
    std::string kind_;
    std::vector<std::string_view> subs_;

public:
    FakeDescriptor(std::string k, std::vector<std::string_view> subs)
        : kind_(std::move(k)), subs_(std::move(subs)) {}

    [[nodiscard]] std::string_view kind() const override {
        return kind_;
    }
    [[nodiscard]] parcel::descriptor::MetaInfo meta() const override {
        return {};
    }
    [[nodiscard]] parcel::descriptor::CellCategory category() const override {
        return parcel::descriptor::CellCategory::Custom;
    }
    [[nodiscard]] std::type_index storage_type() const override {
        return std::type_index(typeid(void));
    }
    [[nodiscard]] parcel::cell_t cell_from_json(parcel::json_t const&,
                                                parcel::ParcelRegistry const&) const override {
        return nullptr;
    }
    [[nodiscard]] parcel::json_t to_json() const override {
        return parcel::json_t{{"kind", kind_}};
    }
    [[nodiscard]] std::vector<std::string_view> sub_kinds() const override {
        return subs_;
    }
};

}  // namespace

TEST(Registry, define_on_cyclic_graph_terminates) {
    parcel::ParcelRegistry reg;
    // a -> b, b -> a — pure cycle, fully synthetic.
    reg.register_kind(std::make_shared<FakeDescriptor>("a", std::vector<std::string_view>{"b"}));
    reg.register_kind(std::make_shared<FakeDescriptor>("b", std::vector<std::string_view>{"a"}));

    const auto [root, referenced] = reg.define("a");
    ASSERT_NE(root, nullptr);
    EXPECT_EQ(root->kind(), "a");
    EXPECT_TRUE(referenced.contains("b"));
    EXPECT_FALSE(referenced.contains("a"));  // root not duplicated
    EXPECT_EQ(referenced.size(), 1u);
}

TEST(Registry, define_self_reference_excluded_from_referenced) {
    parcel::ParcelRegistry reg;
    // selfish references itself — the root-skip path must keep it out of `referenced`.
    reg.register_kind(
        std::make_shared<FakeDescriptor>("selfish", std::vector<std::string_view>{"selfish"}));
    const auto [root, referenced] = reg.define("selfish");
    EXPECT_EQ(root->kind(), "selfish");
    EXPECT_TRUE(referenced.empty());
}

TEST(Registry, define_unknown_kind_throws) {
    auto reg = basic_registry();
    EXPECT_THROW((void)reg.define("does_not_exist"), std::runtime_error);
}

TEST(Registry, define_referenced_unregistered_throws) {
    // Opt out so the only kinds present are the ones explicitly added below;
    // PersonCell references string/l:i32/m:string/address and only i32/person
    // exist, so define("person") must throw.
    parcel::ParcelRegistry reg(
        {.primitives = false, .collections = false, .typed_collections = false, .std = false});
    reg.register_kind(parcel::I32Cell::descriptor());
    reg.register_kind(PersonCell::descriptor());
    EXPECT_THROW((void)reg.define("s:person"), std::runtime_error);
}

TEST(Registry, definition_to_json_shape) {
    auto reg = basic_registry();
    auto def = reg.define("l:i32");
    auto j = def.to_json();
    ASSERT_TRUE(j.is_object());
    ASSERT_TRUE(j.contains("root"));
    ASSERT_TRUE(j.contains("referenced"));

    EXPECT_EQ(j["root"]["kind"].get<std::string>(), "l:i32");

    ASSERT_TRUE(j["referenced"].is_object());
    ASSERT_TRUE(j["referenced"].contains("i32"));
    EXPECT_EQ(j["referenced"]["i32"]["kind"].get<std::string>(), "i32");
}

TEST(Registry, definition_to_json_multi_entry_referenced_shape) {
    auto reg = person_registry();
    auto def = reg.define("s:person");
    auto j = def.to_json();
    ASSERT_TRUE(j.is_object());
    ASSERT_TRUE(j["root"].is_object());
    EXPECT_EQ(j["root"]["kind"].get<std::string>(), "s:person");

    ASSERT_TRUE(j["referenced"].is_object());
    for (auto const* key : {"s:address", "string", "i32", "l:i32", "m:string"}) {
        ASSERT_TRUE(j["referenced"].contains(key)) << "missing referenced key: " << key;
        ASSERT_TRUE(j["referenced"][key].is_object());
        ASSERT_TRUE(j["referenced"][key].contains("kind"))
            << "missing 'kind' in referenced[" << key << "]";
        EXPECT_EQ(j["referenced"][key]["kind"].get<std::string>(), key);
    }
}

// ---------------------------------------------------------------------------
// Auto-populated builtins via the default ctor / BuiltinsOptions.

TEST(Registry, default_ctor_registers_builtins) {
    const parcel::ParcelRegistry reg;
    EXPECT_TRUE(reg.contains("i32"));
    EXPECT_TRUE(reg.contains("string"));
    EXPECT_TRUE(reg.contains("l"));
    EXPECT_TRUE(reg.contains("m"));
    EXPECT_TRUE(reg.contains("l:i32"));
    EXPECT_TRUE(reg.contains("m:string"));
}

TEST(Registry, opt_out_yields_empty_registry) {
    const parcel::ParcelRegistry reg(
        {.primitives = false, .collections = false, .typed_collections = false, .std = false});
    EXPECT_EQ(reg.count(), 0u);
}

TEST(Registry, partial_opt_out_keeps_primitives_only) {
    const parcel::ParcelRegistry reg(
        {.primitives = true, .collections = false, .typed_collections = false});
    EXPECT_TRUE(reg.contains("i32"));
    EXPECT_FALSE(reg.contains("l"));
    EXPECT_FALSE(reg.contains("m"));
    EXPECT_FALSE(reg.contains("l:i32"));
    EXPECT_FALSE(reg.contains("m:string"));
}

TEST(Registry, typed_collections_opt_out_excludes_typed_list_and_map) {
    const parcel::ParcelRegistry reg({.typed_collections = false});
    EXPECT_TRUE(reg.contains("i32"));
    EXPECT_TRUE(reg.contains("l"));
    EXPECT_TRUE(reg.contains("m"));
    EXPECT_FALSE(reg.contains("l:i32"));
    EXPECT_FALSE(reg.contains("m:string"));
}

// ---------------------------------------------------------------------------
// try_cell_from_json maps each error class to the right ParcelError::Code.

TEST(TryCellFromJson, invalid_json_when_root_is_not_object) {
    const auto reg = person_registry();
    const parcel::json_t arr = parcel::json_t::array();
    const auto result = reg.try_cell_from_json(arr);
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, parcel::ParcelError::Code::InvalidJson);
}

TEST(TryCellFromJson, invalid_json_when_k_missing) {
    const auto reg = person_registry();
    const parcel::json_t j = {{"v", 1}};
    const auto result = reg.try_cell_from_json(j);
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, parcel::ParcelError::Code::InvalidJson);
}

TEST(TryCellFromJson, unknown_kind_is_unknown_kind_code) {
    const auto reg = person_registry();
    const parcel::json_t j = {{"k", "no_such_kind"}, {"v", 0}};
    const auto result = reg.try_cell_from_json(j);
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, parcel::ParcelError::Code::UnknownKind);
    EXPECT_EQ(result.error().kind, "no_such_kind");
}

TEST(TryCellFromJson, kind_mismatch_is_kind_mismatch_code) {
    // Wrong kind on a primitive: registered kind dispatches to I32Cell::from_json,
    // which sees "k": "string" and throws KindMismatchException.
    const auto reg = person_registry();
    parcel::json_t j = {{"k", "i32"}, {"v", parcel::json_t{{"k", "string"}, {"v", "x"}}}};
    // Construct a struct payload that triggers the inner kind-mismatch:
    parcel::json_t inner_field = {{"k", "string"}, {"v", "Ada"}};
    parcel::json_t v = parcel::json_t::object();
    // 'name' is a StringCell; supplying an i32 kind triggers a kind mismatch
    // during inner deserialization.
    v["name"] = parcel::json_t{{"k", "i32"}, {"v", 42}};
    v["home"] = parcel::json_t{{"k", "s:address"}, {"v", parcel::json_t::object()}};
    v["scores"] = parcel::json_t{{"k", "l:i32"}, {"v", parcel::json_t::array()}};
    v["tags"] = parcel::json_t{{"k", "m:string"}, {"v", parcel::json_t::object()}};
    const parcel::json_t payload = {{"k", "s:person"}, {"v", std::move(v)}};
    const auto result = reg.try_cell_from_json(payload);
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, parcel::ParcelError::Code::KindMismatch);
}

TEST(TryCellFromJson, missing_required_field_is_missing_field_code) {
    const auto reg = person_registry();
    // PersonCell requires "name", "home", "scores", "tags". Drop "name".
    parcel::json_t v = parcel::json_t::object();
    v["home"] = parcel::json_t{{"k", "s:address"}, {"v", parcel::json_t::object()}};
    v["scores"] = parcel::json_t{{"k", "l:i32"}, {"v", parcel::json_t::array()}};
    v["tags"] = parcel::json_t{{"k", "m:string"}, {"v", parcel::json_t::object()}};
    const parcel::json_t payload = {{"k", "s:person"}, {"v", std::move(v)}};
    const auto result = reg.try_cell_from_json(payload);
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, parcel::ParcelError::Code::MissingField);
    EXPECT_EQ(result.error().field, "name");
}

TEST(TryCellFromJson, union_non_object_v_yields_invalid_json) {
    parcel::ParcelRegistry reg;
    using IntStr = parcel::UnionCell<parcel::I32Cell, parcel::StringCell>;
    reg.register_kind(IntStr::descriptor());
    // 'v' is an array — UnionCell::from_json rejects with InvalidJson before
    // any kind dispatch.
    const parcel::json_t bad = {{"k", IntStr::kind_id}, {"v", parcel::json_t::array({1, 2})}};
    const auto result = reg.try_cell_from_json(bad);
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, parcel::ParcelError::Code::InvalidJson);
}

TEST(TryCellFromJson, union_active_kind_outside_alternatives_yields_kind_mismatch) {
    parcel::ParcelRegistry reg;
    using IntStr = parcel::UnionCell<parcel::I32Cell, parcel::StringCell>;
    reg.register_kind(IntStr::descriptor());
    // Inner kind 'f64' is not one of {"i32","string"}.
    const parcel::json_t inner = {{"k", "f64"}, {"v", 1.0}};
    const parcel::json_t bad = {{"k", IntStr::kind_id}, {"v", inner}};
    const auto result = reg.try_cell_from_json(bad);
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, parcel::ParcelError::Code::KindMismatch);
}

TEST(TryCellFromJson, i128_non_string_value_yields_invalid_json) {
    parcel::ParcelRegistry reg;
    reg.register_kind(parcel::I128Cell::descriptor());
    // i128 must be a decimal string; passing a number trips the
    // adl_serializer<__int128_t>::from_json check, which now throws
    // InvalidJsonException with kind="i128".
    const parcel::json_t bad = {{"k", "i128"}, {"v", 42}};
    const auto result = reg.try_cell_from_json(bad);
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, parcel::ParcelError::Code::InvalidJson);
}
