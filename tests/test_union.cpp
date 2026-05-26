#include <parcel/parcel.h>

#include <gtest/gtest.h>

#include <memory>
#include <string>
#include <variant>
#include <vector>

namespace {

parcel::ParcelRegistry const& registry() {
    static const parcel::ParcelRegistry r;
    return r;
}

parcel::ParcelRegistry const& populated_registry() {
    static const parcel::ParcelRegistry r = [] {
        parcel::ParcelRegistry reg;
        reg.register_kind(parcel::UnionCell<parcel::I32Cell, parcel::StringCell>::descriptor());
        return reg;
    }();
    return r;
}

}  // namespace

using IntOrString = parcel::UnionCell<parcel::I32Cell, parcel::StringCell>;
using IntOrList = parcel::UnionCell<parcel::I32Cell, parcel::TypedListCell<parcel::I32Cell>>;
using ListOrString = parcel::UnionCell<parcel::TypedListCell<parcel::I32Cell>, parcel::StringCell>;

// ---------------------------------------------------------------------------
// Concept gate

static_assert(parcel::CellLike<IntOrString>);
static_assert(parcel::CellLike<IntOrList>);
static_assert(parcel::CellLike<ListOrString>);

// ---------------------------------------------------------------------------
// Construction

TEST(UnionCell, constructs_from_first_alternative_storage) {
    IntOrString u{std::int32_t{42}};
    EXPECT_EQ(u.index(), 0u);
    EXPECT_EQ(u.active_kind(), "i32");
    EXPECT_EQ(u.get<0>(), 42);
}

TEST(UnionCell, constructs_from_second_alternative_storage) {
    IntOrString u{std::string("hi")};
    EXPECT_EQ(u.index(), 1u);
    EXPECT_EQ(u.active_kind(), "string");
    EXPECT_EQ(u.get<1>(), "hi");
}

TEST(UnionCell, assignment_switches_active_alternative) {
    IntOrString u{std::int32_t{1}};
    EXPECT_EQ(u.index(), 0u);
    u = std::string("hello");
    EXPECT_EQ(u.index(), 1u);
    EXPECT_EQ(u.active_kind(), "string");
    EXPECT_EQ(u.get<1>(), "hello");
}

TEST(UnionCell, get_by_storage_type) {
    IntOrString u{std::string("ok")};
    EXPECT_EQ(u.get<std::string>(), "ok");
}

TEST(UnionCell, kind_encodes_alternatives_regardless_of_active) {
    const IntOrString a{std::int32_t{1}};
    const IntOrString b{std::string("x")};
    EXPECT_EQ(a.kind(), "u:i32,string");
    EXPECT_EQ(b.kind(), "u:i32,string");
}

TEST(UnionCell, kind_encodes_alternative_kinds_in_template_order) {
    static_assert(parcel::UnionCell<parcel::U32Cell, parcel::I32Cell>::kind_id == "u:u32,i32");
    EXPECT_EQ((parcel::UnionCell<parcel::U32Cell, parcel::I32Cell>::kind_id), "u:u32,i32");
}

TEST(UnionCell, kind_for_nested_typed_alt) {
    static_assert(
        parcel::UnionCell<parcel::I32Cell, parcel::TypedListCell<parcel::I32Cell>>::kind_id ==
        "u:i32,l:i32");
    SUCCEED();
}

TEST(UnionCell, kind_id_for_single_alternative) {
    static_assert(parcel::UnionCell<parcel::I32Cell>::kind_id == "u:i32");
    SUCCEED();
}

// ---------------------------------------------------------------------------
// to_string()

TEST(UnionCell, to_string_delegates_to_active_alternative) {
    const IntOrString a{std::int32_t{42}};
    EXPECT_EQ(a.to_string(), "42");

    const IntOrString b{std::string("hi")};
    EXPECT_EQ(b.to_string(), "hi");
}

// ---------------------------------------------------------------------------
// to_json() shape

