#include <parcel/parcel.h>

#include <gtest/gtest.h>

#include <map>
#include <memory>
#include <string>

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

static_assert(parcel::CellLike<parcel::TypedMapCell<parcel::I32Cell>>);
static_assert(parcel::CellLike<parcel::TypedMapCell<parcel::StringCell>>);
static_assert(parcel::CellLike<parcel::TypedMapCell<parcel::BoolCell>>);

// ---------------------------------------------------------------------------
// Construction + value access

TEST(TypedMapCell, default_constructs_to_empty_map) {
    const parcel::TypedMapCell<parcel::I32Cell> v;
    EXPECT_TRUE(v.empty());
    EXPECT_EQ(v.size(), 0u);
}

TEST(TypedMapCell, constructs_from_initializer_list) {
    parcel::TypedMapCell<parcel::I32Cell> v{{"alice", 1}, {"bob", 2}};
    ASSERT_EQ(v.size(), 2u);
    EXPECT_EQ(v.at("alice"), 1);
    EXPECT_EQ(v.at("bob"), 2);
}

TEST(TypedMapCell, constructs_from_map) {
    std::map<std::string, std::int32_t> m{{"x", 10}};
    parcel::TypedMapCell<parcel::I32Cell> v{std::move(m)};
    ASSERT_EQ(v.size(), 1u);
    EXPECT_EQ(v.at("x"), 10);
}

// ---------------------------------------------------------------------------
// kind()

TEST(TypedMapCell, kind_encodes_element_type) {
    EXPECT_EQ(parcel::TypedMapCell<parcel::I32Cell>{}.kind(), "m:i32");
    EXPECT_EQ(parcel::TypedMapCell<parcel::StringCell>{}.kind(), "m:string");
    EXPECT_EQ(parcel::TypedMapCell<parcel::BoolCell>{}.kind(), "m:bool");
}

// ---------------------------------------------------------------------------
// to_string()

TEST(TypedMapCell, to_string_empty_dict) {
    const parcel::TypedMapCell<parcel::I32Cell> v;
    EXPECT_EQ(v.to_string(), "{}");
}

TEST(TypedMapCell, to_string_int_dict_sorted_by_key) {
    const parcel::TypedMapCell<parcel::I32Cell> v{{"b", 2}, {"a", 1}};
    EXPECT_EQ(v.to_string(), "{a: 1, b: 2}");
}

// ---------------------------------------------------------------------------
// to_json() shape

TEST(TypedMapCell, to_json_emits_k_v_keys) {
    const parcel::TypedMapCell<parcel::I32Cell> v{{"alice", 1}, {"bob", 2}};
    const auto j = v.to_json();
    ASSERT_TRUE(j.is_object());
    EXPECT_EQ(j["k"].get<std::string>(), "m:i32");
    ASSERT_TRUE(j["v"].is_object());
    EXPECT_EQ(j["v"]["alice"].get<int>(), 1);
    EXPECT_EQ(j["v"]["bob"].get<int>(), 2);
}

TEST(TypedMapCell, to_json_v_contains_raw_scalars_not_objects) {
    const parcel::TypedMapCell<parcel::I32Cell> v{{"x", 42}};
    const auto j = v.to_json();
    ASSERT_TRUE(j["v"].is_object());
    EXPECT_TRUE(j["v"]["x"].is_number()) << "values must be raw scalars, not {k,v} objects";
}

TEST(TypedMapCell, to_json_empty_dict_v_is_empty_object) {
    const parcel::TypedMapCell<parcel::I32Cell> v;
    const auto j = v.to_json();
    ASSERT_TRUE(j["v"].is_object());
    EXPECT_EQ(j["v"].size(), 0u);
    EXPECT_EQ(j["k"].get<std::string>(), "m:i32");
}

// ---------------------------------------------------------------------------
// from_json round-trip

