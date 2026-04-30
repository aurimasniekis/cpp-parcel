// Demo: every string- or stream-shaped output path for a cell.
//   - std::format spec extensions: {}, {:#}, {:j}, {:j2}, {:k}
//   - std::ostream operator<< for ICell& and cell_t
//   - parcel::print / parcel::println (when std::print is available)
//   - cell_to_stream / cell_from_stream / cell_from_bytes
//   - try_cell_from_stream / try_cell_from_bytes (std::expected variants)

#include <parcel/parcel.h>

#include <cstddef>
#include <format>
#include <iostream>
#include <span>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

namespace {

void print_section(const std::string_view title) {
    std::cout << "\n--- " << title << " ---\n";
}

}  // namespace

int main() {
    parcel::ParcelRegistry registry;

    print_section("std::format specs on ICell");
    parcel::I32Cell c{42};
    std::cout << "{}    -> " << std::format("{}", c) << '\n';
    std::cout << "{:#}  -> " << std::format("{:#}", c) << '\n';
    std::cout << "{:j}  -> " << std::format("{:j}", c) << '\n';
    std::cout << "{:j2} ->\n" << std::format("{:j2}", c) << '\n';
    std::cout << "{:k}  -> " << std::format("{:k}", c) << '\n';

    print_section("std::format on cell_t (incl. null placeholder)");
    parcel::cell_t handle = parcel::I32Cell::of(42);
    std::cout << "{}   -> " << std::format("{}", handle) << '\n';
    std::cout << "{:k} -> " << std::format("{:k}", handle) << '\n';
    parcel::cell_t null_handle;
    std::cout << "{} on null -> " << std::format("{}", null_handle) << '\n';

    print_section("std::ostream operator<< for ICell& and cell_t");
    std::cout << "operator<<(ICell&)  : " << c << '\n';
    std::cout << "operator<<(cell_t)  : " << handle << '\n';
    std::cout << "operator<<(null)    : " << null_handle << '\n';

#if defined(__cpp_lib_print) && __cpp_lib_print >= 202207L
    print_section("parcel::print / parcel::println");
    parcel::print("print: i32 cell = {}\n", c);
    parcel::println("println: handle kind = {:k}", handle);
#else
    print_section("parcel::print / parcel::println — not available on this toolchain");
    std::cout << "std::print < C++23 — fall back to operator<< / std::format above\n";
#endif

    print_section("cell_to_stream — compact (default indent=-1)");
    {
        std::stringstream buf;
        parcel::cell_to_stream(buf, handle);
        std::cout << "compact: " << buf.str() << '\n';
    }

    print_section("cell_to_stream — pretty (indent=2)");
    {
        std::stringstream buf;
        parcel::cell_to_stream(buf, handle, 2);
        std::cout << "pretty:\n" << buf.str() << '\n';
    }

    print_section("cell_from_stream — round-trip");
    {
        std::stringstream buf;
        parcel::cell_to_stream(buf, handle);
        const auto rebuilt = parcel::cell_from_stream(buf, registry);
        std::cout << "rebuilt = " << rebuilt << "  (kind=" << rebuilt->kind() << ")\n";
    }

    print_section("cell_from_bytes — round-trip");
    {
        const auto json = handle->to_json().dump();
        std::vector<std::byte> bytes(json.size());
        std::memcpy(bytes.data(), json.data(), json.size());
        const auto rebuilt = parcel::cell_from_bytes(std::span<const std::byte>(bytes), registry);
        std::cout << "bytes.size = " << bytes.size() << "  rebuilt = " << rebuilt << '\n';
    }

#if PARCEL_HAS_EXPECTED
    print_section("try_cell_from_stream — happy path");
    {
        std::stringstream buf;
        parcel::cell_to_stream(buf, handle);
        if (auto r = parcel::try_cell_from_stream(buf, registry); r.has_value()) {
            std::cout << "ok: " << r.value() << '\n';
        }
    }

    print_section("try_cell_from_stream — malformed input -> error");
    {
        std::stringstream buf("{not json");
        if (auto r = parcel::try_cell_from_stream(buf, registry); !r.has_value()) {
            std::cout << "error: code=" << static_cast<int>(r.error().code)
                      << " message=" << r.error().message << '\n';
        }
    }

    print_section("try_cell_from_bytes — happy path");
    {
        const auto json = handle->to_json().dump();
        std::vector<std::byte> bytes(json.size());
        std::memcpy(bytes.data(), json.data(), json.size());
        if (auto r = parcel::try_cell_from_bytes(std::span<const std::byte>(bytes), registry);
            r.has_value()) {
            std::cout << "ok: " << r.value() << '\n';
        }
    }

    print_section("try_cell_from_bytes — malformed bytes -> error");
    {
        const std::string garbage = "not-json";
        std::vector<std::byte> bytes(garbage.size());
        std::memcpy(bytes.data(), garbage.data(), garbage.size());
        if (auto r = parcel::try_cell_from_bytes(std::span<const std::byte>(bytes), registry);
            !r.has_value()) {
            std::cout << "error: code=" << static_cast<int>(r.error().code)
                      << " message=" << r.error().message << '\n';
        }
    }
#else
    print_section("try_cell_from_stream / try_cell_from_bytes — not available (no std::expected)");
    std::cout << "fall back to the throwing overloads above\n";
#endif

    return 0;
}
