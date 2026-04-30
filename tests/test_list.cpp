#include <parcel/parcel.h>

#include <gtest/gtest.h>

#include <memory>
#include <string>
#include <vector>

namespace {

parcel::ParcelRegistry const& registry() {
    static const parcel::ParcelRegistry r;
    return r;
}

parcel::ParcelRegistry const& populated_registry() {
    // Builtins (primitives + List + Map) are auto-populated by the default ctor.
    static const parcel::ParcelRegistry r;
    return r;
}

}  // namespace

// ---------------------------------------------------------------------------
// Concept gate

static_assert(parcel::CellLike<parcel::TypedListCell<parcel::I32Cell>>);
static_assert(parcel::CellLike<parcel::TypedListCell<parcel::StringCell>>);
static_assert(parcel::CellLike<parcel::TypedListCell<parcel::BoolCell>>);

// ---------------------------------------------------------------------------
// Construction + value access

TEST(TypedListCell, default_constructs_to_empty_vector) {
    parcel::TypedListCell<parcel::I32Cell> v;
    EXPECT_TRUE(v.get().empty());
}

TEST(TypedListCell, constructs_from_vector) {
    std::vector<std::int32_t> elems{1, 2};
    parcel::TypedListCell<parcel::I32Cell> v{std::move(elems)};
    ASSERT_EQ(v.get().size(), 2u);
    EXPECT_EQ(v.get()[0], 1);
    EXPECT_EQ(v.get()[1], 2);
}

TEST(TypedListCell, constructs_from_initializer_list) {
    parcel::TypedListCell<parcel::I32Cell> v{10, 20, 30};
    ASSERT_EQ(v.get().size(), 3u);
    EXPECT_EQ(v.get()[0], 10);
    EXPECT_EQ(v.get()[2], 30);
}

// ---------------------------------------------------------------------------
// kind()

TEST(TypedListCell, kind_encodes_element_type) {
    EXPECT_EQ(parcel::TypedListCell<parcel::I32Cell>{}.kind(), "l:i32");
    EXPECT_EQ(parcel::TypedListCell<parcel::StringCell>{}.kind(), "l:string");
    EXPECT_EQ(parcel::TypedListCell<parcel::BoolCell>{}.kind(), "l:bool");
}

// ---------------------------------------------------------------------------
// to_string()

TEST(TypedListCell, to_string_empty_list) {
    const parcel::TypedListCell<parcel::I32Cell> v;
    EXPECT_EQ(v.to_string(), "[]");
}

TEST(TypedListCell, to_string_int_list) {
    const parcel::TypedListCell<parcel::I32Cell> v{1, 2, 3};
    EXPECT_EQ(v.to_string(), "[1, 2, 3]");
}

TEST(TypedListCell, to_string_string_list) {
    const parcel::TypedListCell<parcel::StringCell> v{std::string("hello"), std::string("world")};
    EXPECT_EQ(v.to_string(), "[hello, world]");
}

// ---------------------------------------------------------------------------
// to_json() shape

TEST(TypedListCell, to_json_emits_k_v_keys) {
    const parcel::TypedListCell<parcel::I32Cell> v{1, 2, 3};
    const auto j = v.to_json();
    ASSERT_TRUE(j.is_object());
    EXPECT_EQ(j["k"].get<std::string>(), "l:i32");
    ASSERT_TRUE(j["v"].is_array());
    ASSERT_EQ(j["v"].size(), 3u);
    EXPECT_EQ(j["v"][0].get<int>(), 1);
    EXPECT_EQ(j["v"][1].get<int>(), 2);
    EXPECT_EQ(j["v"][2].get<int>(), 3);
}

TEST(TypedListCell, to_json_v_contains_raw_scalars_not_objects) {
    const parcel::TypedListCell<parcel::I32Cell> v{42};
    const auto j = v.to_json();
    ASSERT_TRUE(j["v"].is_array());
    ASSERT_EQ(j["v"].size(), 1u);
    EXPECT_TRUE(j["v"][0].is_number()) << "elements must be raw scalars, not {k,v} objects";
}

