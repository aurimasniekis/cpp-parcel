// Demo: TypedMapCell<T> (homogeneous values, raw scalars on the wire) vs
// MapCell (heterogeneous values, full {k,v} objects on the wire). Both support
// the curated std::map pass-through API. Iteration is ordered by key.
//
// Each TypedMapCell instantiation has a distinct kind id of the form
// "m:<element_kind>" (e.g. "m:string"). parcel/builtins.h ships convenience
// aliases I32MapCell, StringMapCell, etc. — used below.

#include <parcel/parcel.h>

#include <iostream>
#include <string>

namespace {

void print_section(const std::string_view title) {
    std::cout << "\n--- " << title << " ---\n";
}

}  // namespace

int main() {
    const parcel::ParcelRegistry registry;

    print_section("StringMapCell — homogeneous");
    parcel::StringMapCell tags{
        {"role", std::string{"admin"}},
        {"env", std::string{"prod"}},
    };
    tags["region"] = "eu-west-1";

    std::cout << "kind            = " << tags.kind() << '\n';  // "m:string"
    std::cout << "size            = " << tags.size() << '\n';
    std::cout << "contains(role)  = " << std::boolalpha << tags.contains("role") << '\n';
    std::cout << "at(env)         = " << tags.at("env") << '\n';
    std::cout << "to_string()     = " << tags << '\n';
    std::cout << "to_json()       = " << tags.to_json().dump() << '\n';

    std::cout << "ordered iteration:\n";
    for (auto const& [k, v] : tags) {
        std::cout << "  " << k << " -> " << v << '\n';
    }

    print_section("StringMapCell — round-trip via from_json");
    const auto wire = tags.to_json();
    const auto restored = parcel::StringMapCell::from_json(wire, registry);
    std::cout << "restored.kind() = " << restored->kind() << '\n';
    std::cout << "restored        = " << restored << '\n';

    print_section("MapCell — heterogeneous");
    const parcel::MapCell record{
        {"id", parcel::cell(101)},
        {"name", parcel::cell("alice")},
        {"admin", parcel::cell(true)},
    };

    std::cout << "kind        = " << record.kind() << '\n';
    std::cout << "size        = " << record.size() << '\n';
    std::cout << "to_string() = " << record << '\n';
    std::cout << "to_json()   =\n" << record.to_json().dump(2) << '\n';

    print_section("MapCell — round-trip via registry");
    const auto restored_record = registry.cell_from_json(record.to_json());
    std::cout << "restored.kind() = " << restored_record->kind() << '\n';
    std::cout << "restored        = " << restored_record << '\n';

    return 0;
}
