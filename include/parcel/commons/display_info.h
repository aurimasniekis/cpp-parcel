#pragma once

/**
 * @file display_info.h
 * @brief `DisplayInfoCell` — parcel adapter for `comms::DisplayInfo`.
 */

#include <parcel/cell.h>
#include <parcel/defaults.h>
#include <parcel/descriptor.h>
#include <parcel/json.h>

#include <commons/display_info.hpp>
#include <commons/json/display_info.hpp>

#include <string>
#include <string_view>
#include <utility>

namespace parcel {

/**
 * @brief Cell wrapping `comms::DisplayInfo`. Wire kind `"display_info"`; value is a JSON object.
 */
class DisplayInfoCell : public BaseCell<DisplayInfoCell, comms::DisplayInfo> {
    using base_t = BaseCell<DisplayInfoCell, comms::DisplayInfo>;

public:
    using base_t::base_t;
    using base_t::operator=;

    static constexpr std::string_view kind_id = "display_info";

    [[nodiscard]] std::string to_string() const override {
        return json_t(this->value).dump();
    }

    static cell_t from_json(json_t const& j, ParcelRegistry const&) {
        auto v = base_t::cell_from_json<comms::DisplayInfo>(j, kind_id);
        auto cell = std::make_shared<DisplayInfoCell>(std::move(v));
        base_t::absorb_display_info(j, cell);
        return cell;
    }

    static cell_type_descriptor_t descriptor() {
        static const auto d = std::make_shared<SimpleCellTypeDescriptor<DisplayInfoCell>>(
            DisplayInfo{.name = "DisplayInfo",
                        .description = "Presentation display info (name/description/icon/color)"});
        return d;
    }
};

}  // namespace parcel

PARCEL_DEFAULT_CELL(parcel::DisplayInfoCell);