TEST(TypedListCell, to_json_empty_list_v_is_empty_array) {
    const parcel::TypedListCell<parcel::I32Cell> v;
    const auto j = v.to_json();
    ASSERT_TRUE(j["v"].is_array());
    EXPECT_EQ(j["v"].size(), 0u);
    EXPECT_EQ(j["k"].get<std::string>(), "l:i32");
}

// ---------------------------------------------------------------------------
// from_json round-trip

TEST(TypedListCell, from_json_round_trip_int) {
    const parcel::TypedListCell<parcel::I32Cell> original{1, 2, 3};
    const auto j = original.to_json();
    const auto restored = parcel::TypedListCell<parcel::I32Cell>::from_json(j, registry());
    ASSERT_NE(restored, nullptr);
    EXPECT_EQ(restored->kind(), "l:i32");
    auto* typed = dynamic_cast<parcel::TypedListCell<parcel::I32Cell>*>(restored.get());
    ASSERT_NE(typed, nullptr);
    ASSERT_EQ(typed->get().size(), 3u);
    EXPECT_EQ(typed->get()[0], 1);
    EXPECT_EQ(typed->get()[1], 2);
    EXPECT_EQ(typed->get()[2], 3);
}

TEST(TypedListCell, from_json_round_trip_string) {
    const parcel::TypedListCell<parcel::StringCell> original{std::string("a"), std::string("b")};
    const auto j = original.to_json();
    const auto restored = parcel::TypedListCell<parcel::StringCell>::from_json(j, registry());
    auto* typed = dynamic_cast<parcel::TypedListCell<parcel::StringCell>*>(restored.get());
    ASSERT_NE(typed, nullptr);
    ASSERT_EQ(typed->get().size(), 2u);
    EXPECT_EQ(typed->get()[0], "a");
    EXPECT_EQ(typed->get()[1], "b");
}

TEST(TypedListCell, from_json_round_trip_empty_list) {
    const parcel::TypedListCell<parcel::I32Cell> original;
    const auto j = original.to_json();
    const auto restored = parcel::TypedListCell<parcel::I32Cell>::from_json(j, registry());
    auto* typed = dynamic_cast<parcel::TypedListCell<parcel::I32Cell>*>(restored.get());
    ASSERT_NE(typed, nullptr);
    EXPECT_TRUE(typed->get().empty());
}

// ---------------------------------------------------------------------------
// from_json error paths

TEST(TypedListCell, from_json_rejects_non_object) {
    parcel::json_t arr = parcel::json_t::array({1, 2, 3});
    EXPECT_THROW(parcel::TypedListCell<parcel::I32Cell>::from_json(arr, registry()),
                 std::runtime_error);
}

TEST(TypedListCell, from_json_rejects_missing_kind) {
    parcel::json_t j = {{"v", parcel::json_t::array({1, 2})}};
    EXPECT_THROW(parcel::TypedListCell<parcel::I32Cell>::from_json(j, registry()),
                 std::runtime_error);
}

TEST(TypedListCell, from_json_rejects_wrong_kind) {
    parcel::json_t j = {{"k", "i32"}, {"v", parcel::json_t::array({1})}};
    EXPECT_THROW(parcel::TypedListCell<parcel::I32Cell>::from_json(j, registry()),
                 std::runtime_error);
}

TEST(TypedListCell, from_json_rejects_mismatched_element_kind_via_kind) {
    // Element kind is part of 'k' now, so a payload tagged "l:i64" is simply
    // not the same kind as TypedListCell<I32Cell> ("l:i32"); it must throw.
    parcel::json_t j = {{"k", "l:i64"}, {"v", parcel::json_t::array({1})}};
    EXPECT_THROW(parcel::TypedListCell<parcel::I32Cell>::from_json(j, registry()),
                 std::runtime_error);
}

TEST(TypedListCell, from_json_rejects_missing_v) {
    parcel::json_t j = {{"k", "l:i32"}};
    EXPECT_THROW(parcel::TypedListCell<parcel::I32Cell>::from_json(j, registry()),
                 std::runtime_error);
}

TEST(TypedListCell, from_json_rejects_v_not_array) {
    parcel::json_t j = {{"k", "l:i32"}, {"v", 42}};
    EXPECT_THROW(parcel::TypedListCell<parcel::I32Cell>::from_json(j, registry()),
                 std::runtime_error);
}

