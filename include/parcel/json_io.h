#pragma once

/**
 * @file json_io.h
 * @brief Stream / byte-span JSON I/O for cells.
 *
 * Adds thin facades on top of `ParcelRegistry::cell_from_json` and
 * `ICell::to_json` so callers don't have to do the `std::string`
 * round-trip themselves. The non-throwing `try_*` overloads are wired in
 * when `<expected>` is available.
 */

#include <parcel/cell.h>
#include <parcel/error.h>
#include <parcel/json.h>
#include <parcel/registry.h>

#include <cstddef>
#include <span>
#include <string>

#if PARCEL_HAS_EXPECTED
#include <expected>
#endif

namespace parcel {

/**
 * @brief Read one JSON document from @p is and dispatch via @p reg.
 * @param is  Input stream — read to EOF as a single JSON value.
 * @param reg Registry used to resolve the kind.
 * @return Newly built cell handle.
 * @throws std::runtime_error on parse failure or shape/kind mismatch.
 */
[[nodiscard]] inline cell_t cell_from_stream(std::istream& is, ParcelRegistry const& reg) {
    json_t j;
    is >> j;
    return reg.cell_from_json(j);
}

/**
 * @brief Read raw bytes as JSON and dispatch via @p reg.
 * @param bytes UTF-8 JSON bytes.
 * @param reg   Registry used to resolve the kind.
 * @throws std::runtime_error on parse failure or shape/kind mismatch.
 */
[[nodiscard]] inline cell_t cell_from_bytes(const std::span<const std::byte> bytes,
                                            ParcelRegistry const& reg) {
    // NOLINTBEGIN(cppcoreguidelines-pro-type-reinterpret-cast) — std::byte → const char* is the
    // canonical conversion at this boundary
    const auto j = json_t::parse(reinterpret_cast<const char*>(bytes.data()),
                                 reinterpret_cast<const char*>(bytes.data() + bytes.size()));
    // NOLINTEND(cppcoreguidelines-pro-type-reinterpret-cast)
    return reg.cell_from_json(j);
}

/**
 * @brief Render @p c as JSON onto @p os.
 * @param os     Output stream.
 * @param c      Cell to dump (null → JSON `null`).
 * @param indent `-1` for compact (default); non-negative for pretty.
 */
inline void cell_to_stream(std::ostream& os, cell_t const& c, const int indent = -1) {
    if (!c) {
        os << json_t().dump(indent);
        return;
    }
    os << c->to_json().dump(indent);
}

#if PARCEL_HAS_EXPECTED
/** @brief Non-throwing variant of `cell_from_stream`. */
[[nodiscard]] inline std::expected<cell_t, ParcelError>
try_cell_from_stream(std::istream& is, ParcelRegistry const& reg) {
    json_t j;
    try {
        is >> j;
    } catch (std::exception const& e) {
        return std::unexpected(ParcelError::make(ParcelError::Code::InvalidJson, e.what()));
    } catch (...) {
        return std::unexpected(
            ParcelError::make(ParcelError::Code::InvalidJson, "stream parse failed"));
    }
    return reg.try_cell_from_json(j);
}

/** @brief Non-throwing variant of `cell_from_bytes`. */
[[nodiscard]] inline std::expected<cell_t, ParcelError>
try_cell_from_bytes(const std::span<const std::byte> bytes, ParcelRegistry const& reg) {
    json_t j;
    try {
        // NOLINTBEGIN(cppcoreguidelines-pro-type-reinterpret-cast) — std::byte → const char* is the
        // canonical conversion at this boundary
        j = json_t::parse(reinterpret_cast<const char*>(bytes.data()),
                          reinterpret_cast<const char*>(bytes.data() + bytes.size()));
        // NOLINTEND(cppcoreguidelines-pro-type-reinterpret-cast)
    } catch (std::exception const& e) {
        return std::unexpected(ParcelError::make(ParcelError::Code::InvalidJson, e.what()));
    } catch (...) {
        return std::unexpected(
            ParcelError::make(ParcelError::Code::InvalidJson, "bytes parse failed"));
    }
    return reg.try_cell_from_json(j);
}
#endif

}  // namespace parcel