TEST(TypedMapCell, from_json_round_trip_int) {
    const parcel::TypedMapCell<parcel::I32Cell> original{{"alice", 1}, {"bob", 2}};
    const auto j = original.to_json();
    const auto restored = parcel::TypedMapCell<parcel::I32Cell>::from_json(j, registry());
    ASSERT_NE(restored, nullptr);
    EXPECT_EQ(restored->kind(), "m:i32");
    auto* typed = dynamic_cast<parcel::TypedMapCell<parcel::I32Cell>*>(restored.get());
    ASSERT_NE(typed, nullptr);
    ASSERT_EQ(typed->size(), 2u);
    EXPECT_EQ(typed->at("alice"), 1);
    EXPECT_EQ(typed->at("bob"), 2);
}

TEST(TypedMapCell, from_json_round_trip_string) {
    const parcel::TypedMapCell<parcel::StringCell> original{{"a", std::string("hello")},
                                                            {"b", std::string("world")}};
    const auto j = original.to_json();
    const auto restored = parcel::TypedMapCell<parcel::StringCell>::from_json(j, registry());
    auto* typed = dynamic_cast<parcel::TypedMapCell<parcel::StringCell>*>(restored.get());
    ASSERT_NE(typed, nullptr);
    ASSERT_EQ(typed->size(), 2u);
    EXPECT_EQ(typed->at("a"), "hello");
    EXPECT_EQ(typed->at("b"), "world");
}

TEST(TypedMapCell, from_json_round_trip_empty_dict) {
    const parcel::TypedMapCell<parcel::I32Cell> original;
    const auto j = original.to_json();
    const auto restored = parcel::TypedMapCell<parcel::I32Cell>::from_json(j, registry());
    auto* typed = dynamic_cast<parcel::TypedMapCell<parcel::I32Cell>*>(restored.get());
    ASSERT_NE(typed, nullptr);
    EXPECT_TRUE(typed->empty());
}

// ---------------------------------------------------------------------------
// from_json error paths

TEST(TypedMapCell, from_json_rejects_non_object) {
    parcel::json_t arr = parcel::json_t::array({1, 2, 3});
    EXPECT_THROW(parcel::TypedMapCell<parcel::I32Cell>::from_json(arr, registry()),
                 std::runtime_error);
}

TEST(TypedMapCell, from_json_rejects_missing_kind) {
    parcel::json_t j = {{"v", parcel::json_t::object()}};
    EXPECT_THROW(parcel::TypedMapCell<parcel::I32Cell>::from_json(j, registry()),
                 std::runtime_error);
}

TEST(TypedMapCell, from_json_rejects_wrong_kind) {
    parcel::json_t j = {{"k", "i32"}, {"v", parcel::json_t::object()}};
    EXPECT_THROW(parcel::TypedMapCell<parcel::I32Cell>::from_json(j, registry()),
                 std::runtime_error);
}

TEST(TypedMapCell, from_json_rejects_mismatched_element_kind_via_kind) {
    // Element kind is part of 'k' now, so a payload tagged "m:i64" is simply
    // not the same kind as TypedMapCell<I32Cell> ("m:i32"); it must throw.
    parcel::json_t j = {{"k", "m:i64"}, {"v", parcel::json_t::object()}};
    EXPECT_THROW(parcel::TypedMapCell<parcel::I32Cell>::from_json(j, registry()),
                 std::runtime_error);
}

TEST(TypedMapCell, from_json_rejects_missing_v) {
    parcel::json_t j = {{"k", "m:i32"}};
    EXPECT_THROW(parcel::TypedMapCell<parcel::I32Cell>::from_json(j, registry()),
                 std::runtime_error);
}

TEST(TypedMapCell, from_json_rejects_v_not_object) {
    parcel::json_t j = {{"k", "m:i32"}, {"v", parcel::json_t::array({1})}};
    EXPECT_THROW(parcel::TypedMapCell<parcel::I32Cell>::from_json(j, registry()),
                 std::runtime_error);
}

TEST(TypedMapCell, from_json_rejects_invalid_element) {
    parcel::json_t v = parcel::json_t::object();
    v["x"] = "not-an-int";
    const parcel::json_t j = {{"k", "m:i32"}, {"v", std::move(v)}};
    EXPECT_ANY_THROW(parcel::TypedMapCell<parcel::I32Cell>::from_json(j, registry()));
}

// ---------------------------------------------------------------------------
// std::map pass-through API