TEST(TypedListCell, from_json_rejects_invalid_element) {
    // Element parse failure surfaces as whatever the element type throws —
    // nlohmann's TypeError in the int-from-string case. We just check that
    // *some* exception escapes, not the concrete type.
    const parcel::json_t j = {{"k", "l:i32"}, {"v", parcel::json_t::array({"abc"})}};
    EXPECT_ANY_THROW(parcel::TypedListCell<parcel::I32Cell>::from_json(j, registry()));
}

// ---------------------------------------------------------------------------
// std::vector pass-through API (raw element_storage_t)

TEST(TypedListCell, size_and_empty) {
    parcel::TypedListCell<parcel::I32Cell> v;
    EXPECT_TRUE(v.empty());
    EXPECT_EQ(v.size(), 0u);

    v.push_back(1);
    EXPECT_FALSE(v.empty());
    EXPECT_EQ(v.size(), 1u);
}

TEST(TypedListCell, range_based_for_iterates_elements) {
    parcel::TypedListCell<parcel::I32Cell> v{1, 2, 3};
    int sum = 0;
    for (auto const& el : v) {
        sum += el;
    }
    EXPECT_EQ(sum, 6);
}

TEST(TypedListCell, push_back_and_emplace_back) {
    parcel::TypedListCell<parcel::I32Cell> v;
    v.push_back(10);
    v.emplace_back(20);
    ASSERT_EQ(v.size(), 2u);
    EXPECT_EQ(v[0], 10);
    EXPECT_EQ(v[1], 20);
}

TEST(TypedListCell, indexed_access_read_and_write) {
    parcel::TypedListCell<parcel::I32Cell> v{1, 2};
    EXPECT_EQ(v[0], 1);
    v[0] = 99;
    EXPECT_EQ(v[0], 99);
    EXPECT_EQ(v.at(1), 2);
}

TEST(TypedListCell, at_throws_on_out_of_range) {
    parcel::TypedListCell<parcel::I32Cell> v{1};
    EXPECT_THROW((void)v.at(5), std::out_of_range);
}

TEST(TypedListCell, front_and_back) {
    parcel::TypedListCell<parcel::I32Cell> v{1, 2, 3};
    EXPECT_EQ(v.front(), 1);
    EXPECT_EQ(v.back(), 3);
}

TEST(TypedListCell, pop_back_and_clear) {
    parcel::TypedListCell<parcel::I32Cell> v{1, 2, 3};
    v.pop_back();
    ASSERT_EQ(v.size(), 2u);
    EXPECT_EQ(v.back(), 2);

    v.clear();
    EXPECT_TRUE(v.empty());
}

TEST(TypedListCell, erase_and_insert) {
    parcel::TypedListCell<parcel::I32Cell> v{1, 2, 3};
    v.erase(v.begin() + 1);
    ASSERT_EQ(v.size(), 2u);
    EXPECT_EQ(v[1], 3);

    v.insert(v.begin() + 1, 42);
    ASSERT_EQ(v.size(), 3u);
    EXPECT_EQ(v[1], 42);
}

// ---------------------------------------------------------------------------
// clone()

TEST(TypedListCell, clone_produces_independent_copy) {
    parcel::TypedListCell<parcel::I32Cell> v{1, 2, 3};
    const auto cloned = v.clone();
    ASSERT_NE(cloned, nullptr);
    EXPECT_EQ(cloned->kind(), "l:i32");

    auto* typed = dynamic_cast<parcel::TypedListCell<parcel::I32Cell>*>(cloned.get());
    ASSERT_NE(typed, nullptr);
    ASSERT_EQ(typed->get().size(), 3u);
    EXPECT_EQ(typed->get()[0], 1);

    v.value.clear();
    EXPECT_EQ(typed->get().size(), 3u);
}

// ---------------------------------------------------------------------------
// descriptor()

TEST(TypedListCell, descriptor_returns_non_null_shared_ptr) {
    const auto d = parcel::TypedListCell<parcel::I32Cell>::descriptor();
    ASSERT_NE(d, nullptr);
    EXPECT_EQ(d->kind(), "l:i32");
}

