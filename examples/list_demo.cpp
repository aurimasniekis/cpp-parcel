// Demo: TypedListCell<T> (homogeneous, raw scalars on the wire) vs ListCell
// (heterogeneous, full {k,v} objects on the wire). Both support the curated
// std::vector pass-through API.
//
// Each TypedListCell instantiation has a distinct kind id of the form
// "l:<element_kind>" (e.g. "l:i32"), so the registry dispatches each one
// independently. parcel/builtins.h ships convenience aliases I32ListCell,
// StringListCell, etc. — used below.

#include <parcel/parcel.h>

#include <iostream>
#include <string_view>

namespace {

void print_section(const std::string_view title) {
    std::cout << "\n--- " << title << " ---\n";
}

}  // namespace

int main() {
    const parcel::ParcelRegistry registry;

    print_section("I32ListCell — homogeneous");
    parcel::I32ListCell ints{1, 2, 3};
    ints.push_back(4);
    ints.emplace_back(5);

    std::cout << "kind         = " << ints.kind() << '\n';  // "l:i32"
    std::cout << "size         = " << ints.size() << '\n';
    std::cout << "front, back  = " << ints.front() << ", " << ints.back() << '\n';
    std::cout << "to_string()  = " << ints << '\n';
    std::cout << "to_json()    = " << ints.to_json().dump() << '\n';

    int sum = 0;
    for (auto const& el : ints) {
        sum += el;
    }
    std::cout << "range-for sum = " << sum << '\n';

    print_section("I32ListCell — round-trip via from_json");
    const auto wire = ints.to_json();
    const auto restored = parcel::I32ListCell::from_json(wire, registry);
    std::cout << "restored.kind() = " << restored->kind() << '\n';
    std::cout << "restored        = " << restored << '\n';

    print_section("ListCell — heterogeneous");
    parcel::ListCell mixed{
        parcel::cell(42),
        parcel::cell("hello"),
        parcel::cell(true),
    };
    mixed.push_back(parcel::cell(7));

    std::cout << "kind        = " << mixed.kind() << '\n';
    std::cout << "size        = " << mixed.size() << '\n';
    std::cout << "to_string() = " << mixed << '\n';
    std::cout << "to_json()   =\n" << mixed.to_json().dump(2) << '\n';

    print_section("ListCell — round-trip via registry");
    const auto restored_mixed = registry.cell_from_json(mixed.to_json());
    std::cout << "restored.kind() = " << restored_mixed->kind() << '\n';
    std::cout << "restored        = " << restored_mixed << '\n';

    return 0;
}