TEST(UnionCell, to_json_emits_k_v_for_primitive_alt) {
    const IntOrString u{std::int32_t{42}};
    const auto j = u.to_json();
    ASSERT_TRUE(j.is_object());
    EXPECT_EQ(j["k"].get<std::string>(), "u:i32,string");
    ASSERT_TRUE(j["v"].is_object());
    EXPECT_EQ(j["v"]["k"].get<std::string>(), "i32");
    EXPECT_EQ(j["v"]["v"].get<int>(), 42);
}

TEST(UnionCell, to_json_emits_k_v_for_string_alt) {
    const IntOrString u{std::string("hi")};
    const auto j = u.to_json();
    EXPECT_EQ(j["k"].get<std::string>(), "u:i32,string");
    EXPECT_EQ(j["v"]["k"].get<std::string>(), "string");
    EXPECT_EQ(j["v"]["v"].get<std::string>(), "hi");
}

TEST(UnionCell, to_json_for_nested_typed_list_alt) {
    const IntOrList u{std::vector<std::int32_t>{1, 2, 3}};
    const auto j = u.to_json();
    EXPECT_EQ(j["k"].get<std::string>(), "u:i32,l:i32");
    EXPECT_EQ(j["v"]["k"].get<std::string>(), "l:i32");
    ASSERT_TRUE(j["v"]["v"].is_array());
    ASSERT_EQ(j["v"]["v"].size(), 3u);
    EXPECT_EQ(j["v"]["v"][0].get<int>(), 1);
    EXPECT_EQ(j["v"]["v"][2].get<int>(), 3);
}

// ---------------------------------------------------------------------------
// from_json round-trip

TEST(UnionCell, from_json_round_trip_first_alt) {
    const IntOrString original{std::int32_t{7}};
    const auto j = original.to_json();
    const auto restored = IntOrString::from_json(j, registry());
    auto* typed = dynamic_cast<IntOrString*>(restored.get());
    ASSERT_NE(typed, nullptr);
    EXPECT_EQ(typed->index(), 0u);
    EXPECT_EQ(typed->get<0>(), 7);
}

TEST(UnionCell, from_json_round_trip_second_alt) {
    const IntOrString original{std::string("yo")};
    const auto j = original.to_json();
    const auto restored = IntOrString::from_json(j, registry());
    auto* typed = dynamic_cast<IntOrString*>(restored.get());
    ASSERT_NE(typed, nullptr);
    EXPECT_EQ(typed->index(), 1u);
    EXPECT_EQ(typed->get<1>(), "yo");
}

TEST(UnionCell, from_json_round_trip_nested_typed_list) {
    const IntOrList original{std::vector<std::int32_t>{4, 5, 6}};
    const auto j = original.to_json();
    const auto restored = IntOrList::from_json(j, registry());
    auto* typed = dynamic_cast<IntOrList*>(restored.get());
    ASSERT_NE(typed, nullptr);
    EXPECT_EQ(typed->index(), 1u);
    auto const& vec = typed->get<1>();
    ASSERT_EQ(vec.size(), 3u);
    EXPECT_EQ(vec[0], 4);
    EXPECT_EQ(vec[2], 6);
}

// ---------------------------------------------------------------------------
// from_json error paths

TEST(UnionCell, from_json_rejects_non_object) {
    parcel::json_t arr = parcel::json_t::array({1});
    EXPECT_THROW(IntOrString::from_json(arr, registry()), std::runtime_error);
}

TEST(UnionCell, from_json_rejects_wrong_kind) {
    parcel::json_t inner = {{"k", "i32"}, {"v", 1}};
    parcel::json_t j = {{"k", "l"}, {"v", inner}};
    EXPECT_THROW(IntOrString::from_json(j, registry()), std::runtime_error);
}

TEST(UnionCell, from_json_rejects_missing_kind) {
    parcel::json_t inner = {{"k", "i32"}, {"v", 1}};
    parcel::json_t j = {{"v", inner}};
    EXPECT_THROW(IntOrString::from_json(j, registry()), std::runtime_error);
}