TEST(TypedListCell, descriptor_is_cached_per_instantiation) {
    const auto d1 = parcel::TypedListCell<parcel::I32Cell>::descriptor();
    const auto d2 = parcel::TypedListCell<parcel::I32Cell>::descriptor();
    EXPECT_EQ(d1.get(), d2.get());
}

TEST(TypedListCell, descriptors_differ_across_element_types) {
    const auto a = parcel::TypedListCell<parcel::I32Cell>::descriptor();
    const auto b = parcel::TypedListCell<parcel::I64Cell>::descriptor();
    EXPECT_NE(a.get(), static_cast<void const*>(b.get()));
    EXPECT_EQ(a->kind(), "l:i32");
    EXPECT_EQ(b->kind(), "l:i64");
}

TEST(TypedListCell, descriptor_exposes_element_kind) {
    const auto d = parcel::TypedListCell<parcel::I32Cell>::descriptor();
    auto* typed = dynamic_cast<parcel::TypedListCellTypeDescriptor<parcel::I32Cell>*>(d.get());
    ASSERT_NE(typed, nullptr);
    EXPECT_EQ(typed->element_kind(), "i32");
}

TEST(TypedListCell, descriptor_to_json_includes_kind_meta_and_element_kind) {
    const auto d = parcel::TypedListCell<parcel::I32Cell>::descriptor();
    const auto j = d->to_json();
    EXPECT_EQ(j["kind"].get<std::string>(), "l:i32");
    EXPECT_EQ(j["element_kind"].get<std::string>(), "i32");
    ASSERT_TRUE(j.contains("meta"));
    EXPECT_EQ(j["meta"]["name"].get<std::string>(), "List");
}

TEST(TypedListCell, descriptor_cell_from_json_round_trip) {
    const parcel::TypedListCell<parcel::I32Cell> original{7, 8};
    const auto j = original.to_json();

    const auto desc = parcel::TypedListCell<parcel::I32Cell>::descriptor();
    const auto restored = desc->cell_from_json(j, registry());
    ASSERT_NE(restored, nullptr);
    auto* typed = dynamic_cast<parcel::TypedListCell<parcel::I32Cell>*>(restored.get());
    ASSERT_NE(typed, nullptr);
    ASSERT_EQ(typed->get().size(), 2u);
    EXPECT_EQ(typed->get()[0], 7);
    EXPECT_EQ(typed->get()[1], 8);
}

// ===========================================================================
// Heterogeneous ListCell
// ===========================================================================

static_assert(parcel::CellLike<parcel::ListCell>);

// ---------------------------------------------------------------------------
// Construction

TEST(ListCell, default_constructs_to_empty_vector) {
    const parcel::ListCell v;
    EXPECT_TRUE(v.empty());
    EXPECT_EQ(v.size(), 0u);
}

TEST(ListCell, constructs_from_vector) {
    std::vector<parcel::cell_t> elems{
        std::make_shared<parcel::I32Cell>(1),
        std::make_shared<parcel::StringCell>(std::string("two")),
    };
    const parcel::ListCell v{std::move(elems)};
    ASSERT_EQ(v.size(), 2u);
}

TEST(ListCell, constructs_from_initializer_list) {
    const parcel::ListCell v{
        std::make_shared<parcel::I32Cell>(1),
        std::make_shared<parcel::StringCell>(std::string("hello")),
        std::make_shared<parcel::BoolCell>(true),
    };
    ASSERT_EQ(v.size(), 3u);
}

// ---------------------------------------------------------------------------
// kind()

TEST(ListCell, kind_is_list) {
    const parcel::ListCell v;
    EXPECT_EQ(v.kind(), "l");
}

// ---------------------------------------------------------------------------
// to_string()

TEST(ListCell, to_string_empty_list) {
    const parcel::ListCell v;
    EXPECT_EQ(v.to_string(), "[]");
}

TEST(ListCell, to_string_mixed_primitives) {
    const parcel::ListCell v{
        std::make_shared<parcel::I32Cell>(1),
        std::make_shared<parcel::StringCell>(std::string("hello")),
        std::make_shared<parcel::BoolCell>(true),
    };
    EXPECT_EQ(v.to_string(), "[1, hello, true]");
}

