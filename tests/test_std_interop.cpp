#include <parcel/ext/chrono.h>
#include <parcel/ext/filesystem.h>
#include <parcel/json_io.h>
#include <parcel/parcel.h>
#include <parcel/unordered_map.h>
#include <parcel/walk.h>

#include <gtest/gtest.h>

#include <chrono>
#include <cstddef>
#include <cstring>
#include <filesystem>
#include <format>
#include <sstream>
#include <string>
#include <string_view>
#include <unordered_set>
#include <vector>

namespace {

parcel::ParcelRegistry const& registry() {
    static const parcel::ParcelRegistry r;
    return r;
}

struct WalkPayload {
    std::int32_t id{0};
    std::string label;
};

class WalkPayloadCell : public parcel::StructCell<WalkPayloadCell, WalkPayload, "walk_payload"> {
public:
    using StructCell::StructCell;
    static parcel::DisplayInfo meta_info() {
        return {.name = "WalkPayload"};
    }
    static auto field_descriptors() {
        return parcel::FieldsBuilder<WalkPayload>{}
            .field<&WalkPayload::id>("id")
            .field<&WalkPayload::label>("label")
            .build();
    }
};

}  // namespace

// ---------------------------------------------------------------------------
// A1: try_cell_from_json / try_cell_from_string

#if PARCEL_HAS_EXPECTED
TEST(TryCellFromJson, success_returns_cell) {
    const parcel::json_t j{
        {parcel::ICell::KEY_KIND, "i32"},
        {parcel::ICell::KEY_VALUE, 42},
    };
    const auto r = registry().try_cell_from_json(j);
    ASSERT_TRUE(r.has_value());
    EXPECT_EQ(parcel::cell_cast_value<parcel::I32Cell>(r.value()), 42);
}

TEST(TryCellFromJson, missing_k_yields_invalid_json) {
    const parcel::json_t j{{parcel::ICell::KEY_VALUE, 1}};
    auto r = registry().try_cell_from_json(j);
    ASSERT_FALSE(r.has_value());
    EXPECT_EQ(r.error().code, parcel::ParcelError::Code::InvalidJson);
}

TEST(TryCellFromJson, unknown_kind_yields_unknown_kind) {
    const parcel::json_t j{
        {parcel::ICell::KEY_KIND, "no:such:kind"},
        {parcel::ICell::KEY_VALUE, 1},
    };
    auto r = registry().try_cell_from_json(j);
    ASSERT_FALSE(r.has_value());
    EXPECT_EQ(r.error().code, parcel::ParcelError::Code::UnknownKind);
    EXPECT_EQ(r.error().kind, "no:such:kind");
}

TEST(TryCellFromString, parses_valid_json) {
    const auto r = registry().try_cell_from_string(R"({"k":"i32","v":7})");
    ASSERT_TRUE(r.has_value());
    EXPECT_EQ(parcel::cell_cast_value<parcel::I32Cell>(r.value()), 7);
}

TEST(TryCellFromString, malformed_yields_invalid_json) {
    auto r = registry().try_cell_from_string("{not json");
    ASSERT_FALSE(r.has_value());
    EXPECT_EQ(r.error().code, parcel::ParcelError::Code::InvalidJson);
}

TEST(TryCellFromJson, non_object_input_yields_invalid_json) {
    const parcel::json_t j = 42;
    auto r = registry().try_cell_from_json(j);
    ASSERT_FALSE(r.has_value());
    EXPECT_EQ(r.error().code, parcel::ParcelError::Code::InvalidJson);
}

TEST(TryCellFromJson, dispatcher_throws_yields_type_error) {
    // Known kind ("i32") with malformed payload — the kind dispatcher resolves
    // the descriptor, then the cell's from_json throws on the bad "v".
    const parcel::json_t j{
        {parcel::ICell::KEY_KIND, "i32"},
        {parcel::ICell::KEY_VALUE, "not-an-int"},
    };
    auto r = registry().try_cell_from_json(j);
    ASSERT_FALSE(r.has_value());
    EXPECT_EQ(r.error().code, parcel::ParcelError::Code::TypeError);
    EXPECT_EQ(r.error().kind, "i32");
}
#endif

// ---------------------------------------------------------------------------
// A2: chrono cells

TEST(Chrono, unix_millis_round_trips) {
    using namespace std::chrono;
    sys_time<milliseconds> tp{milliseconds{1'700'000'000'000}};
    const parcel::UnixMillisCell c{tp};
    const auto j = c.to_json();
    const auto rebuilt = parcel::UnixMillisCell::from_json(j, registry());
    auto* typed = dynamic_cast<parcel::UnixMillisCell*>(rebuilt.get());
    ASSERT_NE(typed, nullptr);
    EXPECT_EQ(typed->value, tp);
}

