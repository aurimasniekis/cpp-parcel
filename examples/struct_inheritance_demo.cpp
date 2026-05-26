// Demo: reuse one struct cell's field list inside another via FieldsBuilder
// helpers. When a payload extends another in C++ (`struct B : A`), the
// derived cell can:
//   - .extend<ParentCell>()    splice every parent field in declaration order
//   - .field<&Base::x>("x")    override an inherited field by JSON key
//   - .remove_field("k")       drop an inherited field by JSON key
//
// The wire shape stays flat — every field, inherited or not, lives at the
// top level of the cell's "v" object. The schema does NOT record a
// parent-kind link; field-list reuse is purely a builder concern.

#include <parcel/parcel.h>

#include <iostream>
#include <memory>
#include <string>

namespace demo {

// 1. Payloads — plain C++ inheritance.

struct StreetAddress {
    std::string street;
    std::string city;
};

struct HomeAddress : StreetAddress {
    std::string label;
};

// 2. The base cell.

class StreetAddressCell
    : public parcel::StructCell<StreetAddressCell, StreetAddress, "street_address"> {
public:
    using StructCell::StructCell;

    [[maybe_unused]] static parcel::DisplayInfo display_info() {
        return {.name = "StreetAddress", .description = "Postal address"};
    }

    [[maybe_unused]] static auto field_descriptors() {
        return parcel::FieldsBuilder<StreetAddress>{}
            .field<&StreetAddress::street>("street")
            .name("Street")
            .field<&StreetAddress::city>("city")
            .name("City")
            .build();
    }
};

// 3. The derived cell — splices in StreetAddressCell's fields, adds its own.

class HomeAddressCell : public parcel::StructCell<HomeAddressCell, HomeAddress, "home_address"> {
public:
    using StructCell::StructCell;

    [[maybe_unused]] static parcel::DisplayInfo display_info() {
        return {.name = "HomeAddress"};
    }

    [[maybe_unused]] static auto field_descriptors() {
        return parcel::FieldsBuilder<HomeAddress>{}
            .extend<StreetAddressCell>()  // street + city — declaration order preserved
            .field<&HomeAddress::label>("label")
            .name("Label")
            .build();
    }
};

// 4. A second derived cell that *overrides* an inherited field by key and
//    drops another. Same payload as HomeAddress; just a different schema.

class HomeAddressTrimmedCell
    : public parcel::StructCell<HomeAddressTrimmedCell, HomeAddress, "home_address_trimmed"> {
public:
    using StructCell::StructCell;

    [[maybe_unused]] static parcel::DisplayInfo display_info() {
        return {.name = "HomeAddressTrimmed"};
    }

    [[maybe_unused]] static auto field_descriptors() {
        return parcel::FieldsBuilder<HomeAddress>{}
            .extend<StreetAddressCell>()
            .remove_field("city")  // drop inherited "city"
            // Override "street" — same key replaces the inherited entry in
            // place, the member pointer can point at the base member.
            .field<&StreetAddress::street>("street")
            .name("StreetOverridden")
            .field<&HomeAddress::label>("label")
            .build();
    }
};

void print_section(const std::string_view title) {
    std::cout << "\n--- " << title << " ---\n";
}

}  // namespace demo

int main() {
    parcel::ParcelRegistry registry;
    registry.register_kind(demo::StreetAddressCell::descriptor());
    registry.register_kind(demo::HomeAddressCell::descriptor());
    registry.register_kind(demo::HomeAddressTrimmedCell::descriptor());

    demo::print_section("Build a HomeAddress");
    demo::HomeAddress h;
    h.street = "1 Main St";
    h.city = "Springfield";
    h.label = "primary";
    const demo::HomeAddressCell home{h};
    std::cout << "to_string() = " << home << '\n';

    demo::print_section("to_json — flat wire (street, city, label)");
    std::cout << home.to_json().dump(2) << '\n';

    demo::print_section("Round-trip via registry");
    const auto restored = registry.cell_from_json(home.to_json());
    const auto* typed = dynamic_cast<demo::HomeAddressCell*>(restored.get());
    std::cout << "street = " << typed->value.street << '\n';
    std::cout << "city   = " << typed->value.city << '\n';
    std::cout << "label  = " << typed->value.label << '\n';

    demo::print_section("HomeAddressTrimmed — override + remove_field");
    const auto trimmed_desc = demo::HomeAddressTrimmedCell::descriptor();
    std::cout << trimmed_desc->to_json().dump(2) << '\n';

    demo::print_section("Schema — define('s:home_address')");
    std::cout << registry.define("s:home_address").to_json().dump(2) << '\n';

    return 0;
}