// ---------------------------------------------------------------------------
// to_json() shape

TEST(ListCell, to_json_emits_k_and_v_array_of_wrapped_objects) {
    parcel::ListCell v{
        std::make_shared<parcel::I32Cell>(1),
        std::make_shared<parcel::StringCell>(std::string("hello")),
    };
    const auto j = v.to_json();
    ASSERT_TRUE(j.is_object());
    EXPECT_EQ(j["k"].get<std::string>(), "l");
    ASSERT_TRUE(j["v"].is_array());
    ASSERT_EQ(j["v"].size(), 2u);
    EXPECT_TRUE(j["v"][0].is_object());
    EXPECT_TRUE(j["v"][0].contains("k"));
    EXPECT_EQ(j["v"][0]["k"].get<std::string>(), "i32");
    EXPECT_EQ(j["v"][0]["v"].get<int>(), 1);
    EXPECT_TRUE(j["v"][1].is_object());
    EXPECT_TRUE(j["v"][1].contains("k"));
    EXPECT_EQ(j["v"][1]["k"].get<std::string>(), "string");
}

// ---------------------------------------------------------------------------
// from_json round-trip

TEST(ListCell, from_json_round_trip_mixed_primitives) {
    parcel::ListCell original{
        std::make_shared<parcel::I32Cell>(7),
        std::make_shared<parcel::StringCell>(std::string("hi")),
        std::make_shared<parcel::BoolCell>(false),
    };
    const auto j = original.to_json();
    auto restored = parcel::ListCell::from_json(j, populated_registry());
    ASSERT_NE(restored, nullptr);
    EXPECT_EQ(restored->kind(), "l");
    auto* typed = dynamic_cast<parcel::ListCell*>(restored.get());
    ASSERT_NE(typed, nullptr);
    ASSERT_EQ(typed->size(), 3u);

    auto* e0 = dynamic_cast<parcel::I32Cell*>((*typed)[0].get());
    ASSERT_NE(e0, nullptr);
    EXPECT_EQ(e0->get(), 7);

    auto* e1 = dynamic_cast<parcel::StringCell*>((*typed)[1].get());
    ASSERT_NE(e1, nullptr);
    EXPECT_EQ(e1->get(), "hi");

    auto* e2 = dynamic_cast<parcel::BoolCell*>((*typed)[2].get());
    ASSERT_NE(e2, nullptr);
    EXPECT_EQ(e2->get(), false);
}

// ---------------------------------------------------------------------------
// from_json error paths

TEST(ListCell, from_json_rejects_non_object) {
    parcel::json_t arr = parcel::json_t::array({1, 2});
    EXPECT_THROW(parcel::ListCell::from_json(arr, populated_registry()), std::runtime_error);
}

TEST(ListCell, from_json_rejects_missing_kind) {
    parcel::json_t j = {{"v", parcel::json_t::array()}};
    EXPECT_THROW(parcel::ListCell::from_json(j, populated_registry()), std::runtime_error);
}

TEST(ListCell, from_json_rejects_wrong_kind) {
    parcel::json_t j = {{"k", "i32"}, {"v", parcel::json_t::array()}};
    EXPECT_THROW(parcel::ListCell::from_json(j, populated_registry()), std::runtime_error);
}

TEST(ListCell, from_json_rejects_missing_v) {
    parcel::json_t j = {{"k", "l"}};
    EXPECT_THROW(parcel::ListCell::from_json(j, populated_registry()), std::runtime_error);
}

TEST(ListCell, from_json_rejects_v_not_array) {
    parcel::json_t j = {{"k", "l"}, {"v", 42}};
    EXPECT_THROW(parcel::ListCell::from_json(j, populated_registry()), std::runtime_error);
}

TEST(ListCell, from_json_rejects_unknown_element_kind) {
    parcel::json_t elem = {{"k", "no_such_kind"}, {"v", 0}};
    parcel::json_t j = {{"k", "l"}, {"v", parcel::json_t::array({elem})}};
    EXPECT_THROW(parcel::ListCell::from_json(j, populated_registry()), std::runtime_error);
}

// ---------------------------------------------------------------------------
// clone() — deep copy

