// Demo: the commons vocabulary cells. parcel ships adapter cells for the
// aurimasniekis/cpp-commons types — Color, Icon, DisplayInfo, and flags — so
// they can travel on the wire and round-trip through a default registry.

#include <parcel/parcel.h>

#include <iostream>
#include <string>
#include <vector>

namespace {

// COMMONS_DEFINE_FLAG declares a flag type and self-registers it into the
// program-wide comms::GlobalFlagRegistry, so a flag name read back from JSON
// can be resolved to a FlagRef.
COMMONS_DEFINE_FLAG(BetaFlag, "feature.beta");
COMMONS_DEFINE_FLAG(InternalFlag, "feature.internal");

void print_section(const std::string_view title) {
    std::cout << "\n--- " << title << " ---\n";
}

}  // namespace

int main() {
    const parcel::ParcelRegistry registry;

    print_section("ColorCell — hex on the wire");
    const auto color = parcel::ColorCell::of(*comms::Color::parse("#6366f1"));
    std::cout << "to_string = " << color->to_string() << '\n';
    std::cout << "to_json   = " << color->to_json().dump() << '\n';

    print_section("IconCell — Iconify set:name on the wire");
    const auto icon = parcel::IconCell::of(comms::Icon::from("mdi:rocket-launch"));
    std::cout << "to_string = " << icon->to_string() << '\n';
    std::cout << "to_json   = " << icon->to_json().dump() << '\n';

    print_section("DisplayInfoCell — presentation display-info object");
    const comms::DisplayInfo di{
        .name = "Launch",
        .description = "Start the mission",
        .icon = comms::Icon::from("mdi:rocket-launch"),
        .color = comms::Color::parse("indigo"),
    };
    const auto info = parcel::DisplayInfoCell::of(di);
    std::cout << "to_json   = " << info->to_json().dump(2) << '\n';

    print_section("FlagCell / FlagSetCell — names resolved against the registry");
    const auto flag = parcel::FlagCell::of(comms::FlagRef::of<BetaFlag>());
    const auto flags = parcel::FlagSetCell::of(comms::FlagSet::of<BetaFlag, InternalFlag>());
    std::cout << "flag       = " << flag->to_string() << '\n';
    std::cout << "flag_set   = " << flags->to_string() << '\n';

    print_section("Round-trip through the default registry");
    const std::vector<parcel::cell_t> cells{color, icon, info, flag, flags};
    for (const auto& cell : cells) {
        const auto restored = registry.cell_from_json(cell->to_json());
        std::cout << cell->kind() << ": " << (*restored == *cell ? "equal" : "DIFFERENT") << '\n';
    }

    return 0;
}
