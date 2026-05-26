// Demo: construct primitive values, serialize them to JSON, ship them over
// the (imagined) wire, deserialize on the other side, and inspect descriptors.

#include <parcel/parcel.h>

#include <iostream>
#include <string_view>

namespace {

void print_section(const std::string_view title) {
    std::cout << "\n--- " << title << " ---\n";
}

}  // namespace

int main() {
    const parcel::ParcelRegistry registry;  // primitives don't consult it, but the API requires one

    print_section("Construction + to_string");
    const parcel::I32Cell answer = 42;
    const parcel::DoubleCell pi = 3.14159;
    const parcel::BoolCell flag = true;
    const parcel::StringCell greeting = "hello, wire!";
    const parcel::CharCell letter = 'P';

    std::cout << "answer   (" << answer.kind() << ") = " << answer << '\n';
    std::cout << "pi       (" << pi.kind() << ") = " << pi << '\n';
    std::cout << "flag     (" << flag.kind() << ") = " << flag << '\n';
    std::cout << "greeting (" << greeting.kind() << ") = " << greeting << '\n';
    std::cout << "letter   (" << letter.kind() << ") = " << letter << '\n';

    print_section("to_json — wire format");
    std::cout << "answer.to_json()   = " << answer.to_json().dump() << '\n';
    std::cout << "greeting.to_json() = " << greeting.to_json().dump() << '\n';
    std::cout << "flag.to_json()     = " << flag.to_json().dump() << '\n';

    print_section("Round trip via from_json");
    const auto wire = greeting.to_json();
    const auto restored = parcel::StringCell::from_json(wire, registry);
    std::cout << "restored kind = " << restored->kind() << '\n';
    std::cout << "restored value = " << restored << '\n';

    print_section("Round trip via descriptor (registry-driven dispatch)");
    const auto desc = parcel::I32Cell::descriptor();
    const auto restored_via_desc = desc->cell_from_json(answer.to_json(), registry);
    std::cout << "descriptor->kind()       = " << desc->kind() << '\n';
    std::cout << "restored kind            = " << restored_via_desc->kind() << '\n';
    std::cout << "restored to_string       = " << restored_via_desc << '\n';

    print_section("Descriptor display info");
    std::cout << parcel::DoubleCell::descriptor()->to_json().dump(2) << '\n';

    print_section("clone()");
    parcel::I32Cell original = 7;
    const auto cloned = original.clone();
    original.value = 999;
    std::cout << "original (after mutation) = " << original << '\n';
    std::cout << "cloned (untouched)        = " << cloned << '\n';

    return 0;
}
