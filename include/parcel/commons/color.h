#pragma once

/**
 * @file color.h
 * @brief `ColorCell` — parcel adapter for `comms::Color`.
 */

#include <parcel/cell.h>
#include <parcel/defaults.h>
#include <parcel/descriptor.h>
#include <parcel/json.h>

#include <commons/color.hpp>
#include <commons/json/color.hpp>

#include <string>
#include <string_view>

namespace parcel {

/**
 * @brief Cell wrapping `comms::Color`. Wire kind `"color"`; value is a hex string.
 */
class ColorCell : public BaseCell<ColorCell, comms::Color> {
    using base_t = BaseCell<ColorCell, comms::Color>;

public:
    using base_t::base_t;
    using base_t::operator=;

    static constexpr std::string_view kind_id = "color";

    [[nodiscard]] std::string to_string() const override {
        return this->value.to_hex_string();
    }

    static cell_t from_json(json_t const& j, ParcelRegistry const&) {
        auto v = base_t::cell_from_json<comms::Color>(j, kind_id);
        auto cell = std::make_shared<ColorCell>(v);
        base_t::absorb_display_info(j, cell);
        return cell;
    }

    static cell_type_descriptor_t descriptor() {
        static const auto d = std::make_shared<SimpleCellTypeDescriptor<ColorCell>>(
            DisplayInfo{.name = "Color", .description = "RGBA color (hex string on the wire)"});
        return d;
    }
};

}  // namespace parcel

PARCEL_DEFAULT_CELL(parcel::ColorCell);
