#pragma once

/**
 * @file semver.h
 * @brief `SemVerCell` — parcel adapter for `comms::SemVer`.
 */

#include <parcel/cell.h>
#include <parcel/defaults.h>
#include <parcel/descriptor.h>
#include <parcel/json.h>

#include <commons/json/semver.hpp>
#include <commons/semver.hpp>

#include <string>
#include <string_view>

namespace parcel {

/**
 * @brief Cell wrapping `comms::SemVer`. Wire kind `"semver"`; value is the canonical version
 * string.
 */
class SemVerCell : public BaseCell<SemVerCell, comms::SemVer> {
    using base_t = BaseCell<SemVerCell, comms::SemVer>;

public:
    using base_t::base_t;
    using base_t::operator=;

    static constexpr std::string_view kind_id = "semver";

    [[nodiscard]] std::string to_string() const override {
        return this->value.to_string();
    }

    static cell_t from_json(json_t const& j, ParcelRegistry const&) {
        auto v = base_t::cell_from_json<comms::SemVer>(j, kind_id);
        auto cell = std::make_shared<SemVerCell>(v);
        base_t::absorb_display_info(j, cell);
        return cell;
    }

    static cell_type_descriptor_t descriptor() {
        static const auto d = std::make_shared<SimpleCellTypeDescriptor<SemVerCell>>(DisplayInfo{
            .name = "SemVer", .description = "Semantic version (canonical string on the wire)"});
        return d;
    }
};

}  // namespace parcel

PARCEL_DEFAULT_CELL(parcel::SemVerCell);