TEST(TypedMapCell, size_and_empty) {
    parcel::TypedMapCell<parcel::I32Cell> v;
    EXPECT_TRUE(v.empty());
    v["x"] = 1;
    EXPECT_FALSE(v.empty());
    EXPECT_EQ(v.size(), 1u);
}

TEST(TypedMapCell, range_based_for_iterates_pairs_sorted_by_key) {
    parcel::TypedMapCell<parcel::I32Cell> v{{"b", 2}, {"a", 1}};
    std::string keys;
    for (const auto& key : v | std::views::keys) {
        keys += key;
    }
    EXPECT_EQ(keys, "ab");
}

TEST(TypedMapCell, operator_index_inserts_default) {
    parcel::TypedMapCell<parcel::I32Cell> v;
    v["alice"] = 42;
    EXPECT_EQ(v["alice"], 42);
    EXPECT_EQ(v.size(), 1u);
}

TEST(TypedMapCell, at_throws_on_missing_key) {
    parcel::TypedMapCell<parcel::I32Cell> v;
    EXPECT_THROW((void)v.at("missing"), std::out_of_range);
}

TEST(TypedMapCell, find_and_contains) {
    parcel::TypedMapCell<parcel::I32Cell> v{{"a", 1}};
    EXPECT_TRUE(v.contains("a"));
    EXPECT_FALSE(v.contains("zzz"));
    EXPECT_NE(v.find("a"), v.end());
    EXPECT_EQ(v.find("zzz"), v.end());
}

TEST(TypedMapCell, insert_emplace_erase_clear) {
    parcel::TypedMapCell<parcel::I32Cell> v;
    auto [it1, ok1] = v.insert({"a", 1});
    EXPECT_TRUE(ok1);
    EXPECT_EQ(it1->first, "a");

    auto [it2, ok2] = v.emplace("b", 2);
    EXPECT_TRUE(ok2);
    EXPECT_EQ(it2->second, 2);

    EXPECT_EQ(v.erase("a"), 1u);
    EXPECT_EQ(v.size(), 1u);

    v.clear();
    EXPECT_TRUE(v.empty());
}

// ---------------------------------------------------------------------------
// clone()

TEST(TypedMapCell, clone_produces_independent_copy) {
    parcel::TypedMapCell<parcel::I32Cell> v{{"a", 1}, {"b", 2}};
    const auto cloned = v.clone();
    ASSERT_NE(cloned, nullptr);
    EXPECT_EQ(cloned->kind(), "m:i32");

    auto* typed = dynamic_cast<parcel::TypedMapCell<parcel::I32Cell>*>(cloned.get());
    ASSERT_NE(typed, nullptr);
    EXPECT_EQ(typed->size(), 2u);

    v.clear();
    EXPECT_EQ(typed->size(), 2u);
}

// ---------------------------------------------------------------------------
// descriptor()

TEST(TypedMapCell, descriptor_returns_non_null_shared_ptr) {
    const auto d = parcel::TypedMapCell<parcel::I32Cell>::descriptor();
    ASSERT_NE(d, nullptr);
    EXPECT_EQ(d->kind(), "m:i32");
}

TEST(TypedMapCell, descriptor_is_cached_per_instantiation) {
    const auto d1 = parcel::TypedMapCell<parcel::I32Cell>::descriptor();
    const auto d2 = parcel::TypedMapCell<parcel::I32Cell>::descriptor();
    EXPECT_EQ(d1.get(), d2.get());
}

TEST(TypedMapCell, descriptors_differ_across_element_types) {
    const auto a = parcel::TypedMapCell<parcel::I32Cell>::descriptor();
    const auto b = parcel::TypedMapCell<parcel::I64Cell>::descriptor();
    EXPECT_NE(a.get(), static_cast<void const*>(b.get()));
}

TEST(TypedMapCell, descriptor_exposes_element_kind) {
    const auto d = parcel::TypedMapCell<parcel::I32Cell>::descriptor();
    auto* typed = dynamic_cast<parcel::TypedMapCellTypeDescriptor<parcel::I32Cell>*>(d.get());
    ASSERT_NE(typed, nullptr);
    EXPECT_EQ(typed->element_kind(), "i32");
}

