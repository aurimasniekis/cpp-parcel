// Demo: ParcelRegistry — registration, polymorphic deserialization
// (cell_from_json), category/storage queries, and Definition (transitive
// schema closure).

#include <parcel/parcel.h>

#include <iostream>
#include <memory>
#include <string>
#include <vector>

namespace {

struct Address {
    std::string city;
    std::int32_t zip{0};
};

class AddressCell : public parcel::StructCell<AddressCell, Address, "address"> {
public:
    using StructCell::StructCell;

    [[maybe_unused]] static parcel::DisplayInfo meta_info() {
        return {.name = "Address"};
    }

    [[maybe_unused]] static auto field_descriptors() {
        return parcel::FieldsBuilder<Address>{}
            .field<&Address::city>("city")
            .field<&Address::zip>("zip")
            .build();
    }
};

}  // namespace

template <>
struct parcel::default_cell_for<Address> {
    using type = AddressCell;
};

namespace {

struct Person {
    std::string name;
    Address home;
    std::vector<std::string> tags;
};

class PersonCell : public parcel::StructCell<PersonCell, Person, "person"> {
public:
    using StructCell::StructCell;

    [[maybe_unused]] static parcel::DisplayInfo meta_info() {
        return {.name = "Person"};
    }

    [[maybe_unused]] static auto field_descriptors() {
        return parcel::FieldsBuilder<Person>{}
            .field<&Person::name>("name")
            .field<&Person::home>("home")
            .field<&Person::tags>("tags")
            .build();
    }
};

void print_section(const std::string_view title) {
    std::cout << "\n--- " << title << " ---\n";
}

}  // namespace

int main() {
    // Default-constructing a ParcelRegistry already populates the
    // non-templated builtins (every PrimitiveCell + ListCell + MapCell).
    // Equivalent to:
    //   parcel::ParcelRegistry registry({.primitives = false, .collections = false});
    //   parcel::register_builtins(registry);
    parcel::ParcelRegistry registry;

    // User kinds — union and struct require concrete template instantiations
    // or user-chosen kind ids, so they are still registered explicitly.
    // (TypedListCell/TypedMapCell of every primitive are pre-registered as
    // builtins, so StringListCell etc. need no manual setup here.)
    registry.register_kind(AddressCell::descriptor());
    registry.register_kind(PersonCell::descriptor());

    print_section("Registry contents");
    std::cout << "count = " << registry.count() << '\n';
    std::cout << "kinds = ";
    for (const auto k : registry.kinds()) {
        std::cout << k << ' ';
    }
    std::cout << '\n';
    std::cout << "contains('s:person') = " << std::boolalpha << registry.contains("s:person")
              << '\n';

    print_section("Heterogeneous JSON array — polymorphic dispatch");
    const parcel::I32Cell answer = 42;
    const parcel::StringCell hello = "hello";
    const PersonCell alice = Person{
        .name = "Alice",
        .home = Address{.city = "NYC", .zip = 10001},
        .tags = {"admin", "ops"},
    };

    const std::vector<parcel::json_t> wire{
        answer.to_json(),
        hello.to_json(),
        alice.to_json(),
    };
    for (auto const& j : wire) {
        const auto cell = registry.cell_from_json(j);
        std::cout << "[" << cell->kind() << "] " << cell << '\n';
    }

    print_section("find_by_category");
    for (auto const& d : registry.find_by_category(parcel::descriptor::CellCategory::Struct)) {
        std::cout << "  struct: " << d->kind() << '\n';
    }
    for (auto const& d : registry.find_by_category(parcel::descriptor::CellCategory::Primitive)) {
        std::cout << "  primitive: " << d->kind() << '\n';
    }

    print_section("find_by_storage<std::string>");
    for (auto const& d : registry.find_by_storage<std::string>()) {
        std::cout << "  " << d->kind() << '\n';
    }

    print_section("define('s:person') — transitive schema closure");
    const auto def = registry.define("s:person");
    std::cout << def.to_json().dump(2) << '\n';

    return 0;
}
