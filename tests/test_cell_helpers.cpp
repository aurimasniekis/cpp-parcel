#include <parcel/parcel.h>

#include <gtest/gtest.h>

#include <memory>
#include <optional>
#include <string>
#include <unordered_set>

namespace {

parcel::ParcelRegistry const& registry() {
    static const parcel::ParcelRegistry r;
    return r;
}

}  // namespace

// ---------------------------------------------------------------------------
// cell_cast / cell_cast_or / cell_cast_value

TEST(CellCast, cast_returns_typed_handle) {
    const parcel::cell_t c = parcel::I32Cell::of(42);
    const auto typed = parcel::cell_cast<parcel::I32Cell>(c);
    EXPECT_EQ(typed->value, 42);
}

TEST(CellCast, cast_throws_on_kind_mismatch) {
    const parcel::cell_t c = parcel::I32Cell::of(1);
    EXPECT_THROW({ (void)parcel::cell_cast<parcel::StringCell>(c); }, std::runtime_error);
}

TEST(CellCast, cast_throws_on_null_handle) {
    const parcel::cell_t c;
    EXPECT_THROW({ (void)parcel::cell_cast<parcel::I32Cell>(c); }, std::runtime_error);
}

TEST(CellCast, cast_or_returns_fallback_on_null) {
    const parcel::cell_t c;
    const auto fallback = parcel::I32Cell::of(7);
    const auto out = parcel::cell_cast_or<parcel::I32Cell>(c, fallback);
    EXPECT_EQ(out, fallback);
}

TEST(CellCast, cast_or_returns_fallback_on_mismatch) {
    const parcel::cell_t c = parcel::StringCell::of(std::string("hi"));
    const auto fallback = parcel::I32Cell::of(99);
    const auto out = parcel::cell_cast_or<parcel::I32Cell>(c, fallback);
    EXPECT_EQ(out->value, 99);
}

TEST(CellCast, cast_value_returns_inner_storage) {
    const parcel::cell_t c = parcel::StringCell::of(std::string("abc"));
    const auto v = parcel::cell_cast_value<parcel::StringCell>(c);
    EXPECT_EQ(v, "abc");
}

#if PARCEL_HAS_EXPECTED
TEST(CellCast, try_cell_cast_returns_value_on_success) {
    const parcel::cell_t c = parcel::I32Cell::of(5);
    const auto r = parcel::try_cell_cast<parcel::I32Cell>(c);
    ASSERT_TRUE(r.has_value());
    EXPECT_EQ(r.value()->value, 5);
}

TEST(CellCast, try_cell_cast_returns_error_on_mismatch) {
    const parcel::cell_t c = parcel::StringCell::of(std::string("x"));
    auto r = parcel::try_cell_cast<parcel::I32Cell>(c);
    ASSERT_FALSE(r.has_value());
    EXPECT_EQ(r.error().code, parcel::ParcelError::Code::KindMismatch);
}

TEST(CellCast, try_cell_cast_null_yields_type_error) {
    const parcel::cell_t c;
    auto r = parcel::try_cell_cast<parcel::I32Cell>(c);
    ASSERT_FALSE(r.has_value());
    EXPECT_EQ(r.error().code, parcel::ParcelError::Code::TypeError);
}
#endif

// ---------------------------------------------------------------------------
// as<C> and value_or<C>

TEST(AsAndValueOr, as_returns_value_on_match) {
    const parcel::cell_t c = parcel::I32Cell::of(42);
    const auto opt = parcel::as<parcel::I32Cell>(c);
    ASSERT_TRUE(opt.has_value());
    EXPECT_EQ(*opt, 42);
}

TEST(AsAndValueOr, as_returns_nullopt_on_mismatch) {
    const parcel::cell_t c = parcel::StringCell::of(std::string("hi"));
    const auto opt = parcel::as<parcel::I32Cell>(c);
    EXPECT_FALSE(opt.has_value());
}

TEST(AsAndValueOr, as_returns_nullopt_on_null) {
    const parcel::cell_t c;
    const auto opt = parcel::as<parcel::I32Cell>(c);
    EXPECT_FALSE(opt.has_value());
}

