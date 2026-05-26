// Demo: opt-in lenient struct that captures unknown JSON fields into `extras`
// instead of throwing. Set `static constexpr bool allow_extra_fields = true;`
// on the derived StructCell. Extras are typed via the registry, deep-cloned by
// clone(), and re-emitted by to_json().

#include <parcel/parcel.h>

#include <iostream>
#include <memory>
#include <string>

namespace {

struct Event {
    std::string name;
    std::int32_t code{0};
};

class EventCell : public parcel::StructCell<EventCell, Event, "event"> {
public:
    using StructCell::StructCell;

    [[maybe_unused]] static constexpr bool allow_extra_fields = true;

    [[maybe_unused]] static parcel::DisplayInfo meta_info() {
        return {.name = "Event"};
    }

    [[maybe_unused]] static auto field_descriptors() {
        return parcel::FieldsBuilder<Event>{}
            .field<&Event::name>("name")
            .field<&Event::code>("code")
            .build();
    }
};

void print_section(const std::string_view title) {
    std::cout << "\n--- " << title << " ---\n";
}

}  // namespace

int main() {
    parcel::ParcelRegistry registry;
    registry.register_kind(EventCell::descriptor());

    print_section("Wire payload with two unknown fields");
    parcel::json_t v = parcel::json_t::object();
    v["name"] = parcel::json_t{{"k", "string"}, {"v", "deploy"}};
    v["code"] = parcel::json_t{{"k", "i32"}, {"v", 200}};
    v["region"] = parcel::json_t{{"k", "string"}, {"v", "eu-west-1"}};
    v["dry_run"] = parcel::json_t{{"k", "bool"}, {"v", true}};
    const parcel::json_t wire = {
        {"k", EventCell::kind_id},
        {"v", std::move(v)},
    };
    std::cout << wire.dump(2) << '\n';

    print_section("Deserialize — extras captured, not rejected");
    const auto restored = registry.cell_from_json(wire);
    auto* typed = dynamic_cast<EventCell*>(restored.get());
    std::cout << "kind        = " << typed->kind() << '\n';
    std::cout << "name        = " << typed->value.name << '\n';
    std::cout << "code        = " << typed->value.code << '\n';
    std::cout << "extras.size = " << typed->extras.size() << '\n';
    for (auto const& [key, cell] : typed->extras) {
        std::cout << "  [extra] " << key << " (kind=" << cell->kind() << ") = " << cell << '\n';
    }

    print_section("Re-emit — extras preserved");
    std::cout << typed->to_json().dump(2) << '\n';

    print_section("clone() deep-copies extras");
    const auto cloned = typed->clone();
    const auto* cloned_typed = dynamic_cast<EventCell*>(cloned.get());
    std::cout << "cloned.extras.size = " << cloned_typed->extras.size() << '\n';
    typed->extras.clear();
    std::cout << "after clearing original, cloned.extras.size = " << cloned_typed->extras.size()
              << '\n';

    return 0;
}
