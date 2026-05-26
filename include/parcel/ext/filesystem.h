#pragma once

/**
 * @file ext/filesystem.h
 * @brief Cell adapter for `std::filesystem::path`.
 *
 * Wire shape: `{"k": "fs:path", "v": "<utf-8 generic path>"}`. The path is
 * encoded with `path::generic_string()` so the wire form is portable
 * across hosts (forward-slash separators, no native UNC or drive prefixes).
 */

#include <parcel/cell.h>
#include <parcel/defaults.h>
#include <parcel/descriptor.h>
#include <parcel/json.h>

#include <filesystem>
#include <memory>
#include <string>
#include <string_view>

namespace parcel {

/**
 * @brief Cell wrapping `std::filesystem::path` as a portable UTF-8 string.
 */
class PathCell : public BaseCell<PathCell, std::filesystem::path> {
    using base_t = BaseCell<PathCell, std::filesystem::path>;

public:
    using base_t::base_t;
    using base_t::operator=;

    static constexpr std::string_view kind_id = "fs:path";

    [[nodiscard]] std::string to_string() const override {
        return this->value.generic_string();
    }

    [[nodiscard]] json_t to_json() const override {
        json_t j{
            {ICell::KEY_KIND, kind_id},
            {ICell::KEY_VALUE, this->value.generic_string()},
        };
        this->inject_display_info(j);
        return j;
    }

    static cell_t from_json(json_t const& j, ParcelRegistry const&) {
        const auto s = base_t::cell_from_json<std::string>(j, kind_id);
        auto cell = std::make_shared<PathCell>(std::filesystem::path{s});
        base_t::absorb_display_info(j, cell);
        return cell;
    }

    static cell_type_descriptor_t descriptor() {
        static const auto d = std::make_shared<SimpleCellTypeDescriptor<PathCell>>(
            DisplayInfo{.name = "Path", .description = "Filesystem path (UTF-8 generic form)"});
        return d;
    }
};

}  // namespace parcel

PARCEL_DEFAULT_CELL(parcel::PathCell);
