// Demo: std-type adapter cells from the new opt-in headers.
//   - <parcel/ext/chrono.h>     SystemTimePointCell, UnixMillisCell / TimestampMsCell,
//                               DurationMsCell, YmdCell
//   - <parcel/ext/filesystem.h> PathCell
//   - <parcel/unordered_map.h>  TypedHashMapCell<T>, HashMapCell
// Plus default_cell_for resolution and a struct field that uses
// std::filesystem::path inferred via PARCEL_DEFAULT_CELL.

#include <parcel/ext/chrono.h>
#include <parcel/ext/filesystem.h>
#include <parcel/parcel.h>
#include <parcel/unordered_map.h>

#include <chrono>
#include <filesystem>
#include <iostream>
#include <string>
#include <string_view>
#include <type_traits>

namespace {

struct PathPayload {
    std::filesystem::path location;
};

class PathPayloadCell : public parcel::StructCell<PathPayloadCell, PathPayload, "path_payload"> {
public:
    using StructCell::StructCell;
    [[maybe_unused]] static parcel::descriptor::MetaInfo meta_info() {
        return {.name = "PathPayload"};
    }
    [[maybe_unused]] static auto field_descriptors() {
        return parcel::FieldsBuilder<PathPayload>{}
            .field<&PathPayload::location>("location")
            .build();
    }
};

void print_section(const std::string_view title) {
    std::cout << "\n--- " << title << " ---\n";
}

// Compile-time proofs for the std-type default_cell_for mappings.
static_assert(
    std::is_same_v<parcel::default_cell_for_t<std::chrono::sys_time<std::chrono::milliseconds>>,
                   parcel::UnixMillisCell>);
static_assert(std::is_same_v<parcel::default_cell_for_t<std::filesystem::path>, parcel::PathCell>);

}  // namespace

int main() {
    using namespace std::chrono;

    print_section("SystemTimePointCell — sys_seconds <-> i64 epoch seconds");
    {
        constexpr sys_seconds tp{seconds{1'700'000'000}};
        parcel::SystemTimePointCell c{tp};
        std::cout << "to_string = " << c << '\n';
        std::cout << "to_json   = " << c.to_json().dump() << '\n';
    }

    print_section("UnixMillisCell — sys_time<milliseconds> <-> i64 epoch ms");
    {
        constexpr sys_time<milliseconds> tp{milliseconds{1'700'000'000'000}};
        parcel::UnixMillisCell c{tp};
        std::cout << "to_string = " << c << '\n';
        std::cout << "to_json   = " << c.to_json().dump() << '\n';
        std::cout << "TimestampMsCell is an alias of UnixMillisCell : " << std::boolalpha
                  << std::is_same_v<parcel::TimestampMsCell, parcel::UnixMillisCell> << '\n';
    }

    print_section("DurationMsCell — milliseconds <-> i64");
    {
        parcel::DurationMsCell c{milliseconds{12345}};
        std::cout << "to_string = " << c << '\n';
        std::cout << "to_json   = " << c.to_json().dump() << '\n';
    }

    print_section("YmdCell — year_month_day <-> ISO YYYY-MM-DD string");
    {
        constexpr year_month_day d{2026y, April, 27d};
        parcel::YmdCell c{d};
        std::cout << "to_string = " << c << '\n';
        std::cout << "to_json   = " << c.to_json().dump() << '\n';
    }

    print_section("PathCell — wire is path::generic_string() (forward-slash, portable)");
    {
        parcel::PathCell c{std::filesystem::path{"a/b/c"}};
        std::cout << "to_string = " << c << '\n';
        std::cout << "to_json   = " << c.to_json().dump() << '\n';
    }

    print_section("default_cell_for resolution (proven at compile time above)");
    std::cout << "sys_time<milliseconds>      -> UnixMillisCell\n";
    std::cout << "std::filesystem::path       -> PathCell\n";

    print_section("TypedHashMapCell — storage iteration unspecified, wire is sorted");
    {
        parcel::TypedHashMapCell<parcel::I32Cell> a;
        a["b"] = 2;
        a["a"] = 1;
        a["c"] = 3;

        parcel::TypedHashMapCell<parcel::I32Cell> b;
        b["c"] = 3;
        b["a"] = 1;
        b["b"] = 2;

        std::cout << "a.to_json = " << a.to_json().dump() << '\n';
        std::cout << "b.to_json = " << b.to_json().dump() << '\n';
        std::cout << "wire equal regardless of insertion order : " << std::boolalpha
                  << (a.to_json().dump() == b.to_json().dump()) << '\n';
        std::cout << "to_string a = " << a << '\n';
        std::cout << "to_string b = " << b << '\n';
    }

    print_section("HashMapCell — heterogeneous variant (opt-in: register descriptor)");
    {
        // <parcel/parcel.h> does not pull in <parcel/unordered_map.h>, so the
        // HashMapCell descriptor must be added explicitly.
        parcel::ParcelRegistry reg;
        reg.register_kind(parcel::HashMapCell::descriptor());

        parcel::HashMapCell m{
            {"x", parcel::cell(1)},
            {"y", parcel::cell(std::string("hi"))},
        };
        const auto j = m.to_json();
        std::cout << "to_json = " << j.dump() << '\n';
        const auto rebuilt = parcel::HashMapCell::from_json(j, reg);
        std::cout << "rebuilt size = " << dynamic_cast<parcel::HashMapCell*>(rebuilt.get())->size()
                  << '\n';
    }

    print_section("Struct field of std::filesystem::path inferred via PARCEL_DEFAULT_CELL");
    {
        parcel::ParcelRegistry reg;
        reg.register_kind(PathPayloadCell::descriptor());

        const auto src =
            PathPayloadCell::of(PathPayload{.location = std::filesystem::path{"a/b/c"}});
        const auto j = src->to_json();
        std::cout << "wire = " << j.dump() << '\n';
        const auto rebuilt = reg.cell_from_json(j);
        const auto* typed = dynamic_cast<PathPayloadCell*>(rebuilt.get());
        std::cout << "rebuilt.location = " << typed->value.location.generic_string() << '\n';
    }

    return 0;
}
