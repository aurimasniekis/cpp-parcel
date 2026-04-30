// Demo: SelfStructCell + a CRTP intermediate that carves out its own
// kind-id namespace.
//
// `parcel::StructCell` composes a separate payload struct into the cell.
// `parcel::SelfStructCell` lets the deriving class *be* the payload — useful
// for library authors who want to ship a CRTP base (here: BaseEvent) that
// owns common fields, declares its own wire-namespace prefix (here:
// "s:event:"), and lets concrete subclasses add their own fields.

#include <parcel/parcel.h>

#include <cstdint>
#include <iostream>
#include <string>
#include <string_view>

namespace demo {

// 1. Library-style CRTP base.
//
//    - Uses `id_join_lit_v` to compose its own wire namespace from a
//      fixed-string literal NTTP.
//    - Owns a protected `timestamp_` field; no public setter is exposed
//      beyond what the base defines.
//    - Provides a hook (`event_field_descriptors`) the concrete cell
//      implements to declare its own fields.

template <typename Self, parcel::fixed_string EventId>
class BaseEvent : public parcel::SelfStructCell<Self> {
public:
    static constexpr std::string_view kind_id = parcel::id_join_lit_v<"s:event:", EventId>;

protected:
    std::int64_t timestamp_{};

public:
    [[nodiscard]] std::int64_t timestamp() const {
        return timestamp_;
    }
    void set_timestamp(const std::int64_t v) {
        timestamp_ = v;
    }

    [[maybe_unused]] static parcel::descriptor::MetaInfo meta_info() {
        return {.name = "Event", .description = "Common event base"};
    }

    [[maybe_unused]] static auto field_descriptors() {
        auto builder = parcel::FieldsBuilder<Self>{};
        return Self::event_field_descriptors(builder)
            .template field<&BaseEvent::timestamp_>("timestamp")
            .name("Timestamp")
            .build();
    }
};

// 2. Concrete event — adds its own fields and provides the hook.

class SomethingEvent : public BaseEvent<SomethingEvent, "something"> {
public:
    std::string action;
    std::int32_t weight{0};

    [[maybe_unused]] static auto&
    event_field_descriptors(parcel::FieldsBuilder<SomethingEvent>& b) {
        return b.field<&SomethingEvent::action>("action")
            .name("Action")
            .field<&SomethingEvent::weight>("weight")
            .name("Weight");
    }
};

void print_section(std::string_view title) {
    std::cout << "\n--- " << title << " ---\n";
}

}  // namespace demo

int main() {
    parcel::ParcelRegistry registry;
    registry.register_kind(demo::SomethingEvent::descriptor());

    demo::print_section("Build a SomethingEvent");
    demo::SomethingEvent ev;
    ev.action = "click";
    ev.weight = 5;
    ev.set_timestamp(1700000000);

    std::cout << "kind_id   = " << demo::SomethingEvent::kind_id << '\n';
    std::cout << "to_string = " << ev.to_string() << '\n';

    demo::print_section("to_json — wire shape");
    const auto j = ev.to_json();
    std::cout << j.dump(2) << '\n';

    demo::print_section("Descriptor");
    std::cout << demo::SomethingEvent::descriptor()->to_json().dump(2) << '\n';

    demo::print_section("Round-trip via registry");
    const auto restored = registry.cell_from_json(j);
    const auto* typed = dynamic_cast<demo::SomethingEvent*>(restored.get());
    std::cout << "kind()    = " << typed->kind() << '\n';
    std::cout << "action    = " << typed->action << '\n';
    std::cout << "weight    = " << typed->weight << '\n';
    std::cout << "timestamp = " << typed->timestamp() << '\n';

    return 0;
}