TEST(TypedMapCell, descriptor_to_json_includes_kind_meta_and_element_kind) {
    const auto d = parcel::TypedMapCell<parcel::I32Cell>::descriptor();
    const auto j = d->to_json();
    EXPECT_EQ(j["kind"].get<std::string>(), "m:i32");
    EXPECT_EQ(j["element_kind"].get<std::string>(), "i32");
    ASSERT_TRUE(j.contains("meta"));
    EXPECT_EQ(j["meta"]["name"].get<std::string>(), "Map");
}

TEST(TypedMapCell, descriptor_cell_from_json_round_trip) {
    const parcel::TypedMapCell<parcel::I32Cell> original{{"x", 7}, {"y", 8}};
    const auto j = original.to_json();

    const auto desc = parcel::TypedMapCell<parcel::I32Cell>::descriptor();
    const auto restored = desc->cell_from_json(j, registry());
    ASSERT_NE(restored, nullptr);
    auto* typed = dynamic_cast<parcel::TypedMapCell<parcel::I32Cell>*>(restored.get());
    ASSERT_NE(typed, nullptr);
    EXPECT_EQ(typed->at("x"), 7);
    EXPECT_EQ(typed->at("y"), 8);
}

// ===========================================================================
// Heterogeneous MapCell
// ===========================================================================

static_assert(parcel::CellLike<parcel::MapCell>);

// ---------------------------------------------------------------------------
// Construction

TEST(MapCell, default_constructs_to_empty_map) {
    const parcel::MapCell v;
    EXPECT_TRUE(v.empty());
    EXPECT_EQ(v.size(), 0u);
}

TEST(MapCell, constructs_from_map) {
    std::map<std::string, parcel::cell_t> m{
        {"x", std::make_shared<parcel::I32Cell>(10)},
    };
    const parcel::MapCell v{std::move(m)};
    ASSERT_EQ(v.size(), 1u);
}

TEST(MapCell, constructs_from_initializer_list) {
    const parcel::MapCell v{
        {"name", std::make_shared<parcel::StringCell>(std::string("alice"))},
        {"age", std::make_shared<parcel::I32Cell>(30)},
    };
    ASSERT_EQ(v.size(), 2u);
}

// ---------------------------------------------------------------------------
// kind()

TEST(MapCell, kind_is_map) {
    const parcel::MapCell v;
    EXPECT_EQ(v.kind(), "m");
}

// ---------------------------------------------------------------------------
// to_string()

TEST(MapCell, to_string_empty_dict) {
    const parcel::MapCell v;
    EXPECT_EQ(v.to_string(), "{}");
}

TEST(MapCell, to_string_mixed_primitives_sorted_by_key) {
    const parcel::MapCell v{
        {"name", std::make_shared<parcel::StringCell>(std::string("alice"))},
        {"age", std::make_shared<parcel::I32Cell>(30)},
    };
    EXPECT_EQ(v.to_string(), "{age: 30, name: alice}");
}

// ---------------------------------------------------------------------------
// to_json() shape

TEST(MapCell, to_json_emits_k_and_v_object_of_wrapped_objects) {
    const parcel::MapCell v{
        {"age", std::make_shared<parcel::I32Cell>(30)},
        {"name", std::make_shared<parcel::StringCell>(std::string("alice"))},
    };
    const auto j = v.to_json();
    ASSERT_TRUE(j.is_object());
    EXPECT_EQ(j["k"].get<std::string>(), "m");
    ASSERT_TRUE(j["v"].is_object());
    ASSERT_EQ(j["v"].size(), 2u);
    ASSERT_TRUE(j["v"]["age"].is_object());
    EXPECT_TRUE(j["v"]["age"].contains("k"));
    EXPECT_EQ(j["v"]["age"]["k"].get<std::string>(), "i32");
    EXPECT_EQ(j["v"]["age"]["v"].get<int>(), 30);
    ASSERT_TRUE(j["v"]["name"].is_object());
    EXPECT_EQ(j["v"]["name"]["k"].get<std::string>(), "string");
}

// ---------------------------------------------------------------------------
// from_json round-trip

