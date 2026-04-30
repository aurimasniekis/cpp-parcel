#include <parcel/parcel.h>

#include <gtest/gtest.h>

#include <array>
#include <deque>
#include <list>
#include <ranges>
#include <set>
#include <span>
#include <string>
#include <unordered_map>
#include <variant>
#include <vector>

namespace {

struct Person {
    std::int32_t id{0};
    std::string name;
};

class PersonCell : public parcel::StructCell<PersonCell, Person, "person"> {
public:
    using StructCell::StructCell;
    [[maybe_unused]] static parcel::descriptor::MetaInfo meta_info() {
        return {.name = "Person"};
    }
    [[maybe_unused]] static auto field_descriptors() {
        return parcel::FieldsBuilder<Person>{}
            .field<&Person::id>("id")
            .field<&Person::name>("name")
            .build();
    }
};

struct Address {
    std::string city;
};

class AddressCell : public parcel::StructCell<AddressCell, Address, "address"> {
public:
    using StructCell::StructCell;
    [[maybe_unused]] static parcel::descriptor::MetaInfo meta_info() {
        return {.name = "Address"};
    }
    [[maybe_unused]] static auto field_descriptors() {
        return parcel::FieldsBuilder<Address>{}.field<&Address::city>("city").build();
    }
};

}  // namespace

// ---------------------------------------------------------------------------
// B6: variadic register_kinds / register_cells

TEST(RegistryVariadic, register_kinds_via_descriptors) {
    parcel::ParcelRegistry reg{parcel::BuiltinsOptions{
        .primitives = false, .collections = false, .typed_collections = false, .std = false}};
    reg.register_kinds(PersonCell::descriptor(), AddressCell::descriptor());
    EXPECT_TRUE(reg.contains(PersonCell::kind_id));
    EXPECT_TRUE(reg.contains(AddressCell::kind_id));
}

TEST(RegistryVariadic, register_cells_via_types) {
    parcel::ParcelRegistry reg{parcel::BuiltinsOptions{
        .primitives = false, .collections = false, .typed_collections = false, .std = false}};
    reg.register_cells<PersonCell, AddressCell>();
    EXPECT_TRUE(reg.contains(PersonCell::kind_id));
    EXPECT_TRUE(reg.contains(AddressCell::kind_id));
}

TEST(RegistryVariadic, register_kinds_returns_self_for_chaining) {
    parcel::ParcelRegistry reg{parcel::BuiltinsOptions{
        .primitives = false, .collections = false, .typed_collections = false, .std = false}};
    auto& self = reg.register_kinds(PersonCell::descriptor());
    EXPECT_EQ(&self, &reg);
}

TEST(RegistryVariadic, register_kinds_rejects_null_descriptor_mid_pack) {
    parcel::ParcelRegistry reg{parcel::BuiltinsOptions{
        .primitives = false, .collections = false, .typed_collections = false, .std = false}};
    parcel::cell_type_descriptor_t null_desc;
    EXPECT_THROW(
        { reg.register_kinds(PersonCell::descriptor(), null_desc, AddressCell::descriptor()); },
        std::runtime_error);
}

TEST(RegistryVariadic, register_cells_chains_with_register_kinds) {
    parcel::ParcelRegistry reg{parcel::BuiltinsOptions{
        .primitives = false, .collections = false, .typed_collections = false, .std = false}};
    auto& self = reg.register_cells<PersonCell>().register_kinds(AddressCell::descriptor());
    EXPECT_EQ(&self, &reg);
    EXPECT_TRUE(reg.contains(PersonCell::kind_id));
    EXPECT_TRUE(reg.contains(AddressCell::kind_id));
}

// ---------------------------------------------------------------------------
// B3 / B4: UnionCell visit, get_if, holds_alternative, Overload

using IntStr = parcel::UnionCell<parcel::I32Cell, parcel::StringCell>;

TEST(UnionVisit, visit_dispatches_to_active_alternative) {
    IntStr u;
    u.value.emplace<0>(42);
    const auto result = parcel::visit(
        parcel::Overload{
            [](const std::int32_t i) -> std::string { return "int:" + std::to_string(i); },
            [](std::string const& s) -> std::string { return "string:" + s; },
        },
        u);
    EXPECT_EQ(result, "int:42");
}

TEST(UnionVisit, visit_handles_string_alternative) {
    IntStr u;
    u.value.emplace<1>(std::string("hello"));
    const auto result = parcel::visit(
        parcel::Overload{
            [](const std::int32_t i) -> std::string { return "int:" + std::to_string(i); },
            [](std::string const& s) -> std::string { return "string:" + s; },
        },
        u);
    EXPECT_EQ(result, "string:hello");
}

TEST(UnionVisit, member_visit_works_const) {
    IntStr u;
    u.value.emplace<0>(7);
    IntStr const& cu = u;
    const auto result = cu.visit([]<typename T0>(T0 const& v) -> int {
        if constexpr (std::is_same_v<std::decay_t<T0>, std::int32_t>) {
            return v + 1;
        }
        return 0;
    });
    EXPECT_EQ(result, 8);
}

