// Demo: nest a struct cell inside another struct via PARCEL_DEFAULT_CELL.
//
// Without the macro, declaring `Person::address` would force you to spell out
// the wrapper at every field site:
//   .field<&Person::address, AddressCell>("address")
// With one PARCEL_DEFAULT_CELL line below the cell, FieldsBuilder infers the
// wrapper from the field type alone:
//   .field<&Person::address>("address")
// — same way it handles primitives, vectors, maps, optionals, and variants.

#include <parcel/parcel.h>

#include <iostream>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace demo {

struct Address {
    std::string street;
    std::string city;
    std::string country;
};

class AddressCell : public parcel::StructCell<AddressCell, Address, "address"> {
public:
    using StructCell::StructCell;

    [[maybe_unused]] static parcel::DisplayInfo display_info() {
        return {.name = "Address"};
    }

    [[maybe_unused]] static auto field_descriptors() {
        return parcel::FieldsBuilder<Address>{}
            .field<&Address::street>("street")
            .field<&Address::city>("city")
            .field<&Address::country>("country")
            .build();
    }
};

}  // namespace demo

// One line — Address is now inference-eligible everywhere FieldsBuilder
// looks at field types (including inside std::vector, std::optional, etc).
PARCEL_DEFAULT_CELL(demo::AddressCell);

namespace demo {

struct Person {
    std::string name;
    Address address;                 // -> AddressCell, inferred
    std::optional<Address> billing;  // -> AddressCell, inferred (not required)
    std::vector<Address> previous;   // -> TypedListCell<AddressCell>, inferred
};

class PersonCell : public parcel::StructCell<PersonCell, Person, "person"> {
public:
    using StructCell::StructCell;

    [[maybe_unused]] static parcel::DisplayInfo display_info() {
        return {.name = "Person"};
    }

    [[maybe_unused]] static auto field_descriptors() {
        return parcel::FieldsBuilder<Person>{}
            .field<&Person::name>("name")
            .field<&Person::address>("address")
            .field<&Person::billing>("billing")
            .field<&Person::previous>("previous")
            .build();
    }
};

}  // namespace demo

PARCEL_DEFAULT_CELL(demo::PersonCell);

namespace {

void print_section(const std::string_view title) {
    std::cout << "\n--- " << title << " ---\n";
}

}  // namespace

int main() {
    parcel::ParcelRegistry registry;
    registry.register_kind(demo::AddressCell::descriptor());
    registry.register_kind(parcel::TypedListCell<demo::AddressCell>::descriptor());
    registry.register_kind(demo::PersonCell::descriptor());

    print_section("Build a Person with a nested Address (and a list of past ones)");
    const demo::PersonCell alice = demo::Person{
        .name = "Alice",
        .address = {.street = "1 Main St", .city = "Vilnius", .country = "LT"},
        .billing = std::nullopt,
        .previous =
            {
                {.street = "9 Old Rd", .city = "Kaunas", .country = "LT"},
            },
    };
    std::cout << "to_string() = " << alice << '\n';

    print_section("to_json — nested AddressCell shows up under address.v");
    std::cout << alice.to_json().dump(2) << '\n';

    print_section("Round-trip");
    const auto restored = registry.cell_from_json(alice.to_json());
    const auto* typed = dynamic_cast<demo::PersonCell*>(restored.get());
    std::cout << "name           = " << typed->value.name << '\n';
    std::cout << "address.city   = " << typed->value.address.city << '\n';
    std::cout << "billing        = " << (typed->value.billing ? "present" : "<nullopt>") << '\n';
    std::cout << "previous.size  = " << typed->value.previous.size() << '\n';
    for (auto const& a : typed->value.previous) {
        std::cout << "  prev: " << a.street << ", " << a.city << '\n';
    }

    print_section("parcel::cell(Address{...}) also picks up the registration");
    const auto wrapped =
        parcel::cell(demo::Address{.street = "free fn", .city = "Anywhere", .country = "XX"});
    std::cout << "wrapped.kind() = " << wrapped->kind() << '\n';
    std::cout << "wrapped       = " << wrapped << '\n';

    return 0;
}