TEST(MapCell, from_json_round_trip_mixed_primitives) {
    parcel::MapCell original{
        {"name", std::make_shared<parcel::StringCell>(std::string("alice"))},
        {"age", std::make_shared<parcel::I32Cell>(30)},
        {"active", std::make_shared<parcel::BoolCell>(true)},
    };
    const auto j = original.to_json();
    auto restored = parcel::MapCell::from_json(j, populated_registry());
    ASSERT_NE(restored, nullptr);
    EXPECT_EQ(restored->kind(), "m");
    auto* typed = dynamic_cast<parcel::MapCell*>(restored.get());
    ASSERT_NE(typed, nullptr);
    ASSERT_EQ(typed->size(), 3u);

    auto* name = dynamic_cast<parcel::StringCell*>(typed->at("name").get());
    ASSERT_NE(name, nullptr);
    EXPECT_EQ(name->get(), "alice");

    auto* age = dynamic_cast<parcel::I32Cell*>(typed->at("age").get());
    ASSERT_NE(age, nullptr);
    EXPECT_EQ(age->get(), 30);

    auto* active = dynamic_cast<parcel::BoolCell*>(typed->at("active").get());
    ASSERT_NE(active, nullptr);
    EXPECT_EQ(active->get(), true);
}

// ---------------------------------------------------------------------------
// from_json error paths

TEST(MapCell, from_json_rejects_non_object) {
    parcel::json_t arr = parcel::json_t::array({1, 2});
    EXPECT_THROW(parcel::MapCell::from_json(arr, populated_registry()), std::runtime_error);
}

TEST(MapCell, from_json_rejects_missing_kind) {
    parcel::json_t j = {{"v", parcel::json_t::object()}};
    EXPECT_THROW(parcel::MapCell::from_json(j, populated_registry()), std::runtime_error);
}

TEST(MapCell, from_json_rejects_wrong_kind) {
    parcel::json_t j = {{"k", "l"}, {"v", parcel::json_t::object()}};
    EXPECT_THROW(parcel::MapCell::from_json(j, populated_registry()), std::runtime_error);
}

TEST(MapCell, from_json_rejects_missing_v) {
    parcel::json_t j = {{"k", "m"}};
    EXPECT_THROW(parcel::MapCell::from_json(j, populated_registry()), std::runtime_error);
}

TEST(MapCell, from_json_rejects_v_not_object) {
    parcel::json_t j = {{"k", "m"}, {"v", parcel::json_t::array({1})}};
    EXPECT_THROW(parcel::MapCell::from_json(j, populated_registry()), std::runtime_error);
}

TEST(MapCell, from_json_rejects_unknown_element_kind) {
    parcel::json_t inner = parcel::json_t::object();
    inner["x"] = parcel::json_t{{"k", "no_such_kind"}, {"v", 0}};
    parcel::json_t j = {{"k", "m"}, {"v", std::move(inner)}};
    EXPECT_THROW(parcel::MapCell::from_json(j, populated_registry()), std::runtime_error);
}

// ---------------------------------------------------------------------------
// clone() — deep copy

TEST(MapCell, clone_is_deep) {
    auto shared_elem = std::make_shared<parcel::I32Cell>(1);
    const parcel::MapCell v{{"x", shared_elem}};

    const auto cloned = v.clone();
    ASSERT_NE(cloned, nullptr);
    auto* typed = dynamic_cast<parcel::MapCell*>(cloned.get());
    ASSERT_NE(typed, nullptr);
    ASSERT_EQ(typed->size(), 1u);

    EXPECT_NE(typed->at("x").get(), shared_elem.get())
        << "clone() must allocate fresh elements, not share pointers";

    auto* clone_elem = dynamic_cast<parcel::I32Cell*>(typed->at("x").get());
    ASSERT_NE(clone_elem, nullptr);
    EXPECT_EQ(clone_elem->get(), 1);

    shared_elem->value = 999;
    EXPECT_EQ(clone_elem->get(), 1) << "mutating original element must not affect clone";
}

// ---------------------------------------------------------------------------
// descriptor()

TEST(MapCell, descriptor_returns_non_null_shared_ptr) {
    const auto d = parcel::MapCell::descriptor();
    ASSERT_NE(d, nullptr);
    EXPECT_EQ(d->kind(), "m");
}

