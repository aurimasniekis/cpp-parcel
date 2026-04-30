// Demo: default_cell_for<T> drives FieldsBuilder field-type inference. With it,
// .field<&P::m>("k") needs no explicit CellT for any T parcel knows how to
// wrap: primitives, std::optional<T>, std::vector<T>, std::map<std::string,T>,
// std::variant<Ts...>, and any user T you specialize the trait for.

#include <parcel/parcel.h>

#include <iostream>
#include <map>
#include <optional>
#include <string>
#include <type_traits>
#include <variant>
#include <vector>

namespace {

// Compile-time proofs — these static_asserts are the documentation.
static_assert(std::is_same_v<parcel::default_cell_for_t<std::int32_t>, parcel::I32Cell>);
static_assert(std::is_same_v<parcel::default_cell_for_t<std::string>, parcel::StringCell>);
static_assert(std::is_same_v<parcel::default_cell_for_t<std::vector<std::int32_t>>,
                             parcel::TypedListCell<parcel::I32Cell>>);
static_assert(std::is_same_v<parcel::default_cell_for_t<std::map<std::string, std::string>>,
                             parcel::TypedMapCell<parcel::StringCell>>);
static_assert(std::is_same_v<parcel::default_cell_for_t<std::optional<std::vector<std::int32_t>>>,
                             parcel::TypedListCell<parcel::I32Cell>>);
static_assert(std::is_same_v<parcel::default_cell_for_t<std::variant<std::int32_t, std::string>>,
                             parcel::UnionCell<parcel::I32Cell, parcel::StringCell>>);

struct Settings {
    std::string host;
    std::optional<std::int32_t> port;
    std::vector<std::string> allow_list;
    std::map<std::string, std::string> env;
    std::variant<std::int32_t, std::string> handle;
};

class SettingsCell : public parcel::StructCell<SettingsCell, Settings, "settings"> {
public:
    using StructCell::StructCell;

    [[maybe_unused]] static parcel::descriptor::MetaInfo meta_info() {
        return {.name = "Settings"};
    }

    // Every field uses the inference overload — no explicit CellT.
    [[maybe_unused]] static auto field_descriptors() {
        return parcel::FieldsBuilder<Settings>{}
            .field<&Settings::host>("host")
            .field<&Settings::port>("port")  // optional<i32> -> not required
            .field<&Settings::allow_list>(
                "allow_list")              // vector<string> -> TypedListCell<StringCell>
            .field<&Settings::env>("env")  // map<string,string> -> TypedMapCell<StringCell>
            .field<&Settings::handle>(
                "handle")  // variant<i32,string> -> UnionCell<I32Cell,StringCell>
            .build();
    }
};

void print_section(const std::string_view title) {
    std::cout << "\n--- " << title << " ---\n";
}

}  // namespace

int main() {
    parcel::ParcelRegistry registry;
    // Primitive typed lists/maps (incl. StringListCell, StringMapCell) are
    // pre-registered as builtins. Only user-driven kinds remain.
    registry.register_kind(parcel::UnionCell<parcel::I32Cell, parcel::StringCell>::descriptor());
    registry.register_kind(SettingsCell::descriptor());

    print_section("Build a payload using only inferred field cells");
    const SettingsCell s = Settings{
        .host = "localhost",
        .port = 8080,
        .allow_list = {"alice", "bob"},
        .env = {{"DEBUG", "1"}, {"REGION", "eu"}},
        .handle = std::string{"primary"},
    };

    print_section("Wire payload");
    std::cout << s.to_json().dump(2) << '\n';

    print_section("Inferred field kinds (from descriptor)");
    for (const auto j = SettingsCell::descriptor()->to_json(); auto const& f : j["fields"]) {
        std::cout << "  " << f["key"].get<std::string>()
                  << " : kind=" << f["kind"].get<std::string>()
                  << "  required=" << f["required"].get<bool>() << '\n';
    }

    print_section("Round-trip");
    const auto restored = registry.cell_from_json(s.to_json());
    auto* typed = dynamic_cast<SettingsCell*>(restored.get());
    std::cout << "host       = " << typed->value.host << '\n';
    std::cout << "port       = " << (typed->value.port ? *typed->value.port : -1) << '\n';
    std::cout << "allow_list = ";
    for (auto const& a : typed->value.allow_list) {
        std::cout << a << ' ';
    }
    std::cout << "\nenv        = ";
    for (auto const& [k, v] : typed->value.env) {
        std::cout << k << '=' << v << ' ';
    }
    std::cout << "\nhandle     = " << std::get<std::string>(typed->value.handle) << '\n';

    return 0;
}
