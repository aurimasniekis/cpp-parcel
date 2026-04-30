#include <parcel/parcel.h>
#include <parcel/unordered_map.h>

#include <gtest/gtest.h>

#include <compare>
#include <limits>
#include <memory>
#include <string>

namespace {

parcel::ParcelRegistry const& registry() {
    static const parcel::ParcelRegistry r;
    return r;
}

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

class SelfNode : public parcel::SelfStructCell<SelfNode> {
public:
    [[maybe_unused]] static constexpr std::string_view kind_id{"s:selfnode"};

    SelfNode() = default;
    SelfNode(const std::int32_t v, std::string s) : value_(v), text_(std::move(s)) {}

    [[maybe_unused]] static parcel::descriptor::MetaInfo meta_info() {
        return {.name = "SelfNode"};
    }

    [[maybe_unused]] static auto field_descriptors() {
        return parcel::FieldsBuilder<SelfNode>{}
            .field<&SelfNode::value_>("v")
            .field<&SelfNode::text_>("t")
            .build();
    }

    std::int32_t value_{};
    std::string text_;
};

}  // namespace

// ---------------------------------------------------------------------------
// Primitive comparisons

TEST(CellCompare, equal_primitives_equal) {
    parcel::I32Cell a{5};
    parcel::I32Cell b{5};
    EXPECT_TRUE(a == b);
    EXPECT_FALSE(a < b);
    EXPECT_TRUE((a <=> b) == std::partial_ordering::equivalent);
}

TEST(CellCompare, primitives_three_way_orders_by_value) {
    parcel::I32Cell a{1};
    parcel::I32Cell b{2};
    EXPECT_TRUE(a < b);
    EXPECT_TRUE(b > a);
    EXPECT_FALSE(a == b);
}

TEST(CellCompare, primitives_string_lexicographic) {
    parcel::StringCell a{std::string("alpha")};
    parcel::StringCell b{std::string("beta")};
    EXPECT_TRUE(a < b);
    EXPECT_TRUE(b > a);
    EXPECT_FALSE(a == b);
}

TEST(CellCompare, primitives_different_kinds_compare_by_kind) {
    parcel::I32Cell i{0};
    parcel::StringCell s{std::string{}};
    // "i32" < "string" lexicographically — kind disambiguates first.
    EXPECT_TRUE(static_cast<parcel::ICell const&>(i) < static_cast<parcel::ICell const&>(s));
}

TEST(CellCompare, primitives_meta_is_ignored_in_equality) {
    const auto a = parcel::I32Cell::of(42);
    const auto b = parcel::I32Cell::of(42)->with_name("answer");
    EXPECT_EQ(*a, *b);
    EXPECT_TRUE((*a <=> *b) == std::partial_ordering::equivalent);
}

TEST(CellCompare, primitive_double_nan_is_unordered) {
    parcel::DoubleCell a{std::numeric_limits<double>::quiet_NaN()};
    parcel::DoubleCell b{1.0};
    EXPECT_TRUE((a <=> b) == std::partial_ordering::unordered);
    EXPECT_FALSE(a == b);
}

// ---------------------------------------------------------------------------
// TypedListCell comparisons

TEST(CellCompare, typed_list_lexicographic) {
    parcel::I32ListCell a{1, 2, 3};
    parcel::I32ListCell b{1, 2, 4};
    EXPECT_TRUE(a < b);
    EXPECT_FALSE(a == b);
}

TEST(CellCompare, typed_list_shorter_is_less_when_prefix_equal) {
    parcel::I32ListCell a{1, 2};
    parcel::I32ListCell b{1, 2, 3};
    EXPECT_TRUE(a < b);
}

TEST(CellCompare, typed_list_equal_lists_equivalent) {
    parcel::I32ListCell a{1, 2, 3};
    parcel::I32ListCell b{1, 2, 3};
    EXPECT_EQ(a, b);
}

TEST(CellCompare, typed_list_of_struct_routes_through_cell_compare) {
    // std::vector<Person>::operator== would not compile; the override on
    // TypedListCell uses Cell::compare instead. Just exercising it here.
    parcel::TypedListCell<PersonCell> a;
    a.push_back(Person{1, "alice"});
    a.push_back(Person{2, "bob"});

    parcel::TypedListCell<PersonCell> b;
    b.push_back(Person{1, "alice"});
    b.push_back(Person{2, "bob"});
    EXPECT_EQ(a, b);

    parcel::TypedListCell<PersonCell> c;
    c.push_back(Person{1, "alice"});
    c.push_back(Person{2, "carol"});
    EXPECT_NE(a, c);
}

// ---------------------------------------------------------------------------
// Heterogeneous ListCell comparisons