TEST(MapCell, descriptor_is_cached) {
    const auto d1 = parcel::MapCell::descriptor();
    const auto d2 = parcel::MapCell::descriptor();
    EXPECT_EQ(d1.get(), d2.get());
}

TEST(MapCell, descriptor_to_json_has_kind_meta_no_element_kind) {
    const auto d = parcel::MapCell::descriptor();
    const auto j = d->to_json();
    EXPECT_EQ(j["kind"].get<std::string>(), "m");
    ASSERT_TRUE(j.contains("meta"));
    EXPECT_FALSE(j.contains("element_kind"))
        << "heterogeneous descriptor must not advertise an element_kind";
}

// ---------------------------------------------------------------------------
// TypedMapCell<BoolCell> exercises the bool-element instantiation of
// to_string and to_json (otherwise only int / string elements are reached).

TEST(TypedMapCell, to_string_bool_dict_sorted_by_key) {
    const parcel::TypedMapCell<parcel::BoolCell> v{{"b", false}, {"a", true}};
    EXPECT_EQ(v.to_string(), "{a: true, b: false}");
}

TEST(TypedMapCell, to_json_bool_dict_emits_raw_bools) {
    const parcel::TypedMapCell<parcel::BoolCell> v{{"on", true}, {"off", false}};
    const auto j = v.to_json();
    EXPECT_EQ(j["k"].get<std::string>(), "m:bool");
    ASSERT_TRUE(j["v"].is_object());
    EXPECT_TRUE(j["v"]["on"].is_boolean());
    EXPECT_TRUE(j["v"]["on"].get<bool>());
    EXPECT_FALSE(j["v"]["off"].get<bool>());
}

// ---------------------------------------------------------------------------
// TypedMapCell extra std::map pass-through methods

TEST(TypedMapCell, const_at_returns_value) {
    parcel::TypedMapCell<parcel::I32Cell> v{{"x", 7}};
    parcel::TypedMapCell<parcel::I32Cell> const& cv = v;
    EXPECT_EQ(cv.at("x"), 7);
    EXPECT_THROW((void)cv.at("missing"), std::out_of_range);
}

TEST(TypedMapCell, const_find_count_and_iteration) {
    const parcel::TypedMapCell<parcel::I32Cell> v{{"a", 1}, {"b", 2}};
    parcel::TypedMapCell<parcel::I32Cell> const& cv = v;

    EXPECT_NE(cv.find("a"), cv.end());
    EXPECT_EQ(cv.find("zz"), cv.end());

    EXPECT_EQ(cv.count("a"), 1u);
    EXPECT_EQ(cv.count("zz"), 0u);

    int sum = 0;
    for (auto it = cv.begin(); it != cv.end(); ++it) {
        sum += it->second;
    }
    EXPECT_EQ(sum, 3);

    int csum = 0;
    for (auto it = cv.cbegin(); it != cv.cend(); ++it) {
        csum += it->second;
    }
    EXPECT_EQ(csum, 3);
}

TEST(TypedMapCell, operator_index_const_key_inserts) {
    parcel::TypedMapCell<parcel::I32Cell> v;
    const std::string key = "alice";
    v[key] = 5;
    EXPECT_EQ(v.at("alice"), 5);
}

TEST(TypedMapCell, insert_lvalue_overload_returns_iterator_pair) {
    parcel::TypedMapCell<parcel::I32Cell> v;
    const parcel::TypedMapCell<parcel::I32Cell>::value_type pair{"a", 1};
    auto [it, ok] = v.insert(pair);
    EXPECT_TRUE(ok);
    EXPECT_EQ(it->first, "a");

    auto [it2, ok2] = v.insert(pair);
    EXPECT_FALSE(ok2);
}

TEST(TypedMapCell, erase_by_iterator_returns_next) {
    parcel::TypedMapCell<parcel::I32Cell> v{{"a", 1}, {"b", 2}, {"c", 3}};
    const auto next = v.erase(v.find("b"));
    EXPECT_NE(next, v.end());
    EXPECT_EQ(next->first, "c");
    EXPECT_EQ(v.size(), 2u);
}