TEST(ListCell, clone_is_deep) {
    const auto shared_elem = std::make_shared<parcel::I32Cell>(1);
    const parcel::ListCell v{shared_elem};

    const auto cloned = v.clone();
    ASSERT_NE(cloned, nullptr);
    auto* typed = dynamic_cast<parcel::ListCell*>(cloned.get());
    ASSERT_NE(typed, nullptr);
    ASSERT_EQ(typed->size(), 1u);

    EXPECT_NE((*typed)[0].get(), shared_elem.get())
        << "clone() must allocate fresh elements, not share pointers";

    auto* clone_elem = dynamic_cast<parcel::I32Cell*>((*typed)[0].get());
    ASSERT_NE(clone_elem, nullptr);
    EXPECT_EQ(clone_elem->get(), 1);

    shared_elem->value = 999;
    EXPECT_EQ(clone_elem->get(), 1) << "mutating original element must not affect clone";
}

// ---------------------------------------------------------------------------
// descriptor()

TEST(ListCell, descriptor_returns_non_null_shared_ptr) {
    const auto d = parcel::ListCell::descriptor();
    ASSERT_NE(d, nullptr);
    EXPECT_EQ(d->kind(), "l");
}

TEST(ListCell, descriptor_is_cached) {
    const auto d1 = parcel::ListCell::descriptor();
    const auto d2 = parcel::ListCell::descriptor();
    EXPECT_EQ(d1.get(), d2.get());
}

TEST(ListCell, descriptor_to_json_has_kind_meta_no_element_kind) {
    const auto d = parcel::ListCell::descriptor();
    const auto j = d->to_json();
    EXPECT_EQ(j["kind"].get<std::string>(), "l");
    ASSERT_TRUE(j.contains("meta"));
    EXPECT_FALSE(j.contains("element_kind"))
        << "heterogeneous descriptor must not advertise an element_kind";
}

// ---------------------------------------------------------------------------
// TypedListCell<BoolCell> exercises the bool-element instantiation of
// to_string and to_json (otherwise only int / string elements are reached).

TEST(TypedListCell, to_string_bool_list) {
    const parcel::TypedListCell<parcel::BoolCell> v{true, false, true};
    EXPECT_EQ(v.to_string(), "[true, false, true]");
}

TEST(TypedListCell, to_json_bool_list_emits_raw_bools) {
    const parcel::TypedListCell<parcel::BoolCell> v{true, false};
    const auto j = v.to_json();
    EXPECT_EQ(j["k"].get<std::string>(), "l:bool");
    ASSERT_TRUE(j["v"].is_array());
    EXPECT_TRUE(j["v"][0].is_boolean());
    EXPECT_TRUE(j["v"][0].get<bool>());
    EXPECT_FALSE(j["v"][1].get<bool>());
}

// ---------------------------------------------------------------------------
// ListCell std::vector pass-through API
// (heterogeneous container — typed-list tests can't reach these methods.)

TEST(ListCell, reserve_does_not_change_size) {
    parcel::ListCell v;
    v.reserve(16);
    EXPECT_EQ(v.size(), 0u);
    EXPECT_TRUE(v.empty());
}

TEST(ListCell, indexed_access_mutable_and_const) {
    parcel::ListCell v{
        std::make_shared<parcel::I32Cell>(1),
        std::make_shared<parcel::I32Cell>(2),
    };
    auto* m = dynamic_cast<parcel::I32Cell*>(v[0].get());
    ASSERT_NE(m, nullptr);
    EXPECT_EQ(m->get(), 1);

    parcel::ListCell const& cv = v;
    const auto* c = dynamic_cast<parcel::I32Cell const*>(cv[1].get());
    ASSERT_NE(c, nullptr);
    EXPECT_EQ(c->get(), 2);
}

TEST(ListCell, at_returns_element_and_const_overload) {
    parcel::ListCell v{std::make_shared<parcel::I32Cell>(7)};
    EXPECT_NE(v.at(0), nullptr);

    parcel::ListCell const& cv = v;
    EXPECT_NE(cv.at(0), nullptr);
}

TEST(ListCell, at_throws_on_out_of_range) {
    parcel::ListCell v;
    EXPECT_THROW((void)v.at(0), std::out_of_range);

    parcel::ListCell const& cv = v;
    EXPECT_THROW((void)cv.at(0), std::out_of_range);
}

