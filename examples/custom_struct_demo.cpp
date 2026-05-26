// Recipe: define a brand-new struct-style cell from scratch.
//
// Steps:
//   1. Define the payload as a plain C++ struct.
//   2. Subclass parcel::StructCell<MyCell, MyPayload, "my_id"> — the bare
//      struct id goes in the template arg list; "s:" is added by the framework.
//   3. Provide static display_info() and static field_descriptors() built with
//      FieldsBuilder.
//   4. Register the cell next to every kind it references — define() will
//      surface anything missing with a clear error.
//
// This example reuses the UuidCell built in custom_primitive_demo.cpp to show
// how custom primitives plug into custom structs.

#include <parcel/parcel.h>

#include <iomanip>
#include <iostream>
#include <memory>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace demo {

// -- UuidCell (copied from custom_primitive_demo to keep this file standalone) -

struct Uuid {
    std::uint64_t hi{0};
    std::uint64_t lo{0};

    bool operator==(Uuid const&) const = default;

    [[nodiscard]] std::string to_hex() const {
        std::ostringstream os;
        os << std::hex << std::setfill('0') << std::setw(16) << hi << std::setw(16) << lo;
        return os.str();
    }

    static Uuid from_hex(const std::string_view s) {
        if (s.size() != 32) {
            throw std::runtime_error("Uuid::from_hex: expected 32 hex chars");
        }
        const auto parse_half = [](const std::string_view part) {
            std::uint64_t out = 0;
            for (const char c : part) {
                std::uint64_t digit = 0;
                if (c >= '0' && c <= '9') {
                    digit = static_cast<std::uint64_t>(c - '0');
                } else if (c >= 'a' && c <= 'f') {
                    digit = static_cast<std::uint64_t>(c - 'a') + 10U;
                } else if (c >= 'A' && c <= 'F') {
                    digit = static_cast<std::uint64_t>(c - 'A') + 10U;
                } else {
                    throw std::runtime_error("Uuid::from_hex: non-hex character");
                }
                out = (out << 4) | digit;
            }
            return out;
        };
        return Uuid{parse_half(s.substr(0, 16)), parse_half(s.substr(16, 16))};
    }
};

inline void to_json(parcel::json_t& j, Uuid const& u) {
    j = u.to_hex();
}

inline void from_json(parcel::json_t const& j, Uuid& u) {
    if (!j.is_string()) {
        throw std::runtime_error("Uuid: expected JSON string");
    }
    u = Uuid::from_hex(j.get<std::string>());
}

class UuidCell : public parcel::BaseCell<UuidCell, Uuid> {
    using base_t = parcel::BaseCell<UuidCell, Uuid>;

public:
    using base_t::base_t;
    using base_t::operator=;

    static constexpr std::string_view kind_id = "uuid";

    [[nodiscard]] std::string to_string() const override {
        return this->value.to_hex();
    }

    [[maybe_unused]] static parcel::cell_t from_json(parcel::json_t const& j,
                                                     parcel::ParcelRegistry const&) {
        auto v = base_t::cell_from_json<Uuid>(j, kind_id);
        auto cell = std::make_shared<UuidCell>(v);
        base_t::absorb_display_info(j, cell);
        return cell;
    }

    static parcel::cell_type_descriptor_t descriptor() {
        static const auto d = std::make_shared<parcel::SimpleCellTypeDescriptor<UuidCell>>(
            parcel::DisplayInfo{.name = "Uuid"});
        return d;
    }
};

}  // namespace demo

// Register UuidCell as the default wrapper for Uuid so FieldsBuilder can
// infer the wrapper from the field type alone — no explicit CellT needed
// at the field declaration. The macro expands to the equivalent
// `default_cell_for<demo::Uuid>` specialization.
PARCEL_DEFAULT_CELL(demo::UuidCell);

// ----------------------------------------------------------------------------
// 1. Payload — plain data. Field types are anything FieldsBuilder can wrap:
// primitives, std::optional<T>, std::vector<T>, std::map<std::string,T>,
// std::variant<...>, or any T with a default_cell_for specialization
// (which PARCEL_DEFAULT_CELL adds in one line).

namespace demo {

struct Document {
    Uuid id;
    std::string title;
    std::optional<std::string> summary;
    std::vector<Uuid> references;
};

// 2. The cell.

class DocumentCell : public parcel::StructCell<DocumentCell, Document, "document"> {
public:
    using StructCell::StructCell;

    [[maybe_unused]] static parcel::DisplayInfo display_info() {
        return {.name = "Document", .description = "An indexed document"};
    }

    // 3. Field descriptors — fully inferred. `references` is std::vector<Uuid>;
    // default_cell_for<vector<Uuid>>::type resolves to TypedListCell<UuidCell>.
    [[maybe_unused]] static auto field_descriptors() {
        return parcel::FieldsBuilder<Document>{}
            .field<&Document::id>("id")
            .name("ID")
            .field<&Document::title>("title")
            .name("Title")
            .field<&Document::summary>("summary")  // optional<string> -> not required
            .name("Summary")
            .field<&Document::references>("references")  // vector<Uuid> -> TypedListCell<UuidCell>
            .name("References")
            .build();
    }
};

void print_section(const std::string_view title) {
    std::cout << "\n--- " << title << " ---\n";
}

}  // namespace demo

int main() {
    // 4. Register every user kind the document references — builtins are
    // pre-registered by the default ctor.
    parcel::ParcelRegistry registry;
    registry.register_kind(demo::UuidCell::descriptor());
    registry.register_kind(parcel::TypedListCell<demo::UuidCell>::descriptor());
    registry.register_kind(demo::DocumentCell::descriptor());

    demo::print_section("Build a Document");
    const demo::DocumentCell doc = demo::Document{
        .id = demo::Uuid{0x1, 0x2},
        .title = "Architecture Notes",
        .summary = std::string{"Design choices for the wire format."},
        .references =
            {
                demo::Uuid{0xaaaa, 0xbbbb},
                demo::Uuid{0xcccc, 0xdddd},
            },
    };
    std::cout << "to_string() = " << doc << '\n';

    demo::print_section("to_json — wire payload");
    std::cout << doc.to_json().dump(2) << '\n';

    demo::print_section("Round-trip");
    const auto restored = registry.cell_from_json(doc.to_json());
    const auto* typed = dynamic_cast<demo::DocumentCell*>(restored.get());
    std::cout << "title           = " << typed->value.title << '\n';
    std::cout << "summary         = " << typed->value.summary.value_or("<none>") << '\n';
    std::cout << "references.size = " << typed->value.references.size() << '\n';
    for (auto const& r : typed->value.references) {
        std::cout << "  ref = " << r.to_hex() << '\n';
    }

    demo::print_section("Schema graph via define('document')");
    std::cout << registry.define("s:document").to_json().dump(2) << '\n';

    return 0;
}
