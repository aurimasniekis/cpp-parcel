#pragma once

/**
 * @file icon.h
 * @brief `IconCell` — parcel adapter for `comms::Icon`.
 */

#include <parcel/cell.h>
#include <parcel/defaults.h>
#include <parcel/descriptor.h>
#include <parcel/json.h>

#include <commons/icon.hpp>
#include <commons/json/icon.hpp>

#include <string>
#include <string_view>

namespace parcel {

/**
 * @brief Cell wrapping `comms::Icon`. Wire kind `"icon"`; value is a `"set:name"` string.
 */
class IconCell : public BaseCell<IconCell, comms::Icon> {
    using base_t = BaseCell<IconCell, comms::Icon>;

public:
    using base_t::base_t;
    using base_t::operator=;

    static constexpr std::string_view kind_id = "icon";

    [[nodiscard]] std::string to_string() const override {
        return this->value.to_string();
    }

    static cell_t from_json(json_t const& j, ParcelRegistry const&) {
        auto v = base_t::cell_from_json<comms::Icon>(j, kind_id);
        auto cell = std::make_shared<IconCell>(v);
        base_t::absorb_display_info(j, cell);
        return cell;
    }

    static cell_type_descriptor_t descriptor() {
        static const auto d = std::make_shared<SimpleCellTypeDescriptor<IconCell>>(DisplayInfo{
            .name = "Icon", .description = "Iconify icon identifier (set:name string)"});
        return d;
    }
};

}  // namespace parcel

PARCEL_DEFAULT_CELL(parcel::IconCell);
