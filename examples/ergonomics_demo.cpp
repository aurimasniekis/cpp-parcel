// Demo: heterogeneous builders, range/span construction, the broader
// default_cell_for<T> mappings, and the variadic registry surface.

#include <parcel/parcel.h>

#include <array>
#include <deque>
#include <iostream>
#include <list>
#include <ranges>
#include <set>
#include <span>
#include <string>
#include <string_view>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <vector>

namespace {

struct Person {
    std::int32_t id{0};
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
    [[maybe_unused]] static parcel::DisplayInfo display_info() {
        return {.name = "Address"};
    }
    [[maybe_unused]] static auto field_descriptors() {
        return parcel::FieldsBuilder<Address>{}.field<&Address::city>("city").build();
    }
};

void print_section(const std::string_view title) {
    std::cout << "\n--- " << title << " ---\n";
}

// Compile-time proofs for the new default_cell_for mappings.
static_assert(std::is_same_v<parcel::default_cell_for_t<std::array<std::int32_t, 3>>,
                             parcel::TypedListCell<parcel::I32Cell>>);
static_assert(std::is_same_v<parcel::default_cell_for_t<std::deque<std::string>>,
                             parcel::TypedListCell<parcel::StringCell>>);
static_assert(std::is_same_v<parcel::default_cell_for_t<std::list<std::int32_t>>,
                             parcel::TypedListCell<parcel::I32Cell>>);
static_assert(std::is_same_v<parcel::default_cell_for_t<std::set<std::int32_t>>,
                             parcel::TypedListCell<parcel::I32Cell>>);
static_assert(
    std::is_same_v<parcel::default_cell_for_t<std::unordered_map<std::string, std::int32_t>>,
                   parcel::TypedMapCell<parcel::I32Cell>>);

}  // namespace