TEST(UnionCell, from_json_rejects_missing_v) {
    parcel::json_t j = {{"k", "u:i32,string"}};
    EXPECT_THROW(IntOrString::from_json(j, registry()), std::runtime_error);
}

TEST(UnionCell, from_json_rejects_non_object_v) {
    parcel::json_t j = {{"k", "u:i32,string"}, {"v", 1}};
    EXPECT_THROW(IntOrString::from_json(j, registry()), std::runtime_error);
}

TEST(UnionCell, from_json_rejects_inner_v_missing_kind) {
    parcel::json_t inner = {{"v", 1}};
    parcel::json_t j = {{"k", "u:i32,string"}, {"v", inner}};
    EXPECT_THROW(IntOrString::from_json(j, registry()), std::runtime_error);
}

TEST(UnionCell, from_json_rejects_inner_v_non_string_kind) {
    parcel::json_t inner = {{"k", 1}, {"v", 1}};
    parcel::json_t j = {{"k", "u:i32,string"}, {"v", inner}};
    EXPECT_THROW(IntOrString::from_json(j, registry()), std::runtime_error);
}

TEST(UnionCell, from_json_rejects_active_kind_not_in_alternatives) {
    parcel::json_t inner = {{"k", "f64"}, {"v", 1.0}};
    const parcel::json_t j = {{"k", "u:i32,string"}, {"v", inner}};
    try {
        (void)IntOrString::from_json(j, registry());
        FAIL() << "expected runtime_error";
    } catch (std::runtime_error const& e) {
        const std::string msg = e.what();
        EXPECT_NE(msg.find("is not one of the alternatives"), std::string::npos);
    }
}

TEST(UnionCell, from_json_reads_active_from_inner_v) {
    parcel::json_t inner = {{"k", "string"}, {"v", "hi"}};
    const parcel::json_t j = {{"k", "u:i32,string"}, {"v", inner}};
    const auto restored = IntOrString::from_json(j, registry());
    auto* typed = dynamic_cast<IntOrString*>(restored.get());
    ASSERT_NE(typed, nullptr);
    EXPECT_EQ(typed->index(), 1u);
    EXPECT_EQ(typed->get<1>(), "hi");
}

// ---------------------------------------------------------------------------
// clone() — deep on inner alternative

TEST(UnionCell, clone_returns_independent_copy_for_list_alt) {
    ListOrString u{std::vector<std::int32_t>{1, 2, 3}};
    const auto cloned = u.clone();
    ASSERT_NE(cloned, nullptr);
    EXPECT_EQ(cloned->kind(), "u:l:i32,string");

    auto* typed = dynamic_cast<ListOrString*>(cloned.get());
    ASSERT_NE(typed, nullptr);
    EXPECT_EQ(typed->index(), 0u);

    // Mutate original storage; clone must be unaffected.
    std::get<0>(u.value).clear();
    auto const& cloned_vec = typed->get<0>();
    ASSERT_EQ(cloned_vec.size(), 3u);
    EXPECT_EQ(cloned_vec[0], 1);
    EXPECT_EQ(cloned_vec[2], 3);
}

TEST(UnionCell, clone_for_primitive_alt) {
    const IntOrString u{std::string("immut")};
    const auto cloned = u.clone();
    auto* typed = dynamic_cast<IntOrString*>(cloned.get());
    ASSERT_NE(typed, nullptr);
    EXPECT_EQ(typed->index(), 1u);
    EXPECT_EQ(typed->get<1>(), "immut");
}

// ---------------------------------------------------------------------------
// descriptor()

TEST(UnionCell, descriptor_returns_non_null_shared_ptr) {
    const auto d = IntOrString::descriptor();
    ASSERT_NE(d, nullptr);
    EXPECT_EQ(d->kind(), "u:i32,string");
}