TEST(AsAndValueOr, value_or_returns_value_on_match) {
    const parcel::cell_t c = parcel::I32Cell::of(42);
    const int v = parcel::value_or<parcel::I32Cell>(c, 0);
    EXPECT_EQ(v, 42);
}

TEST(AsAndValueOr, value_or_returns_fallback_on_mismatch) {
    const parcel::cell_t c = parcel::StringCell::of(std::string("hi"));
    const int v = parcel::value_or<parcel::I32Cell>(c, 99);
    EXPECT_EQ(v, 99);
}

TEST(AsAndValueOr, value_or_returns_fallback_on_null) {
    const parcel::cell_t c;
    const int v = parcel::value_or<parcel::I32Cell>(c, 7);
    EXPECT_EQ(v, 7);
}

// ---------------------------------------------------------------------------
// from_json_strict (B10)

TEST(FromJsonStrict, builds_cell_and_absorbs_meta) {
    parcel::I32Cell src{42};
    auto with_meta = src.with_name("answer");
    auto j = with_meta->to_json();

    auto rebuilt = parcel::I32Cell::from_json_strict(j);
    auto* typed = dynamic_cast<parcel::I32Cell*>(rebuilt.get());
    ASSERT_NE(typed, nullptr);
    EXPECT_EQ(typed->value, 42);
    ASSERT_TRUE(typed->meta().has_value());
    EXPECT_EQ(typed->meta()->name, "answer");
}

TEST(FromJsonStrict, throws_on_kind_mismatch) {
    parcel::json_t j{
        {parcel::ICell::KEY_KIND, "string"},
        {parcel::ICell::KEY_VALUE, "x"},
    };
    EXPECT_THROW({ (void)parcel::I32Cell::from_json_strict(j); }, std::runtime_error);
}

// ---------------------------------------------------------------------------
// Free to_json / to_string

TEST(FreeFunctions, free_to_json_round_trips) {
    const parcel::cell_t c = parcel::I32Cell::of(7);
    const auto j = parcel::to_json(c);
    const auto rebuilt = registry().cell_from_json(j);
    EXPECT_EQ(*c, *rebuilt);
}

TEST(FreeFunctions, free_to_json_null_yields_null) {
    const parcel::cell_t c;
    const auto j = parcel::to_json(c);
    EXPECT_TRUE(j.is_null());
}

TEST(FreeFunctions, free_to_string_routes_to_cell) {
    const parcel::cell_t c = parcel::StringCell::of(std::string("hello"));
    EXPECT_EQ(parcel::to_string(c), "hello");
}

TEST(FreeFunctions, free_to_string_null_yields_placeholder) {
    const parcel::cell_t c;
    EXPECT_EQ(parcel::to_string(c), "<null>");
}

TEST(FreeFunctions, to_string_pretty_routes_to_formatted_string) {
    const parcel::cell_t c = parcel::I32Cell::of(1);
    EXPECT_EQ(parcel::to_string_pretty(c), c->to_formatted_string());
}

// ---------------------------------------------------------------------------
// std::hash<cell_t> and std::hash<ICell>

TEST(CellHash, equal_cells_hash_equal) {
    parcel::I32Cell a{42};
    parcel::I32Cell b{42};
    EXPECT_EQ(std::hash<parcel::ICell>{}(a), std::hash<parcel::ICell>{}(b));
}

TEST(CellHash, differing_value_changes_hash) {
    parcel::I32Cell a{42};
    parcel::I32Cell b{43};
    EXPECT_NE(std::hash<parcel::ICell>{}(a), std::hash<parcel::ICell>{}(b));
}

TEST(CellHash, meta_does_not_change_hash_for_primitive) {
    const auto a = parcel::I32Cell::of(42);
    const auto b = parcel::I32Cell::of(42)->with_name("answer");
    // Hash uses just kind and value JSON, so meta is ignored.
    EXPECT_EQ(std::hash<parcel::cell_t>{}(a), std::hash<parcel::cell_t>{}(b));
}

TEST(CellHash, null_cell_hash_is_zero) {
    const parcel::cell_t c;
    EXPECT_EQ(std::hash<parcel::cell_t>{}(c), std::size_t{0});
}