TEST(CellCompare, list_cell_compares_through_elements) {
    parcel::ListCell a{parcel::cell(1), parcel::cell(std::string("x"))};
    parcel::ListCell b{parcel::cell(1), parcel::cell(std::string("x"))};
    EXPECT_EQ(a, b);
    parcel::ListCell c{parcel::cell(1), parcel::cell(std::string("y"))};
    EXPECT_NE(a, c);
    EXPECT_TRUE(a < c);
}

TEST(CellCompare, list_cell_null_element_orders_less_than_nonnull) {
    parcel::ListCell a{parcel::cell_t{}, parcel::cell(1)};
    parcel::ListCell b{parcel::cell(0), parcel::cell(1)};
    EXPECT_TRUE(a < b);
}

// ---------------------------------------------------------------------------
// TypedMapCell comparisons

TEST(CellCompare, typed_map_compares_keys_then_values) {
    parcel::I32MapCell a{{"a", 1}, {"b", 2}};
    parcel::I32MapCell b{{"a", 1}, {"b", 2}};
    EXPECT_EQ(a, b);

    parcel::I32MapCell c{{"a", 1}, {"b", 3}};
    EXPECT_TRUE(a < c);
}

TEST(CellCompare, typed_map_size_breaks_tie_on_prefix) {
    parcel::I32MapCell a{{"a", 1}};
    parcel::I32MapCell b{{"a", 1}, {"b", 2}};
    EXPECT_TRUE(a < b);
}

// ---------------------------------------------------------------------------
// MapCell heterogeneous comparisons

TEST(CellCompare, map_cell_lexicographic_over_keys) {
    parcel::MapCell a{{"x", parcel::cell(1)}, {"y", parcel::cell(std::string("hi"))}};
    parcel::MapCell b{{"x", parcel::cell(1)}, {"y", parcel::cell(std::string("hi"))}};
    EXPECT_EQ(a, b);
    parcel::MapCell c{{"x", parcel::cell(1)}, {"y", parcel::cell(std::string("hj"))}};
    EXPECT_TRUE(a < c);
}

TEST(CellCompare, map_cell_null_value_orders_less_than_nonnull) {
    parcel::MapCell a{{"k", parcel::cell_t{}}};
    parcel::MapCell b{{"k", parcel::cell(0)}};
    EXPECT_TRUE(a < b);
}

// ---------------------------------------------------------------------------
// UnionCell comparisons

TEST(CellCompare, union_compares_equal_when_same_alternative_value) {
    using U = parcel::UnionCell<parcel::I32Cell, parcel::StringCell>;
    U a;
    a.value.emplace<0>(7);
    U b;
    b.value.emplace<0>(7);
    EXPECT_EQ(a, b);
}

TEST(CellCompare, union_orders_by_active_index_first) {
    using U = parcel::UnionCell<parcel::I32Cell, parcel::StringCell>;
    U a;
    a.value.emplace<0>(99);
    U b;
    b.value.emplace<1>(std::string("a"));
    // Index 0 < Index 1, regardless of values.
    EXPECT_TRUE(a < b);
}

TEST(CellCompare, union_orders_by_active_value_when_index_matches) {
    using U = parcel::UnionCell<parcel::I32Cell, parcel::StringCell>;
    U a;
    a.value.emplace<1>(std::string("alpha"));
    U b;
    b.value.emplace<1>(std::string("beta"));
    EXPECT_TRUE(a < b);
}

// ---------------------------------------------------------------------------
// StructCell comparisons (and SelfStructCell)

TEST(CellCompare, struct_compares_field_by_field) {
    const PersonCell a(Person{1, "alice"});
    const PersonCell b(Person{1, "alice"});
    EXPECT_EQ(a, b);

    const PersonCell c(Person{2, "alice"});
    EXPECT_NE(a, c);
}

TEST(CellCompare, struct_meta_does_not_affect_compare) {
    const auto a = std::make_shared<PersonCell>(Person{5, "x"});
    const auto b = a->with_name("renamed");
    EXPECT_EQ(*a, *b);
}

TEST(CellCompare, self_struct_compares_its_own_fields) {
    const SelfNode a(1, "x");
    const SelfNode b(1, "x");
    EXPECT_EQ(a, b);

    const SelfNode c(1, "y");
    EXPECT_NE(a, c);
}

TEST(CellCompare, struct_orders_by_first_non_equal_field_in_declaration_order) {
    // Declaration order in PersonCell::field_descriptors: id, name.
    // a and b agree on id, differ on name → order resolves on the second
    // field, never JSON-projecting the whole struct.
    const PersonCell a(Person{1, "alice"});
    const PersonCell b(Person{1, "bob"});
    EXPECT_TRUE(a < b);
    EXPECT_TRUE(b > a);
    EXPECT_TRUE((a <=> b) == std::partial_ordering::less);
}