// ---------------------------------------------------------------------------
// MapCell std::map pass-through API
// (heterogeneous variant — typed map tests can't reach these.)

TEST(MapCell, operator_index_inserts_default_cell_t) {
    parcel::MapCell v;
    v["x"] = std::make_shared<parcel::I32Cell>(42);
    EXPECT_EQ(v.size(), 1u);
    EXPECT_EQ(dynamic_cast<parcel::I32Cell*>(v.at("x").get())->get(), 42);
}

TEST(MapCell, operator_index_const_key_lvalue) {
    parcel::MapCell v;
    const std::string key = "alice";
    v[key] = std::make_shared<parcel::I32Cell>(1);
    EXPECT_EQ(v.size(), 1u);
    EXPECT_EQ(dynamic_cast<parcel::I32Cell*>(v.at("alice").get())->get(), 1);
}

TEST(MapCell, operator_index_rvalue_key) {
    parcel::MapCell v;
    std::string key = "y";
    v[std::move(key)] = std::make_shared<parcel::I32Cell>(7);
    EXPECT_EQ(dynamic_cast<parcel::I32Cell*>(v.at("y").get())->get(), 7);
}

TEST(MapCell, at_const_overload_returns_value) {
    parcel::MapCell v{{"x", std::make_shared<parcel::I32Cell>(1)}};
    parcel::MapCell const& cv = v;
    EXPECT_NE(cv.at("x"), nullptr);
    EXPECT_THROW((void)cv.at("missing"), std::out_of_range);
}

TEST(MapCell, find_contains_and_count) {
    parcel::MapCell v{{"a", std::make_shared<parcel::I32Cell>(1)}};
    EXPECT_TRUE(v.contains("a"));
    EXPECT_FALSE(v.contains("z"));
    EXPECT_NE(v.find("a"), v.end());
    EXPECT_EQ(v.find("z"), v.end());
    EXPECT_EQ(v.count("a"), 1u);
    EXPECT_EQ(v.count("z"), 0u);

    parcel::MapCell const& cv = v;
    EXPECT_NE(cv.find("a"), cv.end());
}

TEST(MapCell, iteration_mutable_const_and_cbegin) {
    parcel::MapCell v{
        {"a", std::make_shared<parcel::I32Cell>(1)},
        {"b", std::make_shared<parcel::I32Cell>(2)},
    };

    std::string keys;
    for (auto it = v.begin(); it != v.end(); ++it) {
        keys += it->first;
    }
    EXPECT_EQ(keys, "ab");

    parcel::MapCell const& cv = v;
    std::string ckeys;
    for (auto it = cv.begin(); it != cv.end(); ++it) {
        ckeys += it->first;
    }
    EXPECT_EQ(ckeys, "ab");

    std::string cckeys;
    for (auto it = v.cbegin(); it != v.cend(); ++it) {
        cckeys += it->first;
    }
    EXPECT_EQ(cckeys, "ab");
}

TEST(MapCell, insert_lvalue_and_rvalue) {
    parcel::MapCell v;
    const parcel::MapCell::value_type pair{"a", std::make_shared<parcel::I32Cell>(1)};
    auto [it1, ok1] = v.insert(pair);
    EXPECT_TRUE(ok1);

    auto [it2, ok2] =
        v.insert(parcel::MapCell::value_type{"b", std::make_shared<parcel::I32Cell>(2)});
    EXPECT_TRUE(ok2);
    EXPECT_EQ(v.size(), 2u);
}

TEST(MapCell, emplace_constructs_in_place) {
    parcel::MapCell v;
    auto [it, ok] = v.emplace("x", std::make_shared<parcel::I32Cell>(42));
    EXPECT_TRUE(ok);
    EXPECT_EQ(it->first, "x");
    EXPECT_EQ(v.size(), 1u);
}