TEST(UnionCell, descriptor_is_cached_per_instantiation) {
    const auto d1 = IntOrString::descriptor();
    const auto d2 = IntOrString::descriptor();
    EXPECT_EQ(d1.get(), d2.get());
}

TEST(UnionCell, descriptor_to_json_lists_alternatives) {
    const auto d = IntOrString::descriptor();
    const auto j = d->to_json();
    EXPECT_EQ(j["kind"].get<std::string>(), "u:i32,string");
    ASSERT_TRUE(j.contains("alternatives"));
    ASSERT_TRUE(j["alternatives"].is_array());
    ASSERT_EQ(j["alternatives"].size(), 2u);
    EXPECT_EQ(j["alternatives"][0].get<std::string>(), "i32");
    EXPECT_EQ(j["alternatives"][1].get<std::string>(), "string");
}

TEST(UnionCell, descriptor_static_alternatives_match_kind_ids) {
    constexpr auto alts =
        parcel::UnionCellTypeDescriptor<parcel::I32Cell, parcel::StringCell>::alternatives();
    static_assert(alts.size() == 2);
    EXPECT_EQ(alts[0], "i32");
    EXPECT_EQ(alts[1], "string");
}

TEST(UnionCell, descriptor_cell_from_json_round_trip) {
    const IntOrString original{std::int32_t{99}};
    const auto j = original.to_json();
    const auto desc = IntOrString::descriptor();
    const auto restored = desc->cell_from_json(j, registry());
    auto* typed = dynamic_cast<IntOrString*>(restored.get());
    ASSERT_NE(typed, nullptr);
    EXPECT_EQ(typed->index(), 0u);
    EXPECT_EQ(typed->get<0>(), 99);
}

// ---------------------------------------------------------------------------
// default_cell_for<std::variant<...>>

static_assert(std::is_same_v<parcel::default_cell_for_t<std::variant<std::int32_t, std::string>>,
                             parcel::UnionCell<parcel::PrimitiveCell<std::int32_t>,
                                               parcel::PrimitiveCell<std::string>>>);

static_assert(std::is_same_v<
              parcel::default_cell_for_t<std::variant<std::int32_t, std::vector<std::int32_t>>>,
              parcel::UnionCell<parcel::PrimitiveCell<std::int32_t>,
                                parcel::TypedListCell<parcel::PrimitiveCell<std::int32_t>>>>);

namespace {

struct Payload {
    std::variant<std::int32_t, std::string> v;
};

class PayloadCell : public parcel::StructCell<PayloadCell, Payload, "payload"> {
public:
    using StructCell::StructCell;
    [[maybe_unused]] static parcel::DisplayInfo meta_info() {
        return {.name = "Payload"};
    }
    [[maybe_unused]] static auto field_descriptors() {
        return parcel::FieldsBuilder<Payload>{}.field<&Payload::v>("v").build();
    }
};

parcel::ParcelRegistry const& payload_registry() {
    static const parcel::ParcelRegistry r = [] {
        parcel::ParcelRegistry reg;
        reg.register_kind(IntOrString::descriptor());
        reg.register_kind(PayloadCell::descriptor());
        return reg;
    }();
    return r;
}

}  // namespace

TEST(UnionCellInference, struct_field_inferred_as_union_round_trip_i32) {
    Payload p;
    p.v = std::int32_t{7};
    const PayloadCell original{p};
    const auto j = original.to_json();

    EXPECT_EQ(j["v"]["v"]["k"].get<std::string>(), "u:i32,string");
    EXPECT_EQ(j["v"]["v"]["v"]["v"].get<int>(), 7);

    const auto restored = PayloadCell::from_json(j, payload_registry());
    auto* typed = dynamic_cast<PayloadCell*>(restored.get());
    ASSERT_NE(typed, nullptr);
    EXPECT_EQ(typed->value.v.index(), 0u);
    EXPECT_EQ(std::get<0>(typed->value.v), 7);
}