TEST(CellCompare, struct_first_field_dominates_later_disagreement) {
    // a.id < b.id; later fields disagree but the first field decides.
    const PersonCell a(Person{1, "zeta"});
    const PersonCell b(Person{2, "alpha"});
    EXPECT_TRUE(a < b);
}

namespace {

struct Optish {
    std::int32_t id{0};
    std::optional<std::string> note;
};

class OptishCell : public parcel::StructCell<OptishCell, Optish, "optish"> {
public:
    using StructCell::StructCell;
    [[maybe_unused]] static auto field_descriptors() {
        return parcel::FieldsBuilder<Optish>{}
            .field<&Optish::id>("id")
            .field<&Optish::note>("note")
            .build();
    }
};

}  // namespace

TEST(CellCompare, struct_optional_absent_orders_before_present) {
    const OptishCell a(Optish{.id = 1, .note = std::nullopt});
    const OptishCell b(Optish{.id = 1, .note = "hi"});
    EXPECT_TRUE(a < b);
    EXPECT_TRUE(b > a);
}

TEST(CellCompare, struct_optional_both_absent_is_equivalent) {
    const OptishCell a(Optish{.id = 1, .note = std::nullopt});
    const OptishCell b(Optish{.id = 1, .note = std::nullopt});
    EXPECT_EQ(a, b);
}

// ---------------------------------------------------------------------------
// Different concrete types in equality through ICell base

TEST(CellCompare, different_kinds_unequal_through_icell) {
    parcel::I32Cell a{1};
    parcel::U32Cell b{1U};
    parcel::ICell const& ai = a;
    parcel::ICell const& bi = b;
    EXPECT_NE(ai, bi);
}

// ---------------------------------------------------------------------------
// Round-trip property: a cell equals its JSON->cell rebuild

TEST(CellCompare, roundtrip_primitive_equal_after_json) {
    const parcel::I32Cell a{42};
    const auto j = a.to_json();
    const auto rebuilt = parcel::I32Cell::from_json(j, registry());
    EXPECT_EQ(a, *rebuilt);
}

TEST(CellCompare, roundtrip_typed_list_equal_after_json) {
    const parcel::I32ListCell a{1, 2, 3};
    const auto j = a.to_json();
    const auto rebuilt = parcel::I32ListCell::from_json(j, registry());
    EXPECT_EQ(a, *rebuilt);
}

TEST(CellCompare, roundtrip_typed_map_equal_after_json) {
    const parcel::StringMapCell a{{"x", "hi"}, {"y", "lo"}};
    const auto j = a.to_json();
    const auto rebuilt = parcel::StringMapCell::from_json(j, registry());
    EXPECT_EQ(a, *rebuilt);
}

TEST(CellCompare, roundtrip_struct_equal_after_json) {
    const PersonCell a(Person{1, "ada"});
    const auto j = a.to_json();
    const auto rebuilt = PersonCell::from_json(j, registry());
    EXPECT_EQ(a, *rebuilt);
}

// ---------------------------------------------------------------------------
// Hash equality-consistency: two cells that compare equal under
// `operator==` must hash equal, even when nested children carry differing
// MetaInfo. The list/map/hashmap overrides recurse via hash_value() so
// nested meta is never observed.

TEST(CellCompare, list_hash_ignores_nested_meta) {
    const parcel::ListCell a{
        parcel::cell(1)->with_description("plain"),
        parcel::cell(std::string("hi")),
    };
    const parcel::ListCell b{
        parcel::cell(1)->with_description("differently annotated"),
        parcel::cell(std::string("hi")),
    };
    EXPECT_EQ(a, b);
    EXPECT_EQ(std::hash<parcel::ICell>{}(a), std::hash<parcel::ICell>{}(b));
}

TEST(CellCompare, map_hash_ignores_nested_meta) {
    const parcel::MapCell a{
        {"x", parcel::cell(1)->with_name("primary")},
    };
    const parcel::MapCell b{
        {"x", parcel::cell(1)->with_name("renamed")},
    };
    EXPECT_EQ(a, b);
    EXPECT_EQ(std::hash<parcel::ICell>{}(a), std::hash<parcel::ICell>{}(b));
}

TEST(CellCompare, hashmap_hash_ignores_nested_meta_and_iteration_order) {
    parcel::HashMapCell a{
        {"x", parcel::cell(1)->with_color("red")},
        {"y", parcel::cell(2)},
    };
    parcel::HashMapCell b{
        {"y", parcel::cell(2)},
        {"x", parcel::cell(1)->with_color("blue")},
    };
    EXPECT_EQ(a, b);
    EXPECT_EQ(std::hash<parcel::ICell>{}(a), std::hash<parcel::ICell>{}(b));
}