TEST(ListCell, front_and_back_mutable_and_const) {
    parcel::ListCell v{
        std::make_shared<parcel::I32Cell>(1),
        std::make_shared<parcel::I32Cell>(2),
        std::make_shared<parcel::I32Cell>(3),
    };
    EXPECT_EQ(dynamic_cast<parcel::I32Cell*>(v.front().get())->get(), 1);
    EXPECT_EQ(dynamic_cast<parcel::I32Cell*>(v.back().get())->get(), 3);

    parcel::ListCell const& cv = v;
    EXPECT_EQ(dynamic_cast<parcel::I32Cell const*>(cv.front().get())->get(), 1);
    EXPECT_EQ(dynamic_cast<parcel::I32Cell const*>(cv.back().get())->get(), 3);
}

TEST(ListCell, iteration_mutable_and_const) {
    parcel::ListCell v{
        std::make_shared<parcel::I32Cell>(1),
        std::make_shared<parcel::I32Cell>(2),
        std::make_shared<parcel::I32Cell>(3),
    };

    int sum = 0;
    for (auto it = v.begin(); it != v.end(); ++it) {
        sum += dynamic_cast<parcel::I32Cell&>(**it).get();
    }
    EXPECT_EQ(sum, 6);

    parcel::ListCell const& cv = v;
    int csum = 0;
    for (auto it = cv.begin(); it != cv.end(); ++it) {
        csum += dynamic_cast<parcel::I32Cell const&>(**it).get();
    }
    EXPECT_EQ(csum, 6);

    int ccsum = 0;
    for (auto it = v.cbegin(); it != v.cend(); ++it) {
        ccsum += dynamic_cast<parcel::I32Cell const&>(**it).get();
    }
    EXPECT_EQ(ccsum, 6);
}

TEST(ListCell, push_back_lvalue_and_rvalue) {
    parcel::ListCell v;
    const parcel::cell_t lvalue = std::make_shared<parcel::I32Cell>(1);
    v.push_back(lvalue);
    v.push_back(std::make_shared<parcel::I32Cell>(2));
    ASSERT_EQ(v.size(), 2u);
    EXPECT_EQ(dynamic_cast<parcel::I32Cell*>(v[0].get())->get(), 1);
    EXPECT_EQ(dynamic_cast<parcel::I32Cell*>(v[1].get())->get(), 2);
}

TEST(ListCell, emplace_back_constructs_in_place) {
    parcel::ListCell v;
    const auto& ref = v.emplace_back(std::make_shared<parcel::I32Cell>(42));
    EXPECT_EQ(dynamic_cast<parcel::I32Cell*>(ref.get())->get(), 42);
    EXPECT_EQ(v.size(), 1u);
}

TEST(ListCell, pop_back_and_clear) {
    parcel::ListCell v{
        std::make_shared<parcel::I32Cell>(1),
        std::make_shared<parcel::I32Cell>(2),
    };
    v.pop_back();
    EXPECT_EQ(v.size(), 1u);

    v.clear();
    EXPECT_TRUE(v.empty());
}

TEST(ListCell, erase_single_and_range) {
    parcel::ListCell v{
        std::make_shared<parcel::I32Cell>(1),
        std::make_shared<parcel::I32Cell>(2),
        std::make_shared<parcel::I32Cell>(3),
        std::make_shared<parcel::I32Cell>(4),
    };
    v.erase(v.begin() + 1);
    ASSERT_EQ(v.size(), 3u);
    EXPECT_EQ(dynamic_cast<parcel::I32Cell*>(v[1].get())->get(), 3);

    v.erase(v.begin(), v.begin() + 2);
    ASSERT_EQ(v.size(), 1u);
    EXPECT_EQ(dynamic_cast<parcel::I32Cell*>(v[0].get())->get(), 4);
}

TEST(ListCell, insert_lvalue_and_rvalue) {
    parcel::ListCell v;
    const parcel::cell_t lvalue = std::make_shared<parcel::I32Cell>(1);
    v.insert(v.begin(), lvalue);
    v.insert(v.end(), std::make_shared<parcel::I32Cell>(99));
    ASSERT_EQ(v.size(), 2u);
    EXPECT_EQ(dynamic_cast<parcel::I32Cell*>(v[0].get())->get(), 1);
    EXPECT_EQ(dynamic_cast<parcel::I32Cell*>(v[1].get())->get(), 99);
}