TEST(UnionCellInference, struct_field_inferred_as_union_round_trip_string) {
    Payload p;
    p.v = std::string("hello");
    const PayloadCell original{p};
    const auto j = original.to_json();

    EXPECT_EQ(j["v"]["v"]["k"].get<std::string>(), "u:i32,string");
    EXPECT_EQ(j["v"]["v"]["v"]["k"].get<std::string>(), "string");
    EXPECT_EQ(j["v"]["v"]["v"]["v"].get<std::string>(), "hello");

    const auto restored = PayloadCell::from_json(j, payload_registry());
    auto* typed = dynamic_cast<PayloadCell*>(restored.get());
    ASSERT_NE(typed, nullptr);
    EXPECT_EQ(typed->value.v.index(), 1u);
    EXPECT_EQ(std::get<1>(typed->value.v), "hello");
}

TEST(UnionCellInference, struct_field_descriptor_kind_is_union) {
    const auto d = PayloadCell::descriptor();
    const auto j = d->to_json();
    ASSERT_TRUE(j.contains("fields"));
    ASSERT_EQ(j["fields"].size(), 1u);
    EXPECT_EQ(j["fields"][0]["key"].get<std::string>(), "v");
    EXPECT_EQ(j["fields"][0]["kind"].get<std::string>(), "u:i32,string");
}

// ---------------------------------------------------------------------------
// Registry dispatch

TEST(UnionCell, registry_dispatches_union_kind) {
    parcel::json_t inner = {{"k", "i32"}, {"v", 7}};
    const parcel::json_t j = {{"k", "u:i32,string"}, {"v", inner}};
    const auto v = populated_registry().cell_from_json(j);
    ASSERT_NE(v, nullptr);
    auto* typed = dynamic_cast<IntOrString*>(v.get());
    ASSERT_NE(typed, nullptr);
    EXPECT_EQ(typed->index(), 0u);
    EXPECT_EQ(typed->get<0>(), 7);
}

// ---------------------------------------------------------------------------
// Descriptor static alternatives()

TEST(UnionCell, descriptor_static_alternatives_lists_kinds_in_template_order) {
    constexpr auto alts =
        parcel::UnionCellTypeDescriptor<parcel::I32Cell, parcel::StringCell>::alternatives();
    ASSERT_EQ(alts.size(), 2u);
    EXPECT_EQ(alts[0], "i32");
    EXPECT_EQ(alts[1], "string");
}

// ---------------------------------------------------------------------------
// Defensive guards: alternative type mismatch on clone, lying from_json,
// valueless variant on visit.

namespace {

// LiarUnionCell claims kind_id "liar_u" but its from_json returns an
// I32Cell. Used to trigger UnionCell::try_alt's "alternative from_json did
// not return expected type" guard.
class LiarUnionCell : public parcel::BaseCell<LiarUnionCell, std::int32_t> {
    using base_t = parcel::BaseCell<LiarUnionCell, std::int32_t>;

public:
    using base_t::base_t;
    using base_t::operator=;

    [[maybe_unused]] static constexpr std::string_view kind_id = "liar_u";

    [[nodiscard]] std::string to_string() const override {
        return std::to_string(this->value);
    }

    [[maybe_unused]] static parcel::cell_t from_json(parcel::json_t const&,
                                                     parcel::ParcelRegistry const&) {
        return std::make_shared<parcel::I32Cell>(0);
    }

    [[maybe_unused]] static parcel::cell_type_descriptor_t descriptor() {
        static const auto d = std::make_shared<parcel::SimpleCellTypeDescriptor<LiarUnionCell>>(
            parcel::DisplayInfo{.name = "LiarU"});
        return d;
    }
};

// BadCloneCell's clone() returns an I32Cell instead of a BadCloneCell.
// Used to trigger UnionCell::clone's "alternative type mismatch" guard.
class BadCloneCell : public parcel::BaseCell<BadCloneCell, std::int32_t> {
    using base_t = parcel::BaseCell<BadCloneCell, std::int32_t>;

public:
    using base_t::base_t;
    using base_t::operator=;