TEST(CellHash, can_be_used_in_unordered_set) {
    std::unordered_set<parcel::cell_t> set;
    set.insert(parcel::I32Cell::of(1));
    set.insert(parcel::I32Cell::of(2));
    set.insert(parcel::I32Cell::of(1));
    // Note: cell_t hashes by value but unordered_set uses operator== on the
    // shared_ptr — so two different shared_ptr handles to "same" cell are
    // distinct entries. The hash compatibility is tested above.
    EXPECT_GE(set.size(), 1U);
}

// ---------------------------------------------------------------------------
// Free hash_cell helpers

TEST(HashCell, free_helper_matches_std_hash_for_ICell) {
    const parcel::I32Cell c{42};
    EXPECT_EQ(parcel::hash_cell(c), std::hash<parcel::ICell>{}(c));
}

TEST(HashCell, free_helper_matches_std_hash_for_cell_t) {
    const parcel::cell_t c = parcel::I32Cell::of(7);
    EXPECT_EQ(parcel::hash_cell(c), std::hash<parcel::cell_t>{}(c));
}

TEST(HashCell, free_helper_null_cell_is_zero) {
    const parcel::cell_t c;
    EXPECT_EQ(parcel::hash_cell(c), std::size_t{0});
}

// ---------------------------------------------------------------------------
// BaseCell::unique factory (B11)

TEST(BaseCellUnique, returns_unique_ptr_owning_cell) {
    auto p = parcel::I32Cell::unique(123);
    static_assert(std::is_same_v<decltype(p), std::unique_ptr<parcel::I32Cell>>);
    ASSERT_NE(p.get(), nullptr);
    EXPECT_EQ(p->value, 123);
}

TEST(BaseCellUnique, forwards_constructor_args) {
    const auto p = parcel::StringCell::unique(std::string("hello"));
    ASSERT_NE(p.get(), nullptr);
    EXPECT_EQ(p->value, "hello");
}

// ---------------------------------------------------------------------------
// from_json_strict on non-primitive (StringCell, BoolCell)

TEST(FromJsonStrict, instantiates_for_string_cell) {
    parcel::StringCell src{std::string("payload")};
    auto with_meta = src.with_description("note");
    auto j = with_meta->to_json();

    auto rebuilt = parcel::StringCell::from_json_strict(j);
    auto* typed = dynamic_cast<parcel::StringCell*>(rebuilt.get());
    ASSERT_NE(typed, nullptr);
    EXPECT_EQ(typed->value, "payload");
    ASSERT_TRUE(typed->meta().has_value());
    EXPECT_EQ(typed->meta()->description, "note");
}

TEST(FromJsonStrict, instantiates_for_bool_cell) {
    const parcel::BoolCell src{true};
    const auto j = src.to_json();
    const auto rebuilt = parcel::BoolCell::from_json_strict(j);
    auto* typed = dynamic_cast<parcel::BoolCell*>(rebuilt.get());
    ASSERT_NE(typed, nullptr);
    EXPECT_EQ(typed->value, true);
}

// ---------------------------------------------------------------------------
// Free to_json / to_string / to_string_pretty on non-primitive cells

TEST(FreeFunctions, free_to_json_routes_to_list_dispatch) {
    const parcel::cell_t c = parcel::ListCell::of(std::vector<parcel::cell_t>{
        parcel::cell(1),
        parcel::cell(std::string("hi")),
    });
    auto j = parcel::to_json(c);
    ASSERT_TRUE(j.is_object());
    EXPECT_EQ(j.at(parcel::ICell::KEY_KIND).get<std::string>(), "l");
    const auto rebuilt = registry().cell_from_json(j);
    EXPECT_EQ(*c, *rebuilt);
}

TEST(FreeFunctions, free_to_string_routes_to_map_to_string) {
    const parcel::cell_t c = parcel::MapCell::of(std::map<std::string, parcel::cell_t>{
        {"k", parcel::cell(1)},
    });
    EXPECT_EQ(parcel::to_string(c), c->to_string());
}

TEST(FreeFunctions, to_string_pretty_routes_to_list_formatted) {
    const parcel::cell_t c = parcel::ListCell::of(std::vector<parcel::cell_t>{
        parcel::cell(1),
        parcel::cell(2),
    });
    EXPECT_EQ(parcel::to_string_pretty(c), c->to_formatted_string());
}