TEST(MapCell, erase_by_key_and_by_iterator) {
    parcel::MapCell v{
        {"a", std::make_shared<parcel::I32Cell>(1)},
        {"b", std::make_shared<parcel::I32Cell>(2)},
        {"c", std::make_shared<parcel::I32Cell>(3)},
    };
    EXPECT_EQ(v.erase("a"), 1u);
    EXPECT_EQ(v.erase("missing"), 0u);

    const auto next = v.erase(v.find("b"));
    EXPECT_NE(next, v.end());
    EXPECT_EQ(next->first, "c");
    EXPECT_EQ(v.size(), 1u);
}

TEST(MapCell, clear) {
    parcel::MapCell v{{"x", std::make_shared<parcel::I32Cell>(1)}};
    v.clear();
    EXPECT_TRUE(v.empty());
}

// ---------------------------------------------------------------------------
// MapCell serialization edge cases

TEST(MapCell, to_string_renders_null_value_as_placeholder) {
    const parcel::MapCell v{
        {"a", std::make_shared<parcel::I32Cell>(1)},
        {"b", nullptr},
    };
    EXPECT_EQ(v.to_string(), "{a: 1, b: <null>}");
}

TEST(MapCell, to_json_emits_null_for_null_value) {
    const parcel::MapCell v{
        {"a", std::make_shared<parcel::I32Cell>(1)},
        {"b", nullptr},
    };
    const auto j = v.to_json();
    ASSERT_TRUE(j["v"].is_object());
    EXPECT_TRUE(j["v"]["b"].is_null());
}

TEST(MapCell, clone_preserves_null_values) {
    const parcel::MapCell v{
        {"a", std::make_shared<parcel::I32Cell>(1)},
        {"b", nullptr},
    };
    const auto cloned = v.clone();
    auto* typed = dynamic_cast<parcel::MapCell*>(cloned.get());
    ASSERT_NE(typed, nullptr);
    EXPECT_EQ(dynamic_cast<parcel::I32Cell*>(typed->at("a").get())->get(), 1);
    EXPECT_EQ(typed->at("b"), nullptr);
}

TEST(MapCell, from_json_round_trips_null_value) {
    const parcel::MapCell src{
        {"a", std::make_shared<parcel::I32Cell>(1)},
        {"b", nullptr},
    };
    const auto j = src.to_json();
    const auto restored = parcel::MapCell::from_json(j, populated_registry());
    auto* typed = dynamic_cast<parcel::MapCell*>(restored.get());
    ASSERT_NE(typed, nullptr);
    EXPECT_EQ(dynamic_cast<parcel::I32Cell*>(typed->at("a").get())->get(), 1);
    EXPECT_EQ(typed->at("b"), nullptr);
}

TEST(MapCell, descriptor_reports_map_category) {
    EXPECT_EQ(parcel::MapCell::descriptor()->category(), parcel::descriptor::CellCategory::Map);
}

// ---------------------------------------------------------------------------
// Defensive dynamic_cast guard in TypedMapCell::from_json

namespace {

// LiarCell claims to be CellLike with kind_id "liar_d", but its from_json
// returns a StringCell instead. Used to trigger the
// "element from_json did not return T" guard.
class LiarCell : public parcel::BaseCell<LiarCell, std::string> {
    using base_t = parcel::BaseCell<LiarCell, std::string>;

public:
    using base_t::base_t;
    using base_t::operator=;

    [[maybe_unused]] static constexpr std::string_view kind_id = "liar_d";

    [[nodiscard]] std::string to_string() const override {
        return this->value;
    }

    [[maybe_unused]] static parcel::cell_t from_json(parcel::json_t const&,
                                                     parcel::ParcelRegistry const&) {
        return std::make_shared<parcel::StringCell>(std::string{});  // wrong concrete type
    }

    [[maybe_unused]] static parcel::cell_type_descriptor_t descriptor() {
        static const auto d = std::make_shared<parcel::SimpleCellTypeDescriptor<LiarCell>>(
            parcel::descriptor::MetaInfo{.name = "LiarD"});
        return d;
    }
};

}  // namespace

TEST(TypedMapCell, from_json_throws_when_element_dynamic_cast_fails) {
    parcel::json_t j = {
        {"k", "m:liar_d"},
        {"v", {{"k", ""}}},
    };
    EXPECT_THROW((void)parcel::TypedMapCell<LiarCell>::from_json(j, registry()),
                 std::runtime_error);
}
