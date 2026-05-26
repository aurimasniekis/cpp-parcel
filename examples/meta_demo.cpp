// Demo: immutable meta builders (with_name / with_description / with_icon /
// with_color) and the "d" wire block. Builders return a fresh cell_t — they
// never mutate the receiver.

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

    print_section("Immutability — builders return a new cell");
    const auto original = parcel::I32Cell::of(7);
    const auto labeled = original->with_name("Lucky");
    std::cout << "original.meta? " << std::boolalpha << original->meta().has_value() << '\n';
    std::cout << "labeled.meta?  " << labeled->meta().has_value() << '\n';
    std::cout << "labeled.meta.name = " << *labeled->meta()->name << '\n';

    print_section("Chaining — every builder clones");
    const auto annotated = parcel::I32Cell{42}
                               .with_name("Answer")
                               ->with_description("To life, the universe, and everything")
                               ->with_icon("mdi:star")
                               ->with_color("#ffcc00");
    std::cout << annotated->to_json().dump(2) << '\n';

    print_section("Round-trip preserves meta");
    const auto wire = annotated->to_json();
    const auto restored = registry.cell_from_json(wire);
    std::cout << "restored.kind()             = " << restored->kind() << '\n';
    std::cout << "restored.meta->name         = " << *restored->meta()->name << '\n';
    std::cout << "restored.meta->description  = " << *restored->meta()->description << '\n';
    std::cout << "restored.meta->icon         = " << *restored->meta()->icon << '\n';
    std::cout << "restored.meta->color        = " << *restored->meta()->color << '\n';

    print_section("with_meta replaces the whole struct");
    const auto replaced = annotated->with_meta(parcel::DisplayInfo{.description = "just a number"});
    std::cout << "replaced.meta->name?         " << replaced->meta()->name.has_value() << '\n';
    std::cout << "replaced.meta->description = " << *replaced->meta()->description << '\n';

    return 0;
}