TEST(Chrono, system_time_seconds_round_trips) {
    using namespace std::chrono;
    sys_seconds tp{seconds{1'700'000'000}};
    const parcel::SystemTimePointCell c{tp};
    const auto j = c.to_json();
    const auto rebuilt = parcel::SystemTimePointCell::from_json(j, registry());
    auto* typed = dynamic_cast<parcel::SystemTimePointCell*>(rebuilt.get());
    ASSERT_NE(typed, nullptr);
    EXPECT_EQ(typed->value, tp);
}

TEST(Chrono, duration_ms_round_trips) {
    const parcel::DurationMsCell c{std::chrono::milliseconds{12345}};
    const auto j = c.to_json();
    const auto rebuilt = parcel::DurationMsCell::from_json(j, registry());
    auto* typed = dynamic_cast<parcel::DurationMsCell*>(rebuilt.get());
    ASSERT_NE(typed, nullptr);
    EXPECT_EQ(typed->value.count(), 12345);
}

TEST(Chrono, ymd_round_trips) {
    using namespace std::chrono;
    year_month_day d{2026y, April, 27d};
    parcel::YmdCell c{d};
    auto j = c.to_json();
    EXPECT_EQ(j.at(parcel::ICell::KEY_VALUE).get<std::string>(), "2026-04-27");
    auto rebuilt = parcel::YmdCell::from_json(j, registry());
    auto* typed = dynamic_cast<parcel::YmdCell*>(rebuilt.get());
    ASSERT_NE(typed, nullptr);
    EXPECT_EQ(typed->value, d);
}

TEST(Chrono, default_cell_for_resolves) {
    using namespace std::chrono;
    using TS = sys_time<milliseconds>;
    using Cell = parcel::default_cell_for_t<TS>;
    static_assert(std::is_same_v<Cell, parcel::UnixMillisCell>);
    SUCCEED();
}

TEST(Chrono, timestamp_alias_is_unix_millis) {
    static_assert(std::is_same_v<parcel::TimestampMsCell, parcel::UnixMillisCell>);
    SUCCEED();
}

// ---------------------------------------------------------------------------
// A3: filesystem path cell

TEST(Filesystem, path_round_trips) {
    const parcel::PathCell c{std::filesystem::path{"/tmp/parcel/test"}};
    auto j = c.to_json();
    EXPECT_EQ(j.at(parcel::ICell::KEY_VALUE).get<std::string>(), "/tmp/parcel/test");
    const auto rebuilt = parcel::PathCell::from_json(j, registry());
    auto* typed = dynamic_cast<parcel::PathCell*>(rebuilt.get());
    ASSERT_NE(typed, nullptr);
    EXPECT_EQ(typed->value, std::filesystem::path{"/tmp/parcel/test"});
}

TEST(Filesystem, path_uses_generic_form_on_wire) {
    const parcel::PathCell c{std::filesystem::path{"a/b/c"}};
    auto j = c.to_json();
    EXPECT_EQ(j.at(parcel::ICell::KEY_VALUE).get<std::string>(), "a/b/c");
}

TEST(Filesystem, default_cell_for_path_resolves) {
    using Cell = parcel::default_cell_for_t<std::filesystem::path>;
    static_assert(std::is_same_v<Cell, parcel::PathCell>);
    SUCCEED();
}

// ---------------------------------------------------------------------------
// A6: walk over a cell tree

TEST(Walk, vector_walks_root_only_for_primitive) {
    const auto entries = parcel::walk_to_vector(parcel::I32Cell::of(42));
    ASSERT_EQ(entries.size(), 1U);
    EXPECT_EQ(entries[0].path, "");
    EXPECT_EQ(entries[0].cell->kind(), "i32");
}

TEST(Walk, vector_descends_into_list) {
    const auto root = parcel::ListCell::of(std::vector<parcel::cell_t>{
        parcel::cell(1),
        parcel::cell(std::string("hi")),
    });
    const auto entries = parcel::walk_to_vector(root);
    ASSERT_EQ(entries.size(), 3U);
    EXPECT_EQ(entries[0].path, "");
    EXPECT_EQ(entries[1].path, "/0");
    EXPECT_EQ(entries[2].path, "/1");
}

TEST(Walk, vector_descends_into_map) {
    const parcel::MapCell root{
        {"a", parcel::cell(1)},
        {"b", parcel::cell(2)},
    };
    const auto entries = parcel::walk_to_vector(root.clone());
    ASSERT_EQ(entries.size(), 3U);
    EXPECT_EQ(entries[0].path, "");
    // Map keys come out in sorted order (std::map).
    EXPECT_EQ(entries[1].path, "/a");
    EXPECT_EQ(entries[2].path, "/b");
}

TEST(Walk, vector_handles_nested_structure) {
    auto inner = parcel::ListCell::of(std::vector<parcel::cell_t>{
        parcel::cell(1),
        parcel::cell(2),
    });
    parcel::MapCell root{{"xs", inner}};
    auto entries = parcel::walk_to_vector(root.clone());
    ASSERT_EQ(entries.size(), 4U);
    EXPECT_EQ(entries[0].path, "");
    EXPECT_EQ(entries[1].path, "/xs");
    EXPECT_EQ(entries[2].path, "/xs/0");
    EXPECT_EQ(entries[3].path, "/xs/1");
}

#if PARCEL_HAS_GENERATOR
TEST(Walk, generator_yields_entries_lazily) {
    auto root = parcel::ListCell::of(std::vector<parcel::cell_t>{
        parcel::cell(1),
        parcel::cell(2),
    });
    std::size_t count = 0;
    for (auto&& entry : parcel::walk(root)) {
        (void)entry;
        ++count;
    }
    EXPECT_EQ(count, 3U);
}

TEST(Walk, generator_on_null_root_yields_nothing) {
    parcel::cell_t c;
    std::size_t count = 0;
    for (auto&& entry : parcel::walk(c)) {
        (void)entry;
        ++count;
    }
    EXPECT_EQ(count, 0U);
}
#endif

TEST(Walk, vector_on_null_root_is_empty) {
    const parcel::cell_t c;
    const auto entries = parcel::walk_to_vector(c);
    EXPECT_TRUE(entries.empty());
}

TEST(Walk, vector_treats_struct_as_leaf) {
    const auto root = WalkPayloadCell::of(WalkPayload{.id = 1, .label = "x"});
    const auto entries = parcel::walk_to_vector(root);
    ASSERT_EQ(entries.size(), 1U);
    EXPECT_EQ(entries[0].path, "");
    EXPECT_EQ(entries[0].cell->kind(), "s:walk_payload");
}

TEST(Walk, vector_escapes_map_keys_per_rfc6901) {
    const parcel::MapCell root{
        {"a/b", parcel::cell(1)},
        {"x~y", parcel::cell(2)},
    };
    const auto entries = parcel::walk_to_vector(root.clone());
    ASSERT_EQ(entries.size(), 3U);
    // std::map sorts keys: "a/b" < "x~y".
    EXPECT_EQ(entries[1].path, "/a~1b");
    EXPECT_EQ(entries[2].path, "/x~0y");
}

// ---------------------------------------------------------------------------
// A7 / A8: format-spec extensions

TEST(FormatSpec, default_spec_uses_to_string) {
    parcel::I32Cell c{42};
    EXPECT_EQ(std::format("{}", c), "42");
}

TEST(FormatSpec, json_compact_spec) {
    parcel::I32Cell c{42};
    const auto out = std::format("{:j}", c);
    EXPECT_EQ(out, R"({"k":"i32","v":42})");
}

TEST(FormatSpec, json_pretty_spec_uses_indent_2) {
    parcel::I32Cell c{42};
    auto out = std::format("{:j2}", c);
    // Pretty JSON includes newlines; just confirm it differs from compact.
    EXPECT_NE(out, std::format("{:j}", c));
    EXPECT_NE(out.find('\n'), std::string::npos);
}

TEST(FormatSpec, kind_only_spec_emits_kind_id) {
    parcel::I32Cell c{42};
    EXPECT_EQ(std::format("{:k}", c), "i32");
}

TEST(FormatSpec, hash_spec_uses_formatted_string) {
    parcel::I32Cell c{42};
    EXPECT_EQ(std::format("{:#}", c), c.to_formatted_string());
}

TEST(FormatSpec, invalid_spec_throws_format_error) {
    parcel::I32Cell c{42};
    // Use std::vformat with a runtime spec to bypass compile-time spec parsing.
    EXPECT_THROW(
        { (void)std::vformat(std::string_view{"{:x}"}, std::make_format_args(c)); },
        std::format_error);
}

TEST(FormatSpec, cell_t_handle_compact_spec) {
    parcel::cell_t c = parcel::I32Cell::of(42);
    EXPECT_EQ(std::format("{}", c), "42");
}

TEST(FormatSpec, cell_t_handle_formatted_spec) {
    parcel::cell_t c = parcel::I32Cell::of(42);
    EXPECT_EQ(std::format("{:#}", c), c->to_formatted_string());
}

TEST(FormatSpec, cell_t_handle_json_specs) {
    parcel::cell_t c = parcel::I32Cell::of(42);
    EXPECT_EQ(std::format("{:j}", c), R"({"k":"i32","v":42})");
    auto pretty = std::format("{:j2}", c);
    EXPECT_NE(pretty.find('\n'), std::string::npos);
}

TEST(FormatSpec, cell_t_handle_kind_spec) {
    parcel::cell_t c = parcel::I32Cell::of(42);
    EXPECT_EQ(std::format("{:k}", c), "i32");
}

TEST(FormatSpec, cell_t_null_handle_renders_placeholder) {
    parcel::cell_t c;
    EXPECT_EQ(std::format("{}", c), "<null>");
}

#if defined(__cpp_lib_print) && __cpp_lib_print >= 202207L
TEST(FormatSpec, parcel_print_and_println_smoke) {
    testing::internal::CaptureStdout();
    parcel::print("hi {}", 42);
    parcel::println(" then {}", std::string("done"));
    const auto out = testing::internal::GetCapturedStdout();
    EXPECT_TRUE(out.starts_with("hi 42"));
    EXPECT_TRUE(out.ends_with("done\n"));
}
#endif

// ---------------------------------------------------------------------------
// A9: stream / byte JSON I/O

TEST(JsonIO, cell_to_stream_and_from_stream_round_trip) {
    const auto src = parcel::I32Cell::of(123);
    std::stringstream buf;
    parcel::cell_to_stream(buf, src);
    const auto rebuilt = parcel::cell_from_stream(buf, registry());
    EXPECT_EQ(*src, *rebuilt);
}

TEST(JsonIO, cell_from_bytes_round_trip) {
    const auto src = parcel::StringCell::of(std::string("hello"));
    const auto json = src->to_json().dump();
    std::vector<std::byte> bytes(json.size());
    std::memcpy(bytes.data(), json.data(), json.size());
    const auto rebuilt = parcel::cell_from_bytes(std::span<const std::byte>(bytes), registry());
    EXPECT_EQ(*src, *rebuilt);
}

#if PARCEL_HAS_EXPECTED
TEST(JsonIO, try_cell_from_stream_returns_error_on_garbage) {
    std::stringstream buf("{not json");
    auto r = parcel::try_cell_from_stream(buf, registry());
    ASSERT_FALSE(r.has_value());
    EXPECT_EQ(r.error().code, parcel::ParcelError::Code::InvalidJson);
}

TEST(JsonIO, try_cell_from_bytes_returns_error_on_garbage) {
    std::string garbage = "not-json";
    std::vector<std::byte> bytes(garbage.size());
    std::memcpy(bytes.data(), garbage.data(), garbage.size());
    auto r = parcel::try_cell_from_bytes(std::span<const std::byte>(bytes), registry());
    ASSERT_FALSE(r.has_value());
    EXPECT_EQ(r.error().code, parcel::ParcelError::Code::InvalidJson);
}
#endif

TEST(JsonIO, cell_to_stream_indented_emits_multiline) {
    const auto src = parcel::I32Cell::of(1);
    std::stringstream buf;
    parcel::cell_to_stream(buf, src, 2);
    EXPECT_NE(buf.str().find('\n'), std::string::npos);
}

#if PARCEL_HAS_EXPECTED
TEST(JsonIO, try_cell_from_stream_success_returns_cell) {
    auto src = parcel::I32Cell::of(11);
    std::stringstream buf;
    parcel::cell_to_stream(buf, src);
    auto r = parcel::try_cell_from_stream(buf, registry());
    ASSERT_TRUE(r.has_value());
    EXPECT_EQ(*src, *r.value());
}

TEST(JsonIO, try_cell_from_bytes_success_returns_cell) {
    auto src = parcel::StringCell::of(std::string("by"));
    auto json = src->to_json().dump();
    std::vector<std::byte> bytes(json.size());
    std::memcpy(bytes.data(), json.data(), json.size());
    auto r = parcel::try_cell_from_bytes(std::span<const std::byte>(bytes), registry());
    ASSERT_TRUE(r.has_value());
    EXPECT_EQ(*src, *r.value());
}
#endif

TEST(JsonIO, cell_to_stream_writes_null_for_null_cell) {
    const parcel::cell_t c;
    std::stringstream buf;
    parcel::cell_to_stream(buf, c);
    EXPECT_EQ(buf.str(), "null");
}

TEST(JsonIO, cell_to_stream_indented_round_trips) {
    const auto src = parcel::ListCell::of(std::vector<parcel::cell_t>{
        parcel::cell(1),
        parcel::cell(2),
    });
    std::stringstream buf;
    parcel::cell_to_stream(buf, src, 2);
    EXPECT_NE(buf.str().find('\n'), std::string::npos);
    const auto rebuilt = parcel::cell_from_stream(buf, registry());
    EXPECT_EQ(*src, *rebuilt);
}

// ---------------------------------------------------------------------------
// A10: TypedHashMapCell / HashMapCell

TEST(HashMap, typed_round_trips_with_canonical_wire_order) {
    parcel::TypedHashMapCell<parcel::I32Cell> m;
    m["b"] = 2;
    m["a"] = 1;
    auto j = m.to_json();
    // Wire is canonical (sorted), so JSON strings compare equal regardless of
    // hash insertion order.
    auto v = j.at(parcel::ICell::KEY_VALUE);
    auto keys = v.items();
    auto it = keys.begin();
    EXPECT_EQ(it.key(), "a");
    ++it;
    EXPECT_EQ(it.key(), "b");

    auto rebuilt = parcel::TypedHashMapCell<parcel::I32Cell>::from_json(j, registry());
    auto* typed = dynamic_cast<parcel::TypedHashMapCell<parcel::I32Cell>*>(rebuilt.get());
    ASSERT_NE(typed, nullptr);
    EXPECT_EQ(typed->at("a"), 1);
    EXPECT_EQ(typed->at("b"), 2);
}

TEST(HashMap, heterogeneous_round_trips) {
    parcel::ParcelRegistry reg;  // builtins enabled — provides 'i32', 'string', 'hm'.
    reg.register_kind(parcel::HashMapCell::descriptor());

    parcel::HashMapCell m{
        {"x", parcel::cell(1)},
        {"y", parcel::cell(std::string("hi"))},
    };
    auto j = m.to_json();
    auto rebuilt = parcel::HashMapCell::from_json(j, reg);
    auto* typed = dynamic_cast<parcel::HashMapCell*>(rebuilt.get());
    ASSERT_NE(typed, nullptr);
    EXPECT_EQ(typed->size(), 2U);
}

TEST(HashMap, equal_maps_compare_equivalent_regardless_of_iteration_order) {
    parcel::TypedHashMapCell<parcel::I32Cell> a;
    a["a"] = 1;
    a["b"] = 2;

    parcel::TypedHashMapCell<parcel::I32Cell> b;
    b["b"] = 2;
    b["a"] = 1;

    EXPECT_EQ(a, b);
}

TEST(HashMap, equal_typed_hash_maps_hash_equal_regardless_of_insertion_order) {
    // Pin the equality-consistent-hashing contract: two TypedHashMapCells
    // with identical logical contents but different insertion orders must
    // hash to the same value (same as ordered TypedMapCell), so they round-
    // trip cleanly into std::unordered_set<cell_t>.
    parcel::TypedHashMapCell<parcel::I32Cell> a;
    a["a"] = 1;
    a["b"] = 2;
    a["c"] = 3;

    parcel::TypedHashMapCell<parcel::I32Cell> b;
    b["c"] = 3;
    b["a"] = 1;
    b["b"] = 2;

    ASSERT_EQ(a, b);
    EXPECT_EQ(a.hash_value(), b.hash_value());
}

TEST(HashMap, equal_hetero_hash_maps_hash_equal_regardless_of_insertion_order) {
    parcel::HashMapCell a{
        {"k1", parcel::cell(1)},
        {"k2", parcel::cell(std::string("hi"))},
    };
    parcel::HashMapCell b{
        {"k2", parcel::cell(std::string("hi"))},
        {"k1", parcel::cell(1)},
    };

    ASSERT_EQ(a, b);
    EXPECT_EQ(a.hash_value(), b.hash_value());
}

TEST(HashMap, typed_to_string_is_sorted_and_deterministic) {
    parcel::TypedHashMapCell<parcel::I32Cell> a;
    a["b"] = 2;
    a["a"] = 1;
    a["c"] = 3;

    parcel::TypedHashMapCell<parcel::I32Cell> b;
    b["c"] = 3;
    b["a"] = 1;
    b["b"] = 2;

    const auto sa = a.to_string();
    EXPECT_EQ(sa, b.to_string());
    EXPECT_LT(sa.find("a:"), sa.find("b:"));
    EXPECT_LT(sa.find("b:"), sa.find("c:"));
}

TEST(HashMap, typed_from_json_throws_on_kind_mismatch) {
    parcel::json_t j{
        {parcel::ICell::KEY_KIND, "wrong"},
        {parcel::ICell::KEY_VALUE, parcel::json_t::object()},
    };
    EXPECT_THROW(
        { (void)parcel::TypedHashMapCell<parcel::I32Cell>::from_json(j, registry()); },
        std::runtime_error);
}

TEST(HashMap, typed_from_json_throws_on_non_object_value) {
    parcel::json_t j{
        {parcel::ICell::KEY_KIND, parcel::TypedHashMapCell<parcel::I32Cell>::kind_id},
        {parcel::ICell::KEY_VALUE, 42},
    };
    EXPECT_THROW(
        { (void)parcel::TypedHashMapCell<parcel::I32Cell>::from_json(j, registry()); },
        std::runtime_error);
}

TEST(HashMap, hetero_from_json_throws_on_kind_mismatch) {
    parcel::ParcelRegistry reg;
    reg.register_kind(parcel::HashMapCell::descriptor());
    parcel::json_t j{
        {parcel::ICell::KEY_KIND, "wrong"},
        {parcel::ICell::KEY_VALUE, parcel::json_t::object()},
    };
    EXPECT_THROW({ (void)parcel::HashMapCell::from_json(j, reg); }, std::runtime_error);
}

TEST(HashMap, hetero_equality_is_order_independent) {
    parcel::HashMapCell a{
        {"x", parcel::cell(1)},
        {"y", parcel::cell(std::string("hi"))},
    };
    parcel::HashMapCell b{
        {"y", parcel::cell(std::string("hi"))},
        {"x", parcel::cell(1)},
    };
    EXPECT_EQ(a, b);
}

TEST(HashMap, hetero_clone_preserves_meta_and_deep_copies) {
    parcel::HashMapCell src{
        {"x", parcel::cell(1)},
    };
    auto with_name = src.with_name("annotated");
    auto cloned = with_name->clone();
    auto* typed = dynamic_cast<parcel::HashMapCell*>(cloned.get());
    ASSERT_NE(typed, nullptr);
    ASSERT_TRUE(typed->meta().has_value());
    EXPECT_EQ(typed->meta()->name, "annotated");

    // Mutating the clone leaves the original intact.
    (*typed)["x"] = parcel::cell(999);
    auto* original = dynamic_cast<parcel::HashMapCell*>(with_name.get());
    ASSERT_NE(original, nullptr);
    auto v = parcel::cell_cast_value<parcel::I32Cell>(original->at("x"));
    EXPECT_EQ(v, 1);
}

TEST(HashMap, hetero_ordering_compare_strict_less) {
    parcel::HashMapCell a{
        {"a", parcel::cell(1)},
    };
    parcel::HashMapCell b{
        {"b", parcel::cell(1)},
    };
    EXPECT_EQ(a.compare(b), std::partial_ordering::less);
}

TEST(HashMap, hetero_descriptor_reports_map_category) {
    EXPECT_EQ(parcel::HashMapCell::descriptor()->category(), parcel::descriptor::CellCategory::Map);
}

TEST(HashMap, hetero_from_json_round_trips_null_value) {
    parcel::ParcelRegistry reg;
    reg.register_kind(parcel::HashMapCell::descriptor());
    parcel::HashMapCell src{
        {"a", parcel::cell(1)},
        {"b", nullptr},
    };
    const auto j = src.to_json();
    const auto restored = parcel::HashMapCell::from_json(j, reg);
    auto* typed = dynamic_cast<parcel::HashMapCell*>(restored.get());
    ASSERT_NE(typed, nullptr);
    EXPECT_EQ(typed->size(), 2U);
    EXPECT_EQ(typed->at("b"), nullptr);
}

// ---------------------------------------------------------------------------
// §H: integration of default_cell_for / chrono / filesystem with FieldsBuilder.

namespace {

struct PathPayload {
    std::filesystem::path location;
};

class PathPayloadCell : public parcel::StructCell<PathPayloadCell, PathPayload, "path_payload"> {
public:
    using StructCell::StructCell;
    [[maybe_unused]] static parcel::DisplayInfo meta_info() {
        return {.name = "PathPayload"};
    }
    [[maybe_unused]] static auto field_descriptors() {
        return parcel::FieldsBuilder<PathPayload>{}
            .field<&PathPayload::location>("location")
            .build();
    }
};

struct TimestampPayload {
    std::chrono::sys_time<std::chrono::milliseconds> at;
};

class TimestampPayloadCell
    : public parcel::StructCell<TimestampPayloadCell, TimestampPayload, "ts_payload"> {
public:
    using StructCell::StructCell;
    [[maybe_unused]] static parcel::DisplayInfo meta_info() {
        return {.name = "TimestampPayload"};
    }
    [[maybe_unused]] static auto field_descriptors() {
        return parcel::FieldsBuilder<TimestampPayload>{}.field<&TimestampPayload::at>("at").build();
    }
};

}  // namespace

TEST(StructFieldIntegration, filesystem_path_field_round_trips) {
    parcel::ParcelRegistry reg;
    reg.register_kind(PathPayloadCell::descriptor());

    const auto src = PathPayloadCell::of(PathPayload{.location = std::filesystem::path{"a/b/c"}});
    const auto j = src->to_json();
    const auto rebuilt = reg.cell_from_json(j);
    auto* typed = dynamic_cast<PathPayloadCell*>(rebuilt.get());
    ASSERT_NE(typed, nullptr);
    EXPECT_EQ(typed->value.location, std::filesystem::path{"a/b/c"});
}

TEST(StructFieldIntegration, chrono_sys_time_ms_field_round_trips) {
    parcel::ParcelRegistry reg;
    reg.register_kind(TimestampPayloadCell::descriptor());

    using namespace std::chrono;
    constexpr sys_time<milliseconds> tp{milliseconds{1'700'000'000'000}};
    const auto src = TimestampPayloadCell::of(TimestampPayload{.at = tp});
    const auto j = src->to_json();
    const auto rebuilt = reg.cell_from_json(j);
    auto* typed = dynamic_cast<TimestampPayloadCell*>(rebuilt.get());
    ASSERT_NE(typed, nullptr);
    EXPECT_EQ(typed->value.at, tp);
}

// ---------------------------------------------------------------------------
// YmdCell parse / json error paths and BCE.

TEST(Chrono, ymd_parse_error_throws) {
    parcel::json_t j{
        {parcel::ICell::KEY_KIND, "time:ymd"},
        {parcel::ICell::KEY_VALUE, "not-a-date"},
    };
    EXPECT_THROW({ (void)parcel::YmdCell::from_json(j, registry()); }, std::runtime_error);
}

TEST(Chrono, ymd_bce_year_round_trips) {
    using namespace std::chrono;
    year_month_day d{year{-1}, December, 31d};
    parcel::YmdCell c{d};
    auto j = c.to_json();
    EXPECT_EQ(j.at(parcel::ICell::KEY_VALUE).get<std::string>(), "-0001-12-31");
    auto rebuilt = parcel::YmdCell::from_json(j, registry());
    auto* typed = dynamic_cast<parcel::YmdCell*>(rebuilt.get());
    ASSERT_NE(typed, nullptr);
    EXPECT_EQ(typed->value, d);
}

TEST(Chrono, ymd_from_json_rejects_non_string_v) {
    parcel::json_t j{
        {parcel::ICell::KEY_KIND, "time:ymd"},
        {parcel::ICell::KEY_VALUE, 12345},
    };
    EXPECT_THROW({ (void)parcel::YmdCell::from_json(j, registry()); }, parcel::json_t::exception);
}

// ---------------------------------------------------------------------------
// Comparison and hashing on chrono and filesystem cells.

TEST(Chrono, unix_millis_supports_equality_and_hash) {
    using namespace std::chrono;
    sys_time<milliseconds> tp{milliseconds{500}};
    const auto a = parcel::UnixMillisCell::of(tp);
    const auto b = parcel::UnixMillisCell::of(tp);
    EXPECT_EQ(*a, *b);
    EXPECT_EQ(parcel::hash_cell(a), parcel::hash_cell(b));
    std::unordered_set<parcel::cell_t> set;
    set.insert(a);
    EXPECT_EQ(set.size(), 1U);
}

TEST(Filesystem, path_supports_equality_and_hash) {
    const auto a = parcel::PathCell::of(std::filesystem::path{"/x/y"});
    const auto b = parcel::PathCell::of(std::filesystem::path{"/x/y"});
    EXPECT_EQ(*a, *b);
    EXPECT_EQ(parcel::hash_cell(a), parcel::hash_cell(b));
    std::unordered_set<parcel::cell_t> set;
    set.insert(a);
    EXPECT_EQ(set.size(), 1U);
}

// ---------------------------------------------------------------------------
// C: chrono cell to_string / descriptor / kind-mismatch coverage.

TEST(SystemTimePointCell, to_string_emits_epoch_seconds) {
    using namespace std::chrono;
    const parcel::SystemTimePointCell c{sys_seconds{seconds{1234}}};
    EXPECT_EQ(c.to_string(), "1234");
}

TEST(UnixMillisCell, to_string_emits_epoch_ms_with_suffix) {
    using namespace std::chrono;
    const parcel::UnixMillisCell c{sys_time<milliseconds>{milliseconds{500}}};
    EXPECT_EQ(c.to_string(), "500ms");
}

TEST(DurationMsCell, to_string_emits_count_with_suffix) {
    const parcel::DurationMsCell c{std::chrono::milliseconds{750}};
    EXPECT_EQ(c.to_string(), "750ms");
}

TEST(YmdCell, to_string_matches_iso_format) {
    using namespace std::chrono;
    const parcel::YmdCell c{year_month_day{year{2026}, April, 27d}};
    EXPECT_EQ(c.to_string(), "2026-04-27");
}

TEST(SystemTimePointCell, descriptor_returns_non_null_with_meta) {
    const auto d = parcel::SystemTimePointCell::descriptor();
    ASSERT_NE(d, nullptr);
    EXPECT_EQ(d->kind(), parcel::SystemTimePointCell::kind_id);
    EXPECT_EQ(d->meta().name, "SystemTimePoint");
}

TEST(UnixMillisCell, descriptor_returns_non_null_with_meta) {
    const auto d = parcel::UnixMillisCell::descriptor();
    ASSERT_NE(d, nullptr);
    EXPECT_EQ(d->kind(), parcel::UnixMillisCell::kind_id);
    EXPECT_EQ(d->meta().name, "UnixMillis");
}

TEST(DurationMsCell, descriptor_returns_non_null_with_meta) {
    const auto d = parcel::DurationMsCell::descriptor();
    ASSERT_NE(d, nullptr);
    EXPECT_EQ(d->kind(), parcel::DurationMsCell::kind_id);
    EXPECT_EQ(d->meta().name, "DurationMs");
}

TEST(YmdCell, descriptor_returns_non_null_with_meta) {
    const auto d = parcel::YmdCell::descriptor();
    ASSERT_NE(d, nullptr);
    EXPECT_EQ(d->kind(), parcel::YmdCell::kind_id);
    EXPECT_EQ(d->meta().name, "YearMonthDay");
}

TEST(SystemTimePointCell, from_json_throws_on_kind_mismatch) {
    parcel::json_t j{
        {parcel::ICell::KEY_KIND, "time:unix_ms"},
        {parcel::ICell::KEY_VALUE, 1},
    };
    EXPECT_THROW(
        { (void)parcel::SystemTimePointCell::from_json(j, registry()); }, std::runtime_error);
}

TEST(UnixMillisCell, from_json_throws_on_kind_mismatch) {
    parcel::json_t j{
        {parcel::ICell::KEY_KIND, "time:sys_seconds"},
        {parcel::ICell::KEY_VALUE, 1},
    };
    EXPECT_THROW({ (void)parcel::UnixMillisCell::from_json(j, registry()); }, std::runtime_error);
}

TEST(DurationMsCell, from_json_throws_on_kind_mismatch) {
    parcel::json_t j{
        {parcel::ICell::KEY_KIND, "time:unix_ms"},
        {parcel::ICell::KEY_VALUE, 1},
    };
    EXPECT_THROW({ (void)parcel::DurationMsCell::from_json(j, registry()); }, std::runtime_error);
}

TEST(YmdCell, format_iso_pads_single_digit_month_and_day) {
    using namespace std::chrono;
    parcel::YmdCell c{year_month_day{year{2024}, January, 1d}};
    EXPECT_EQ(c.to_string(), "2024-01-01");
    auto j = c.to_json();
    EXPECT_EQ(j.at(parcel::ICell::KEY_VALUE).get<std::string>(), "2024-01-01");
    auto rebuilt = parcel::YmdCell::from_json(j, registry());
    auto* typed = dynamic_cast<parcel::YmdCell*>(rebuilt.get());
    ASSERT_NE(typed, nullptr);
    EXPECT_EQ(typed->value, c.value);
}

TEST(YmdCell, format_iso_pads_bce_year_with_leading_minus) {
    using namespace std::chrono;
    parcel::YmdCell c{year_month_day{year{-44}, January, 1d}};
    EXPECT_EQ(c.to_string(), "-0044-01-01");
    auto j = c.to_json();
    EXPECT_EQ(j.at(parcel::ICell::KEY_VALUE).get<std::string>(), "-0044-01-01");
    auto rebuilt = parcel::YmdCell::from_json(j, registry());
    auto* typed = dynamic_cast<parcel::YmdCell*>(rebuilt.get());
    ASSERT_NE(typed, nullptr);
    EXPECT_EQ(typed->value, c.value);
}

// ---------------------------------------------------------------------------
// D: PathCell to_string / clone / compare / kind-mismatch / descriptor.

TEST(PathCell, to_string_returns_generic_form) {
    const parcel::PathCell c{std::filesystem::path{"/a/b/c"}};
    EXPECT_EQ(c.to_string(), "/a/b/c");
}

TEST(PathCell, clone_round_trip_preserves_value) {
    const auto a = parcel::PathCell::of(std::filesystem::path{"/x/y"});
    const auto b = a->clone();
    auto* typed = dynamic_cast<parcel::PathCell*>(b.get());
    ASSERT_NE(typed, nullptr);
    EXPECT_EQ(typed->value, std::filesystem::path{"/x/y"});
}

TEST(PathCell, compare_orders_paths_lexicographically) {
    parcel::PathCell a{std::filesystem::path{"/a"}};
    parcel::PathCell b{std::filesystem::path{"/b"}};
    parcel::PathCell a2{std::filesystem::path{"/a"}};
    EXPECT_TRUE(a.compare(b) < 0);
    EXPECT_TRUE(b.compare(a) > 0);
    EXPECT_TRUE(a.compare(a2) == 0);
}

TEST(PathCell, from_json_throws_on_kind_mismatch) {
    parcel::json_t j{
        {parcel::ICell::KEY_KIND, "string"},
        {parcel::ICell::KEY_VALUE, "/a/b"},
    };
    EXPECT_THROW({ (void)parcel::PathCell::from_json(j, registry()); }, std::runtime_error);
}

TEST(PathCell, descriptor_returns_non_null_with_meta) {
    const auto d = parcel::PathCell::descriptor();
    ASSERT_NE(d, nullptr);
    EXPECT_EQ(d->kind(), parcel::PathCell::kind_id);
    EXPECT_EQ(d->meta().name, "Path");
}

// ---------------------------------------------------------------------------
// B: TypedHashMapCell + HashMapCell storage helpers, comparison branches,
// and descriptor coverage.

TEST(TypedHashMapCell, empty_returns_true_when_no_elements) {
    const parcel::TypedHashMapCell<parcel::I32Cell> m;
    EXPECT_TRUE(m.empty());
    EXPECT_EQ(m.size(), 0U);
}

TEST(TypedHashMapCell, mutating_subscript_inserts_default) {
    parcel::TypedHashMapCell<parcel::I32Cell> m;
    const auto& slot = m["k"];
    EXPECT_EQ(slot, 0);
    EXPECT_TRUE(m.contains("k"));
    EXPECT_EQ(m.size(), 1U);
}

TEST(TypedHashMapCell, contains_reports_membership) {
    parcel::TypedHashMapCell<parcel::I32Cell> m;
    m["a"] = 1;
    EXPECT_TRUE(m.contains("a"));
    EXPECT_FALSE(m.contains("b"));
}

TEST(TypedHashMapCell, iteration_visits_all_pairs_non_const) {
    parcel::TypedHashMapCell<parcel::I32Cell> m;
    m["a"] = 1;
    m["b"] = 2;
    int sum = 0;
    for (auto& [k, v] : m) {
        (void)k;
        sum += v;
    }
    EXPECT_EQ(sum, 3);
}

TEST(TypedHashMapCell, iteration_visits_all_pairs_const) {
    parcel::TypedHashMapCell<parcel::I32Cell> m;
    m["a"] = 1;
    m["b"] = 2;
    const auto& cm = m;
    int sum = 0;
    for (auto const& [k, v] : cm) {
        (void)k;
        sum += v;
    }
    EXPECT_EQ(sum, 3);
}

TEST(TypedHashMapCell, compare_size_asymmetry_when_prefix_equal) {
    parcel::TypedHashMapCell<parcel::I32Cell> a;
    a["a"] = 1;
    a["b"] = 2;

    parcel::TypedHashMapCell<parcel::I32Cell> b;
    b["a"] = 1;

    EXPECT_TRUE(a.compare(b) > 0);
    EXPECT_TRUE(b.compare(a) < 0);
}

TEST(TypedHashMapCell, descriptor_returns_descriptor_with_typed_map_category) {
    const auto d = parcel::TypedHashMapCell<parcel::I32Cell>::descriptor();
    ASSERT_NE(d, nullptr);
    EXPECT_EQ(d->category(), parcel::descriptor::CellCategory::TypedMap);
    EXPECT_EQ(d->kind(), parcel::TypedHashMapCell<parcel::I32Cell>::kind_id);
}

TEST(HashMapCell, empty_and_size_zero) {
    const parcel::HashMapCell m;
    EXPECT_TRUE(m.empty());
    EXPECT_EQ(m.size(), 0U);
}

TEST(HashMapCell, mutating_subscript_inserts_null) {
    parcel::HashMapCell m;
    const auto& slot = m["k"];
    EXPECT_EQ(slot, nullptr);
    EXPECT_TRUE(m.contains("k"));
    EXPECT_EQ(m.size(), 1U);
}

TEST(HashMapCell, contains_reports_membership) {
    const parcel::HashMapCell m{{"a", parcel::cell(1)}};
    EXPECT_TRUE(m.contains("a"));
    EXPECT_FALSE(m.contains("b"));
}

TEST(HashMapCell, to_string_renders_null_value_as_placeholder) {
    parcel::HashMapCell m;
    m["ghost"] = nullptr;
    EXPECT_NE(m.to_string().find("<null>"), std::string::npos);
}

TEST(HashMapCell, clone_preserves_null_value_at_key) {
    parcel::HashMapCell m;
    m["ghost"] = nullptr;
    m["alive"] = parcel::cell(1);
    const auto cloned = m.clone();
    auto* typed = dynamic_cast<parcel::HashMapCell*>(cloned.get());
    ASSERT_NE(typed, nullptr);
    ASSERT_TRUE(typed->contains("ghost"));
    EXPECT_EQ(typed->at("ghost"), nullptr);
    ASSERT_TRUE(typed->contains("alive"));
    EXPECT_NE(typed->at("alive"), nullptr);
}

TEST(HashMapCell, compare_orders_left_null_below_right_value) {
    parcel::HashMapCell a;
    a["k"] = nullptr;
    parcel::HashMapCell b;
    b["k"] = parcel::cell(1);
    EXPECT_TRUE(a.compare(b) < 0);
}

TEST(HashMapCell, compare_orders_left_value_above_right_null) {
    parcel::HashMapCell a;
    a["k"] = parcel::cell(1);
    parcel::HashMapCell b;
    b["k"] = nullptr;
    EXPECT_TRUE(a.compare(b) > 0);
}

TEST(HashMapCell, descriptor_returns_non_null_with_meta) {
    const auto d = parcel::HashMapCell::descriptor();
    ASSERT_NE(d, nullptr);
    EXPECT_EQ(d->kind(), parcel::HashMapCell::kind_id);
    EXPECT_EQ(d->meta().name, "HashMap");
}
