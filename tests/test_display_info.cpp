#include <parcel/parcel.h>

#include <gtest/gtest.h>

#include <memory>
#include <string>

namespace {

struct Person {
    std::int32_t age{0};
    std::string name;
};

class PersonCell : public parcel::StructCell<PersonCell, Person, "person"> {
public:
    using StructCell::StructCell;

    [[maybe_unused]] static parcel::DisplayInfo display_info() {
        return {.name = "Person"};
    }

    [[maybe_unused]] static auto field_descriptors() {
        return parcel::FieldsBuilder<Person>{}
            .field<&Person::age>("age")
            .field<&Person::name>("name")
            .build();
    }
};

parcel::ParcelRegistry make_registry() {
    parcel::ParcelRegistry r;
    r.register_kind(parcel::UnionCell<parcel::I32Cell, parcel::StringCell>::descriptor());
    r.register_kind(PersonCell::descriptor());
    return r;
}

}  // namespace

// ---------------------------------------------------------------------------
// Builder immutability + chaining

TEST(CellDisplayInfo, with_name_returns_new_cell_and_leaves_original_untouched) {
    const auto original = std::make_shared<parcel::I32Cell>(42);
    EXPECT_FALSE(original->overridden_display_info().has_value());

    const auto annotated = original->with_name("Answer");
    ASSERT_NE(annotated.get(), original.get());
    ASSERT_TRUE(annotated->overridden_display_info().has_value());
    EXPECT_EQ(annotated->overridden_display_info()->name, "Answer");

    EXPECT_FALSE(original->overridden_display_info().has_value());
}

TEST(CellDisplayInfo, chained_builders_accumulate_fields) {
    const auto cell = parcel::I32Cell{7}
                          .with_name("Lucky")
                          ->with_description("A lucky number")
                          ->with_icon("mdi:star")
                          ->with_color("#ffcc00");
    ASSERT_TRUE(cell->overridden_display_info().has_value());
    EXPECT_EQ(cell->overridden_display_info()->name, "Lucky");
    EXPECT_EQ(cell->overridden_display_info()->description, "A lucky number");
    EXPECT_EQ(cell->overridden_display_info()->icon, comms::Icon::from("mdi:star"));
    EXPECT_EQ(cell->overridden_display_info()->color, comms::Color::parse("#ffcc00"));
}

TEST(CellDisplayInfo, with_description_alone_omits_name) {
    const auto cell = parcel::I32Cell{1}.with_description("just a number");
    ASSERT_TRUE(cell->overridden_display_info().has_value());
    EXPECT_FALSE(cell->overridden_display_info()->name.has_value());
    EXPECT_EQ(cell->overridden_display_info()->description, "just a number");
}

TEST(CellDisplayInfo, with_display_info_replaces_whole_struct) {
    const auto base = std::make_shared<parcel::StringCell>("x");
    const auto first = base->with_name("A")->with_color("red");
    ASSERT_TRUE(first->overridden_display_info().has_value());

    const auto replaced = first->with_display_info(parcel::DisplayInfo{.description = "B"});
    ASSERT_TRUE(replaced->overridden_display_info().has_value());
    EXPECT_FALSE(replaced->overridden_display_info()->name.has_value());
    EXPECT_FALSE(replaced->overridden_display_info()->color.has_value());
    EXPECT_EQ(replaced->overridden_display_info()->description, "B");
}

// ---------------------------------------------------------------------------
// JSON: emission

TEST(CellDisplayInfo, to_json_omits_d_when_display_info_unset) {
    const auto j = parcel::I32Cell{42}.to_json();
    EXPECT_FALSE(j.contains(std::string{parcel::ICell::KEY_DESCRIPTION}));
}

TEST(CellDisplayInfo, to_json_emits_d_with_only_set_fields) {
    const auto annotated = parcel::I32Cell{42}.with_description("ans");
    const auto j = annotated->to_json();
    ASSERT_TRUE(j.contains("d"));
    EXPECT_TRUE(j.at("d").is_object());
    EXPECT_FALSE(j.at("d").contains("name"));
    EXPECT_EQ(j.at("d").at("description"), "ans");
}

TEST(CellDisplayInfo, to_json_emits_d_with_full_display_info) {
    const auto annotated = parcel::I32Cell{1}
                               .with_name("N")
                               ->with_description("D")
                               ->with_icon("mdi:information")
                               ->with_color("cyan");
    const auto j = annotated->to_json();
    ASSERT_TRUE(j.contains("d"));
    EXPECT_EQ(j["d"]["name"], "N");
    EXPECT_EQ(j["d"]["description"], "D");
    // icon serializes as its Iconify set:name string; color as a hex string.
    EXPECT_EQ(j["d"]["icon"], "mdi:information");
    EXPECT_EQ(j["d"]["color"], "#00ffff");
}

// ---------------------------------------------------------------------------
// JSON: round-trip per cell type

TEST(CellDisplayInfo, primitive_round_trip) {
    const auto reg = make_registry();
    const auto annotated = parcel::I32Cell{42}.with_name("Answer")->with_description("life");

    const auto j = annotated->to_json();
    const auto restored = reg.cell_from_json(j);
    ASSERT_TRUE(restored->overridden_display_info().has_value());
    EXPECT_EQ(restored->overridden_display_info()->name, "Answer");
    EXPECT_EQ(restored->overridden_display_info()->description, "life");
}