    [[maybe_unused]] static constexpr std::string_view kind_id = "bad_clone";

    [[nodiscard]] std::string to_string() const override {
        return std::to_string(this->value);
    }

    [[nodiscard]] parcel::cell_t clone() const override {
        return std::make_shared<parcel::I32Cell>(this->value);
    }

    [[maybe_unused]] static parcel::cell_t from_json(parcel::json_t const&,
                                                     parcel::ParcelRegistry const&) {
        return std::make_shared<BadCloneCell>(0);
    }

    [[maybe_unused]] static parcel::cell_type_descriptor_t descriptor() {
        static const auto d = std::make_shared<parcel::SimpleCellTypeDescriptor<BadCloneCell>>(
            parcel::DisplayInfo{.name = "BadClone"});
        return d;
    }
};

// ThrowingStorage's int constructor throws — used to drive a std::variant
// into the valueless_by_exception() state via emplace<1>().
struct ThrowingStorage {
    ThrowingStorage() = default;
    explicit ThrowingStorage(int /*unused*/) {
        throw std::runtime_error("ThrowingStorage: boom");
    }
};

class ThrowingCell : public parcel::BaseCell<ThrowingCell, ThrowingStorage> {
    using base_t = parcel::BaseCell<ThrowingCell, ThrowingStorage>;

public:
    using base_t::base_t;
    using base_t::operator=;

    static constexpr std::string_view kind_id = "throwing";

    [[nodiscard]] std::string to_string() const override {
        return "throwing";
    }

    [[nodiscard]] parcel::json_t to_json() const override {
        return parcel::json_t{{"k", kind_id}, {"v", nullptr}};
    }

    [[maybe_unused]] static parcel::cell_t from_json(parcel::json_t const&,
                                                     parcel::ParcelRegistry const&) {
        return std::make_shared<ThrowingCell>();
    }

    [[maybe_unused]] static parcel::cell_type_descriptor_t descriptor() {
        static const auto d = std::make_shared<parcel::SimpleCellTypeDescriptor<ThrowingCell>>(
            parcel::DisplayInfo{.name = "Throwing"});
        return d;
    }
};

}  // namespace

using LiarUnion = parcel::UnionCell<LiarUnionCell, parcel::StringCell>;
using BadCloneUnion = parcel::UnionCell<BadCloneCell, parcel::StringCell>;
using ThrowingUnion = parcel::UnionCell<parcel::I32Cell, ThrowingCell>;

TEST(UnionCell, from_json_throws_when_alternative_dynamic_cast_fails) {
    parcel::json_t raw = {{"k", "liar_u"}, {"v", 0}};
    parcel::json_t j = {
        {"k", LiarUnion::kind_id},
        {"v", raw},
    };
    EXPECT_THROW((void)LiarUnion::from_json(j, registry()), std::runtime_error);
}

TEST(UnionCell, clone_throws_when_alternative_clone_returns_wrong_type) {
    BadCloneUnion u;
    u.value.emplace<0>(std::int32_t{1});
    EXPECT_THROW((void)u.clone(), std::runtime_error);
}

TEST(UnionCell, to_json_throws_on_valueless_variant) {
    ThrowingUnion u{std::int32_t{1}};
    try {
        u.value.emplace<1>(42);  // ThrowingStorage(int) throws
    } catch (std::exception const&) {
        // expected — variant *may* now be valueless (impl-defined: libc++ goes
        // valueless; libstdc++ uses the never-valueless optimization for many
        // variants and keeps the old value).
    }
    if (!u.value.valueless_by_exception()) {
        GTEST_SKIP() << "stdlib's variant did not go valueless on throwing emplace";
    }
    EXPECT_THROW((void)u.to_json(), std::runtime_error);
}