// ---------------------------------------------------------------------------
// ListCell serialization edge cases

TEST(ListCell, to_string_renders_null_element_as_placeholder) {
    const parcel::ListCell v{
        std::make_shared<parcel::I32Cell>(1),
        nullptr,
        std::make_shared<parcel::I32Cell>(3),
    };
    EXPECT_EQ(v.to_string(), "[1, <null>, 3]");
}

TEST(ListCell, to_json_emits_null_for_null_element) {
    const parcel::ListCell v{
        std::make_shared<parcel::I32Cell>(1),
        nullptr,
    };
    const auto j = v.to_json();
    ASSERT_TRUE(j["v"].is_array());
    ASSERT_EQ(j["v"].size(), 2u);
    EXPECT_TRUE(j["v"][1].is_null());
}

TEST(ListCell, clone_preserves_null_elements) {
    const parcel::ListCell v{
        std::make_shared<parcel::I32Cell>(1),
        nullptr,
    };
    const auto cloned = v.clone();
    auto* typed = dynamic_cast<parcel::ListCell*>(cloned.get());
    ASSERT_NE(typed, nullptr);
    ASSERT_EQ(typed->size(), 2u);
    EXPECT_EQ(dynamic_cast<parcel::I32Cell*>((*typed)[0].get())->get(), 1);
    EXPECT_EQ((*typed)[1], nullptr);
}

TEST(ListCell, from_json_round_trips_null_element) {
    const parcel::ListCell src{
        std::make_shared<parcel::I32Cell>(1),
        nullptr,
        std::make_shared<parcel::I32Cell>(3),
    };
    const auto j = src.to_json();
    const auto restored = parcel::ListCell::from_json(j, populated_registry());
    auto* typed = dynamic_cast<parcel::ListCell*>(restored.get());
    ASSERT_NE(typed, nullptr);
    ASSERT_EQ(typed->size(), 3u);
    EXPECT_EQ(dynamic_cast<parcel::I32Cell*>((*typed)[0].get())->get(), 1);
    EXPECT_EQ((*typed)[1], nullptr);
    EXPECT_EQ(dynamic_cast<parcel::I32Cell*>((*typed)[2].get())->get(), 3);
}

TEST(ListCell, descriptor_reports_list_category) {
    EXPECT_EQ(parcel::ListCell::descriptor()->category(), parcel::descriptor::CellCategory::List);
}

// ---------------------------------------------------------------------------
// Defensive dynamic_cast guard in TypedListCell::from_json

namespace {

// LiarCell claims to be CellLike with kind_id "liar", but its from_json
// returns an I32Cell instead of a LiarCell. Used to trigger the
// "element from_json did not return T" guard.
class LiarCell : public parcel::BaseCell<LiarCell, std::int32_t> {
    using base_t = parcel::BaseCell<LiarCell, std::int32_t>;

public:
    using base_t::base_t;
    using base_t::operator=;

    [[maybe_unused]] static constexpr std::string_view kind_id = "liar";

    [[nodiscard]] std::string to_string() const override {
        return std::to_string(this->value);
    }

    [[maybe_unused]] static parcel::cell_t from_json(parcel::json_t const&,
                                                     parcel::ParcelRegistry const&) {
        return std::make_shared<parcel::I32Cell>(0);  // wrong concrete type
    }

    [[maybe_unused]] static parcel::cell_type_descriptor_t descriptor() {
        static const auto d = std::make_shared<parcel::SimpleCellTypeDescriptor<LiarCell>>(
            parcel::descriptor::MetaInfo{.name = "Liar"});
        return d;
    }
};

}  // namespace

TEST(TypedListCell, from_json_throws_when_element_dynamic_cast_fails) {
    parcel::json_t j = {
        {"k", "l:liar"},
        {"v", {0}},
    };
    EXPECT_THROW((void)parcel::TypedListCell<LiarCell>::from_json(j, registry()),
                 std::runtime_error);
}
