// Demo: every documented runtime_error path that parcel can throw during
// deserialization or registry use. JSON deserialization is strict — every
// documented field is mandatory and verified.

#include <parcel/parcel.h>

#include <exception>
#include <iostream>
#include <string>

namespace {

struct Order {
    std::int32_t id{0};
    std::string sku;
};

class OrderCell : public parcel::StructCell<OrderCell, Order, "order"> {
public:
    using StructCell::StructCell;

    [[maybe_unused]] static parcel::descriptor::MetaInfo meta_info() {
        return {.name = "Order"};
    }

    [[maybe_unused]] static auto field_descriptors() {
        return parcel::FieldsBuilder<Order>{}
            .field<&Order::id>("id")
            .field<&Order::sku>("sku")
            .build();
    }
};

template <typename F>
void expect_throw(const std::string_view what, F&& f) {
    try {
        f();
        std::cout << "  [BUG] " << what << " did not throw\n";
    } catch (std::exception const& e) {
        std::cout << "  [ok] " << what << "  -> " << e.what() << '\n';
    }
}

void print_section(const std::string_view title) {
    std::cout << "\n--- " << title << " ---\n";
}

}  // namespace

int main() {
    parcel::ParcelRegistry registry;
    registry.register_kind(OrderCell::descriptor());

    print_section("Primitive — missing/invalid 'k'");
    expect_throw("missing 'k'", [] {
        const parcel::json_t j = {{"v", 1}};
        parcel::I32Cell::from_json(j, parcel::ParcelRegistry{});
    });
    expect_throw("wrong 'k'", [] {
        const parcel::json_t j = {{"k", "string"}, {"v", 1}};
        parcel::I32Cell::from_json(j, parcel::ParcelRegistry{});
    });

    print_section("TypedListCell — missing/wrong kind");
    expect_throw("missing 'k'", [] {
        const parcel::json_t j = {{"v", parcel::json_t::array({1})}};
        parcel::TypedListCell<parcel::I32Cell>::from_json(j, parcel::ParcelRegistry{});
    });
    expect_throw("element kind mismatch", [] {
        // Element kind is encoded in 'k'; "l:i64" is not the same kind as
        // TypedListCell<I32Cell>::kind_id ("l:i32") — must throw.
        const parcel::json_t j = {
            {"k", "l:i64"},
            {"v", parcel::json_t::array({1})},
        };
        parcel::TypedListCell<parcel::I32Cell>::from_json(j, parcel::ParcelRegistry{});
    });

    print_section("UnionCell — active kind not in alternatives");
    using IntStr = parcel::UnionCell<parcel::I32Cell, parcel::StringCell>;
    expect_throw("inner v.k = 'f64' not in alternatives", [&] {
        // The active alternative is recovered from the inner v.k. "f64" is
        // not one of {"i32","string"} so dispatch must throw.
        const parcel::json_t inner = {{"k", "f64"}, {"v", 1.0}};
        const parcel::json_t j = {{"k", IntStr::kind_id}, {"v", inner}};
        IntStr::from_json(j, registry);
    });

    print_section("StructCell — missing required field");
    expect_throw("missing 'sku'", [&] {
        const parcel::json_t inner_id = {{"k", "i32"}, {"v", 1}};
        parcel::json_t v = parcel::json_t::object();
        v["id"] = inner_id;
        const parcel::json_t j = {{"k", "s:order"}, {"v", std::move(v)}};
        OrderCell::from_json(j, registry);
    });

    print_section("StructCell — unknown field on closed struct");
    expect_throw("unknown field 'extra'", [&] {
        const parcel::json_t inner_id = {{"k", "i32"}, {"v", 1}};
        const parcel::json_t inner_sku = {{"k", "string"}, {"v", "ABC"}};
        const parcel::json_t inner_x = {{"k", "i32"}, {"v", 9}};
        parcel::json_t v = parcel::json_t::object();
        v["id"] = inner_id;
        v["sku"] = inner_sku;
        v["extra"] = inner_x;
        const parcel::json_t j = {{"k", "s:order"}, {"v", std::move(v)}};
        OrderCell::from_json(j, registry);
    });

    print_section("ParcelRegistry — unknown kind");
    expect_throw("unknown kind 'foo'", [&] {
        const parcel::json_t j = {{"k", "foo"}, {"v", 0}};
        (void)registry.cell_from_json(j);
    });

    print_section("ParcelRegistry::define — unregistered kind");
    expect_throw("define('does_not_exist')", [&] { (void)registry.define("does_not_exist"); });

    return 0;
}