int main() {
    print_section("parcel::cell(value) — auto-wrap a raw value");
    auto wrapped_int = parcel::cell(42);
    auto wrapped_string = parcel::cell(std::string("hi"));
    auto wrapped_lit = parcel::cell("literal-normalises-to-string");
    auto wrapped_bool = parcel::cell(true);
    std::cout << "cell(42)        kind=" << wrapped_int->kind() << "    value=" << *wrapped_int
              << '\n';
    std::cout << "cell(\"hi\")      kind=" << wrapped_string->kind() << " value=" << *wrapped_string
              << '\n';
    std::cout << "cell(literal)   kind=" << wrapped_lit->kind() << " value=" << *wrapped_lit
              << '\n';
    std::cout << "cell(true)      kind=" << wrapped_bool->kind() << "   value=" << *wrapped_bool
              << '\n';

    print_section("make_list — heterogeneous ListCell from raw values");
    auto l = parcel::make_list(1, std::string("hi"), true);
    std::cout << "make_list size=" << l->size() << "  kinds=[";
    for (std::size_t k = 0; k < l->size(); ++k) {
        std::cout << (k == 0 ? "" : ",") << l->at(k)->kind();
    }
    std::cout << "]\n";
    std::cout << "to_json = " << l->to_json().dump() << '\n';

    print_section("make_map — heterogeneous MapCell from {key, cell} pairs");
    auto m = parcel::make_map({
        {"x", parcel::cell(1)},
        {"y", parcel::cell(std::string("hi"))},
        {"z", parcel::cell(true)},
    });
    std::cout << "make_map size=" << m->size() << '\n';
    std::cout << "to_json = " << m->to_json().dump() << '\n';

    print_section("default_cell_for — broader STL coverage (proven by static_assert above)");
    std::cout << "std::array<i32,3>             -> TypedListCell<I32Cell>\n";
    std::cout << "std::deque<string>            -> TypedListCell<StringCell>\n";
    std::cout << "std::list<i32>                -> TypedListCell<I32Cell>\n";
    std::cout << "std::set<i32>                 -> TypedListCell<I32Cell>\n";
    std::cout << "std::unordered_map<str, i32>  -> TypedMapCell<I32Cell>\n";

    print_section("TypedListCell(std::from_range, view) over std::views::filter");
    std::vector<std::int32_t> source{1, 2, 3, 4, 5};
    auto evens = source | std::views::filter([](const int x) { return x % 2 == 0; });
    parcel::I32ListCell evens_cell(std::from_range, evens);
    std::cout << "evens = " << evens_cell << "  size=" << evens_cell.size() << '\n';

    print_section("TypedListCell::as_span — const view + mutable in-place edit");
    parcel::I32ListCell mutable_list{10, 20, 30};
    {
        const auto sp = mutable_list.as_span();
        std::cout << "const span[1] = " << sp[1] << '\n';
    }
    {
        auto sp = mutable_list.as_span();
        sp[1] = 99;
    }
    std::cout << "after mutating span[1] = " << mutable_list[1] << "  list = " << mutable_list
              << '\n';

    print_section("TypedMapCell(std::from_range, vector<pair>) + keys() / values() (sorted)");
    std::vector<std::pair<std::string, std::int32_t>> pairs{{"b", 2}, {"a", 1}, {"c", 3}};
    parcel::I32MapCell typed_map(std::from_range, pairs);
    std::cout << "keys (sorted): ";
    for (auto const& k : typed_map.keys()) {
        std::cout << k << ' ';
    }
    std::cout << "\nvalues (key order): ";
    for (auto const& v : typed_map.values()) {
        std::cout << v << ' ';
    }
    std::cout << '\n';

    print_section("MapCell::keys() / values() for the heterogeneous flavor");
    parcel::MapCell hetero_map{
        {"x", parcel::cell(1)},
        {"a", parcel::cell(std::string("alpha"))},
    };
    std::cout << "keys (sorted): ";
    for (auto const& k : hetero_map.keys()) {
        std::cout << k << ' ';
    }
    std::cout << "\nvalues (key order): ";
    for (auto const& v : hetero_map.values()) {
        std::cout << v << ' ';
    }
    std::cout << '\n';

    print_section("ParcelRegistry::register_kinds + register_cells (variadic, chainable)");
    parcel::ParcelRegistry reg{parcel::BuiltinsOptions{
        .primitives = false, .collections = false, .typed_collections = false, .std = false}};
    auto& self = reg.register_cells<PersonCell>().register_kinds(AddressCell::descriptor());
    std::cout << "&self == &reg              : " << (&self == &reg ? "yes" : "no") << '\n';
    std::cout << "reg.contains(\"s:person\")   : "
              << (reg.contains(PersonCell::kind_id) ? "yes" : "no") << '\n';
    std::cout << "reg.contains(\"s:address\")  : "
              << (reg.contains(AddressCell::kind_id) ? "yes" : "no") << '\n';

#if PARCEL_HAS_EXPECTED
    print_section("try_cell_from_json — happy path on user kind");
    {
        // Re-use a registry with builtins so the inner i32/string fields resolve.
        parcel::ParcelRegistry full;
        full.register_cells<PersonCell, AddressCell>();
        const auto src = PersonCell::of(Person{.id = 1, .name = "Ada"});
        if (auto r = full.try_cell_from_json(src->to_json()); r.has_value()) {
            std::cout << "ok: kind=" << r.value()->kind() << "  value=" << r.value() << '\n';
        }
    }

    print_section("try_cell_from_string — unknown kind path");
    {
        const parcel::ParcelRegistry empty{parcel::BuiltinsOptions{
            .primitives = false, .collections = false, .typed_collections = false, .std = false}};
        if (auto r = empty.try_cell_from_string(R"({"k":"i32","v":1})"); !r.has_value()) {
            std::cout << "error: code=" << static_cast<int>(r.error().code)
                      << "  kind=" << r.error().kind << "  message=" << r.error().message << '\n';
        }
    }
#else
    print_section("try_cell_from_json / try_cell_from_string — not available (no std::expected)");
    std::cout << "fall back to throwing cell_from_json instead\n";
#endif

    return 0;
}
