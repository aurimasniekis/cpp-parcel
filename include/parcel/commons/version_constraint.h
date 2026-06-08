#pragma once

/**
 * @file version_constraint.h
 * @brief `VersionConstraintCell` — parcel adapter for `comms::VersionConstraint`.
 */

#include <parcel/cell.h>
#include <parcel/defaults.h>
#include <parcel/descriptor.h>
#include <parcel/json.h>

#include <commons/json/version_constraint.hpp>
#include <commons/version_constraint.hpp>

#include <string>
#include <string_view>
#include <utility>

namespace parcel {

/**
 * @brief Cell wrapping `comms::VersionConstraint`. Wire kind `"version_constraint"`; value is the
 *        raw npm-style range string.
 *
 * Decoding a malformed range throws (commons' `VersionConstraint::parse` throws),
 * consistent with parcel's strict-deserialization policy.
 */
class VersionConstraintCell : public BaseCell<VersionConstraintCell, comms::VersionConstraint> {
    using base_t = BaseCell<VersionConstraintCell, comms::VersionConstraint>;

public:
    using base_t::base_t;
    using base_t::operator=;

    static constexpr std::string_view kind_id = "version_constraint";

    [[nodiscard]] std::string to_string() const override {
        return this->value.to_string();
    }

    static cell_t from_json(json_t const& j, ParcelRegistry const&) {
        auto v = base_t::cell_from_json<comms::VersionConstraint>(j, kind_id);
        auto cell = std::make_shared<VersionConstraintCell>(std::move(v));
        base_t::absorb_display_info(j, cell);
        return cell;
    }

    static cell_type_descriptor_t descriptor() {
        static const auto d = std::make_shared<SimpleCellTypeDescriptor<VersionConstraintCell>>(
            DisplayInfo{.name = "VersionConstraint",
                        .description = "Semver range constraint (range string on the wire)"});
        return d;
    }
};

}  // namespace parcel

PARCEL_DEFAULT_CELL(parcel::VersionConstraintCell);
