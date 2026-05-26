// Demo: define a payload struct + StructCell wrapper using FieldsBuilder.
//   - required and optional fields
//   - inferred field types via default_cell_for (no explicit CellT needed)
//   - JSON round-trip via the registry
//   - inspect descriptor()->fields()

#include <parcel/parcel.h>

#include <iostream>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace {

struct Person {
    std::int32_t id{0};
    std::string name;
    std::optional<std::string> email;
    std::vector<std::string> roles;
};

class PersonCell : public parcel::StructCell<PersonCell, Person, "person"> {
public:
    using StructCell::StructCell;

    [[maybe_unused]] static parcel::DisplayInfo display_info() {
        return {.name = "Person", .description = "A person record"};
    }

    [[maybe_unused]] static auto field_descriptors() {
        return parcel::FieldsBuilder<Person>{}
            .field<&Person::id>("id")
            .name("ID")
            .description("Stable, monotonic identifier")
            .field<&Person::name>("name")
            .name("Name")
            .field<&Person::email>("email")  // optional<string> -> not required
            .name("Email")
            .field<&Person::roles>("roles")  // vector<string>   -> TypedListCell<StringCell>
            .name("Roles")
            .build();
    }
};

void print_section(const std::string_view title) {
    std::cout << "\n--- " << title << " ---\n";
}

}  // namespace

int main() {
    parcel::ParcelRegistry registry;
    registry.register_kind(PersonCell::descriptor());

    print_section("Construction + to_string");
    const PersonCell alice = Person{
        .id = 1,
        .name = "Alice",
        .email = std::string{"alice@example.com"},
        .roles = {"admin", "ops"},
    };
    std::cout << "kind        = " << alice.kind() << '\n';
    std::cout << "to_string() = " << alice << '\n';

    print_section("to_json — full payload");
    std::cout << alice.to_json().dump(2) << '\n';

    print_section("Round-trip via registry");
    const auto wire = alice.to_json();
    const auto restored = registry.cell_from_json(wire);
    const auto* typed = dynamic_cast<PersonCell*>(restored.get());
    std::cout << "restored.kind() = " << restored->kind() << '\n';
    std::cout << "id    = " << typed->value.id << '\n';
    std::cout << "name  = " << typed->value.name << '\n';
    std::cout << "email = " << typed->value.email.value_or("<none>") << '\n';
    std::cout << "roles = ";
    for (auto const& r : typed->value.roles) {
        std::cout << r << ' ';
    }
    std::cout << '\n';

    print_section("Optional field omitted on the wire");
    const PersonCell bob = Person{.id = 2, .name = "Bob", .email = std::nullopt, .roles = {}};
    const auto bob_json = bob.to_json();
    std::cout << bob_json.dump(2) << '\n';
    std::cout << "v contains 'email'? " << std::boolalpha << bob_json["v"].contains("email")
              << '\n';

    print_section("descriptor()->to_json() — schema");
    const auto desc = PersonCell::descriptor();
    std::cout << desc->to_json().dump(2) << '\n';

    print_section("descriptor() exposes IHasFields");
    for (const auto* hf = dynamic_cast<parcel::IHasFields*>(desc.get());
         auto const& [key, fd] : hf->fields()) {
        std::cout << "  " << key << " : kind=" << fd->kind() << "  required=" << fd->is_required()
                  << '\n';
    }

    return 0;
}