TEST(UnionGetIf, get_if_index_returns_pointer_when_active) {
    IntStr u;
    u.value.emplace<0>(99);
    auto* p = u.get_if<0>();
    ASSERT_NE(p, nullptr);
    EXPECT_EQ(*p, 99);
    EXPECT_EQ(u.get_if<1>(), nullptr);
}

TEST(UnionGetIf, get_if_storage_type) {
    IntStr u;
    u.value.emplace<1>(std::string("x"));
    auto* p = u.get_if<std::string>();
    ASSERT_NE(p, nullptr);
    EXPECT_EQ(*p, "x");
    EXPECT_EQ(u.get_if<std::int32_t>(), nullptr);
}

TEST(UnionGetIf, free_get_if_handles_null_pointer) {
    IntStr* up = nullptr;
    EXPECT_EQ(parcel::get_if<0>(up), nullptr);
}

TEST(UnionGetIf, free_get_if_returns_value_when_active) {
    IntStr u;
    u.value.emplace<0>(11);
    auto* p = parcel::get_if<0>(&u);
    ASSERT_NE(p, nullptr);
    EXPECT_EQ(*p, 11);
}

TEST(UnionHolds, holds_alternative_index) {
    IntStr u;
    u.value.emplace<1>(std::string("y"));
    EXPECT_TRUE(u.holds_alternative<1>());
    EXPECT_FALSE(u.holds_alternative<0>());
}

TEST(UnionHolds, holds_storage_type) {
    IntStr u;
    u.value.emplace<0>(7);
    EXPECT_TRUE(u.holds<std::int32_t>());
    EXPECT_FALSE(u.holds<std::string>());
}

TEST(UnionFreeGet, free_get_returns_active_value) {
    IntStr u;
    u.value.emplace<0>(123);
    EXPECT_EQ(parcel::get<0>(u), 123);
    EXPECT_EQ(parcel::get<std::int32_t>(u), 123);
}

TEST(UnionFreeGet, free_get_const_overloads) {
    IntStr u;
    u.value.emplace<1>(std::string("zz"));
    IntStr const& cu = u;
    EXPECT_EQ(parcel::get<1>(cu), "zz");
    EXPECT_EQ(parcel::get<std::string>(cu), "zz");
}

TEST(UnionVisit, free_visit_const_reference) {
    IntStr u;
    u.value.emplace<1>(std::string("ok"));
    IntStr const& cu = u;
    const auto result =
        parcel::visit(parcel::Overload{
                          [](std::int32_t) -> std::string { return "int"; },
                          [](std::string const& s) -> std::string { return "string:" + s; },
                      },
                      cu);
    EXPECT_EQ(result, "string:ok");
}

TEST(UnionGetIf, free_get_if_storage_type) {
    IntStr u;
    u.value.emplace<1>(std::string("hello"));
    auto* p = parcel::get_if<std::string>(&u);
    ASSERT_NE(p, nullptr);
    EXPECT_EQ(*p, "hello");
    EXPECT_EQ(parcel::get_if<std::int32_t>(&u), nullptr);
}

TEST(UnionHolds, default_constructed_holds_index_zero) {
    const IntStr u;
    EXPECT_TRUE(u.holds_alternative<0>());
    EXPECT_TRUE(u.holds<std::int32_t>());
    EXPECT_FALSE(u.holds<std::string>());
}

TEST(UnionVisit, overload_with_generic_lambda_handles_every_alternative) {
    IntStr u;
    u.value.emplace<0>(7);
    auto stringify = []<typename T0>(T0 const& v) -> std::string {
        if constexpr (std::is_same_v<std::decay_t<T0>, std::int32_t>) {
            return "int:" + std::to_string(v);
        } else {
            return "string:" + v;
        }
    };
    EXPECT_EQ(parcel::visit(parcel::Overload{stringify}, u), "int:7");
    u.value.emplace<1>(std::string("hi"));
    EXPECT_EQ(parcel::visit(parcel::Overload{stringify}, u), "string:hi");
}

// ---------------------------------------------------------------------------
// A4: span / from_range_t constructors and as_span on TypedListCell

TEST(TypedListSpan, from_range_constructor) {
    std::vector<std::int32_t> source{1, 2, 3, 4};
    parcel::I32ListCell list(std::from_range, source);
    ASSERT_EQ(list.size(), 4U);
    EXPECT_EQ(list[0], 1);
    EXPECT_EQ(list[3], 4);
}

TEST(TypedListSpan, from_range_with_filter_view) {
    std::vector<std::int32_t> source{1, 2, 3, 4, 5};
    auto evens = source | std::views::filter([](const int x) { return x % 2 == 0; });
    parcel::I32ListCell list(std::from_range, evens);
    ASSERT_EQ(list.size(), 2U);
    EXPECT_EQ(list[0], 2);
    EXPECT_EQ(list[1], 4);
}

TEST(TypedListSpan, span_constructor_copies_data) {
    std::array<std::int32_t, 3> source{10, 20, 30};
    parcel::I32ListCell list{std::span<const std::int32_t>(source)};
    ASSERT_EQ(list.size(), 3U);
    EXPECT_EQ(list[1], 20);
}