TEST(CellDisplayInfo, typed_list_round_trip_outer_display_info) {
    const auto reg = make_registry();
    const auto list = parcel::TypedListCell<parcel::I32Cell>{1, 2, 3}.with_name("Numbers");

    const auto j = list->to_json();
    const auto restored = reg.cell_from_json(j);
    ASSERT_TRUE(restored->overridden_display_info().has_value());
    EXPECT_EQ(restored->overridden_display_info()->name, "Numbers");
}

TEST(CellDisplayInfo, list_round_trip_outer_and_element_display_info) {
    const auto reg = make_registry();

    parcel::ListCell list;
    list.push_back(std::make_shared<parcel::I32Cell>(1)->with_name("first"));
    list.push_back(std::make_shared<parcel::StringCell>("hi")->with_description("greeting"));
    const auto annotated = list.with_name("Mixed");

    const auto j = annotated->to_json();
    const auto restored = reg.cell_from_json(j);
    ASSERT_TRUE(restored->overridden_display_info().has_value());
    EXPECT_EQ(restored->overridden_display_info()->name, "Mixed");

    auto* lc = dynamic_cast<parcel::ListCell*>(restored.get());
    ASSERT_NE(lc, nullptr);
    ASSERT_EQ(lc->size(), 2u);
    ASSERT_TRUE((*lc)[0]->overridden_display_info().has_value());
    EXPECT_EQ((*lc)[0]->overridden_display_info()->name, "first");
    ASSERT_TRUE((*lc)[1]->overridden_display_info().has_value());
    EXPECT_EQ((*lc)[1]->overridden_display_info()->description, "greeting");
}

TEST(CellDisplayInfo, typed_map_round_trip) {
    const auto reg = make_registry();
    parcel::TypedMapCell<parcel::I32Cell> tm;
    tm.emplace("a", 1);
    tm.emplace("b", 2);
    const auto annotated = tm.with_color("blue");

    const auto j = annotated->to_json();
    const auto restored = reg.cell_from_json(j);
    ASSERT_TRUE(restored->overridden_display_info().has_value());
    EXPECT_EQ(restored->overridden_display_info()->color, comms::Color::parse("blue"));
}

TEST(CellDisplayInfo, map_round_trip_outer_and_element_display_info) {
    const auto reg = make_registry();
    parcel::MapCell m;
    m.emplace("x", std::make_shared<parcel::I32Cell>(1)->with_icon("mdi:dot"));
    const auto annotated = m.with_name("Bag");

    const auto j = annotated->to_json();
    const auto restored = reg.cell_from_json(j);
    ASSERT_TRUE(restored->overridden_display_info().has_value());
    EXPECT_EQ(restored->overridden_display_info()->name, "Bag");

    auto* mc = dynamic_cast<parcel::MapCell*>(restored.get());
    ASSERT_NE(mc, nullptr);
    const auto it = mc->find("x");
    ASSERT_NE(it, mc->end());
    ASSERT_TRUE(it->second->overridden_display_info().has_value());
    EXPECT_EQ(it->second->overridden_display_info()->icon, comms::Icon::from("mdi:dot"));
}

TEST(CellDisplayInfo, struct_round_trip) {
    const auto reg = make_registry();
    const PersonCell p{Person{.age = 30, .name = "Alice"}};
    const auto annotated = p.with_description("primary contact");

    const auto j = annotated->to_json();
    const auto restored = reg.cell_from_json(j);
    ASSERT_TRUE(restored->overridden_display_info().has_value());
    EXPECT_EQ(restored->overridden_display_info()->description, "primary contact");

    auto* pc = dynamic_cast<PersonCell*>(restored.get());
    ASSERT_NE(pc, nullptr);
    EXPECT_EQ(pc->value.age, 30);
    EXPECT_EQ(pc->value.name, "Alice");
}

TEST(CellDisplayInfo, union_round_trip) {
    using U = parcel::UnionCell<parcel::I32Cell, parcel::StringCell>;
    const auto reg = make_registry();

    U u;
    u.value.emplace<0>(7);
    const auto annotated = u.with_name("Choice");

    const auto j = annotated->to_json();
    const auto restored = reg.cell_from_json(j);
    ASSERT_TRUE(restored->overridden_display_info().has_value());
    EXPECT_EQ(restored->overridden_display_info()->name, "Choice");
}

// ---------------------------------------------------------------------------
// JSON: deserialization without display info yields nullopt

TEST(CellDisplayInfo, from_json_without_d_yields_nullopt) {
    const auto reg = make_registry();
    const parcel::json_t j{{"k", "i32"}, {"v", 5}};
    const auto restored = reg.cell_from_json(j);
    EXPECT_FALSE(restored->overridden_display_info().has_value());
}

// ---------------------------------------------------------------------------
// clone preserves display info

TEST(CellDisplayInfo, clone_preserves_display_info) {
    const auto annotated = parcel::I32Cell{1}.with_name("A");
    const auto cloned = annotated->clone();
    ASSERT_TRUE(cloned->overridden_display_info().has_value());
    EXPECT_EQ(cloned->overridden_display_info()->name, "A");
}
