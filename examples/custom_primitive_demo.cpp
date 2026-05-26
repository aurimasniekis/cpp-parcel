// Recipe: define a brand-new primitive-style cell from scratch.
//
// Steps demonstrated below:
//   1. Define a user storage type (Uuid: a pair of u64 halves).
//   2. Give Storage a JSON round-trip via free to_json/from_json in its own
//      namespace (so nlohmann::json can encode and decode it).
//   3. Subclass parcel::BaseCell<UuidCell, Uuid>, declare kind_id, override
//      to_string(), inherit BaseCell's to_json() (works because Uuid is
//      JSON-convertible), and provide static from_json() and descriptor().
//   4. (Optional but recommended) Specialize parcel::default_cell_for<Uuid>
//      so FieldsBuilder can infer UuidCell when wrapping Uuid fields.
//   5. Register the kind, round-trip, and use it inside a StructCell.

#include <parcel/parcel.h>

#include <iomanip>
#include <iostream>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>

namespace demo {

// ----------------------------------------------------------------------------
// 1. Storage type.
// A 128-bit UUID stored as two 64-bit halves. Encoded on the wire as a
// canonical hex string ("hi-lo" without dashes for brevity).

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

// ----------------------------------------------------------------------------
// 2. JSON round-trip for the storage type, in the same namespace as Uuid so
// nlohmann::json discovers it via ADL.

inline void to_json(parcel::json_t& j, Uuid const& u) {
    j = u.to_hex();
}

inline void from_json(parcel::json_t const& j, Uuid& u) {
    if (!j.is_string()) {
        throw std::runtime_error("Uuid: expected JSON string");
    }
    u = Uuid::from_hex(j.get<std::string>());
}

// ----------------------------------------------------------------------------
// 3. The cell.

class UuidCell : public parcel::BaseCell<UuidCell, Uuid> {
    using base_t = BaseCell<UuidCell, Uuid>;

public:
    using base_t::base_t;
    using base_t::operator=;

    static constexpr std::string_view kind_id = "uuid";

    [[nodiscard]] std::string to_string() const override {
        return this->value.to_hex();
    }

    [[maybe_unused]] static parcel::cell_t from_json(parcel::json_t const& j,
                                                     parcel::ParcelRegistry const&) {
        auto v = cell_from_json<Uuid>(j, kind_id);
        auto cell = std::make_shared<UuidCell>(v);
        absorb_display_info(j, cell);
        return cell;
    }

    static parcel::cell_type_descriptor_t descriptor() {
        static const auto d =
            std::make_shared<parcel::SimpleCellTypeDescriptor<UuidCell>>(parcel::DisplayInfo{
                .name = "Uuid",
                .description = "128-bit unique identifier (hex-encoded on the wire)",
            });
        return d;
    }
};

}  // namespace demo

// ----------------------------------------------------------------------------
// 4. Inference hook — placed at namespace scope so FieldsBuilder can find
// default_cell_for<Uuid>::type without an explicit CellT template arg.

template <>
struct parcel::default_cell_for<demo::Uuid> {
    using type = demo::UuidCell;
};

// ----------------------------------------------------------------------------
// 5. Use it standalone, then inside a StructCell.

namespace demo {

struct Account {
    Uuid id;
    std::string name;
};

class AccountCell : public parcel::StructCell<AccountCell, Account, "account"> {
public:
    using StructCell::StructCell;

    [[maybe_unused]] static parcel::DisplayInfo display_info() {
        return {.name = "Account"};
    }

    // Note: no explicit CellT on the `id` field — picked up via
    // default_cell_for<Uuid>::type.
    [[maybe_unused]] static auto field_descriptors() {
        return parcel::FieldsBuilder<Account>{}
            .field<&Account::id>("id")
            .field<&Account::name>("name")
            .build();
    }
};

void print_section(const std::string_view title) {
    std::cout << "\n--- " << title << " ---\n";
}

}  // namespace demo

int main() {
    parcel::ParcelRegistry registry;
    registry.register_kind(demo::UuidCell::descriptor());
    registry.register_kind(demo::AccountCell::descriptor());

    demo::print_section("UuidCell standalone");
    const demo::UuidCell u = demo::Uuid{0x0123456789abcdefULL, 0xfedcba9876543210ULL};
    std::cout << "kind        = " << u.kind() << '\n';
    std::cout << "to_string() = " << u << '\n';
    std::cout << "to_json()   = " << u.to_json().dump() << '\n';

    demo::print_section("UuidCell round-trip via registry");
    const auto restored = registry.cell_from_json(u.to_json());
    std::cout << "restored.kind() = " << restored->kind() << '\n';
    std::cout << "restored        = " << restored << '\n';

    demo::print_section("UuidCell as a struct field (inferred)");
    const demo::AccountCell acct = demo::Account{
        .id = demo::Uuid{0xaaaabbbbccccddddULL, 0x1111222233334444ULL},
        .name = "ops",
    };
    std::cout << acct.to_json().dump(2) << '\n';

    demo::print_section("Round-trip the AccountCell");
    const auto restored_acct = registry.cell_from_json(acct.to_json());
    std::cout << "restored.kind() = " << restored_acct->kind() << '\n';
    std::cout << "restored        = " << restored_acct << '\n';

    return 0;
}
