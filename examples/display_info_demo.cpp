// Demo: immutable display info builders (with_name / with_description / with_icon /
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
    std::cout << "original.display_info? " << std::boolalpha
              << original->overridden_display_info().has_value() << '\n';
    std::cout << "labeled.display_info?  " << labeled->overridden_display_info().has_value()
              << '\n';
    std::cout << "labeled.display_info.name = " << *labeled->overridden_display_info()->name
              << '\n';

    print_section("Chaining — every builder clones");
    const auto annotated = parcel::I32Cell{42}
                               .with_name("Answer")
                               ->with_description("To life, the universe, and everything")
                               ->with_icon("mdi:star")
                               ->with_color("#ffcc00");
    std::cout << annotated->to_json().dump(2) << '\n';

    print_section("Round-trip preserves display info");
    const auto wire = annotated->to_json();
    const auto restored = registry.cell_from_json(wire);
    std::cout << "restored.kind()             = " << restored->kind() << '\n';
    std::cout << "restored.display_info->name         = "
              << *restored->overridden_display_info()->name << '\n';
    std::cout << "restored.display_info->description  = "
              << *restored->overridden_display_info()->description << '\n';
    std::cout << "restored.display_info->icon         = "
              << *restored->overridden_display_info()->icon << '\n';
    std::cout << "restored.display_info->color        = "
              << *restored->overridden_display_info()->color << '\n';

    print_section("with_display_info replaces the whole struct");
    const auto replaced =
        annotated->with_display_info(parcel::DisplayInfo{.description = "just a number"});
    std::cout << "replaced.display_info->name?         "
              << replaced->overridden_display_info()->name.has_value() << '\n';
    std::cout << "replaced.display_info->description = "
              << *replaced->overridden_display_info()->description << '\n';

    return 0;
}