TEST(TypedListSpan, as_span_const_view_matches_storage) {
    parcel::I32ListCell list{1, 2, 3};
    const auto sp = list.as_span();
    ASSERT_EQ(sp.size(), 3U);
    EXPECT_EQ(sp[2], 3);
}

TEST(TypedListSpan, as_span_mutable_allows_in_place_edit) {
    parcel::I32ListCell list{1, 2, 3};
    auto sp = list.as_span();
    sp[1] = 99;
    EXPECT_EQ(list[1], 99);
}

// ---------------------------------------------------------------------------
// A4 (TypedMapCell side) and A5: keys/values views

TEST(TypedMapRanges, from_range_constructor_takes_pair_range) {
    std::vector<std::pair<std::string, std::int32_t>> source{{"a", 1}, {"b", 2}};
    parcel::I32MapCell m(std::from_range, source);
    ASSERT_EQ(m.size(), 2U);
    EXPECT_EQ(m["a"], 1);
    EXPECT_EQ(m["b"], 2);
}

TEST(TypedMapRanges, keys_view_yields_sorted_keys) {
    const parcel::I32MapCell m{{"b", 1}, {"a", 2}, {"c", 3}};
    std::vector<std::string> keys;
    for (auto const& k : m.keys()) {
        keys.push_back(k);
    }
    ASSERT_EQ(keys.size(), 3U);
    EXPECT_EQ(keys[0], "a");
    EXPECT_EQ(keys[1], "b");
    EXPECT_EQ(keys[2], "c");
}

TEST(TypedMapRanges, values_view_yields_values_in_key_order) {
    const parcel::I32MapCell m{{"a", 10}, {"b", 20}};
    std::vector<std::int32_t> values;
    for (auto const& v : m.values()) {
        values.push_back(v);
    }
    ASSERT_EQ(values.size(), 2U);
    EXPECT_EQ(values[0], 10);
    EXPECT_EQ(values[1], 20);
}

TEST(MapCellRanges, keys_values_yield_sorted_views) {
    const parcel::MapCell m{{"x", parcel::cell(1)}, {"a", parcel::cell(std::string("x"))}};
    std::vector<std::string> keys;
    for (auto const& k : m.keys()) {
        keys.push_back(k);
    }
    EXPECT_EQ(keys.front(), "a");

    std::vector<parcel::cell_t> vals;
    for (auto const& v : m.values()) {
        vals.push_back(v);
    }
    EXPECT_EQ(vals.size(), 2U);
}

// ---------------------------------------------------------------------------
// B7: make_list / make_map

TEST(MakeList, builds_heterogeneous_list_from_raw_values) {
    const auto l = parcel::make_list(1, std::string("hi"), true);
    ASSERT_EQ(l->size(), 3U);
    EXPECT_EQ(l->at(0)->kind(), "i32");
    EXPECT_EQ(l->at(1)->kind(), "string");
    EXPECT_EQ(l->at(2)->kind(), "bool");
}

TEST(MakeList, accepts_string_literal) {
    const auto l = parcel::make_list("alpha");
    ASSERT_EQ(l->size(), 1U);
    EXPECT_EQ(l->at(0)->kind(), "string");
}

TEST(MakeMap, builds_heterogeneous_map_from_pairs) {
    const auto m = parcel::make_map({
        {"x", parcel::cell(1)},
        {"y", parcel::cell(std::string("hi"))},
    });
    ASSERT_EQ(m->size(), 2U);
    EXPECT_EQ(m->at("x")->kind(), "i32");
    EXPECT_EQ(m->at("y")->kind(), "string");
}

// ---------------------------------------------------------------------------
// B9: extra default_cell_for specializations

TEST(DefaultCellFor, std_array_resolves_to_typed_list) {
    using Cell = parcel::default_cell_for_t<std::array<std::int32_t, 3>>;
    static_assert(std::is_same_v<Cell, parcel::TypedListCell<parcel::I32Cell>>);
    SUCCEED();
}

TEST(DefaultCellFor, std_deque_resolves_to_typed_list) {
    using Cell = parcel::default_cell_for_t<std::deque<std::string>>;
    static_assert(std::is_same_v<Cell, parcel::TypedListCell<parcel::StringCell>>);
    SUCCEED();
}

TEST(DefaultCellFor, std_list_resolves_to_typed_list) {
    using Cell = parcel::default_cell_for_t<std::list<std::int32_t>>;
    static_assert(std::is_same_v<Cell, parcel::TypedListCell<parcel::I32Cell>>);
    SUCCEED();
}

TEST(DefaultCellFor, std_set_resolves_to_typed_list) {
    using Cell = parcel::default_cell_for_t<std::set<std::int32_t>>;
    static_assert(std::is_same_v<Cell, parcel::TypedListCell<parcel::I32Cell>>);
    SUCCEED();
}

TEST(DefaultCellFor, std_unordered_map_resolves_to_typed_map) {
    using Cell = parcel::default_cell_for_t<std::unordered_map<std::string, std::int32_t>>;
    static_assert(std::is_same_v<Cell, parcel::TypedMapCell<parcel::I32Cell>>);
    SUCCEED();
}
