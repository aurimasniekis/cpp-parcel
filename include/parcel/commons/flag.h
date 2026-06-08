#pragma once

/**
 * @file flag.h
 * @brief `FlagCell` / `FlagSetCell` — parcel adapters for the `comms::Flag` family.
 *
 * Decoding a `FlagCell` / `FlagSetCell` resolves each flag name against
 * `comms::GlobalFlagRegistry`; an unregistered name throws (commons behavior).
 */

#include <parcel/cell.h>
#include <parcel/defaults.h>
#include <parcel/descriptor.h>
#include <parcel/json.h>

#include <commons/flag.hpp>
#include <commons/json/flag.hpp>

#include <string>
#include <string_view>
#include <utility>

namespace parcel {

/**
 * @brief Cell wrapping `comms::FlagRef`. Wire kind `"flag"`; value is the flag name.
 *
 * Decoding resolves the name against `comms::GlobalFlagRegistry`; an
 * unregistered name throws (commons behavior).
 */
class FlagCell : public BaseCell<FlagCell, comms::FlagRef> {
    using base_t = BaseCell<FlagCell, comms::FlagRef>;

public:
    using base_t::base_t;
    using base_t::operator=;

    static constexpr std::string_view kind_id = "flag";

    [[nodiscard]] std::string to_string() const override {
        return std::string{this->value.name};
    }

    static cell_t from_json(json_t const& j, ParcelRegistry const&) {
        auto v = base_t::cell_from_json<comms::FlagRef>(j, kind_id);
        auto cell = std::make_shared<FlagCell>(v);
        base_t::absorb_display_info(j, cell);
        return cell;
    }

    static cell_type_descriptor_t descriptor() {
        static const auto d = std::make_shared<SimpleCellTypeDescriptor<FlagCell>>(DisplayInfo{
            .name = "Flag", .description = "Reference to a registered flag (name on the wire)"});
        return d;
    }
};

/**
 * @brief Cell wrapping `comms::FlagSet`. Wire kind `"flag_set"`; value is an array of names.
 *
 * Decoding resolves each name against `comms::GlobalFlagRegistry`; an
 * unregistered name throws (commons behavior).
 */
class FlagSetCell : public BaseCell<FlagSetCell, comms::FlagSet> {
    using base_t = BaseCell<FlagSetCell, comms::FlagSet>;

public:
    using base_t::base_t;
    using base_t::operator=;

    static constexpr std::string_view kind_id = "flag_set";

    [[nodiscard]] std::string to_string() const override {
        std::string out = "[";
        bool first = true;
        for (auto const& f : this->value) {
            if (!first) {
                out += ", ";
            }
            out += std::string{f.name};
            first = false;
        }
        out += "]";
        return out;
    }

    static cell_t from_json(json_t const& j, ParcelRegistry const&) {
        auto v = base_t::cell_from_json<comms::FlagSet>(j, kind_id);
        auto cell = std::make_shared<FlagSetCell>(std::move(v));
        base_t::absorb_display_info(j, cell);
        return cell;
    }

    static cell_type_descriptor_t descriptor() {
        static const auto d = std::make_shared<SimpleCellTypeDescriptor<FlagSetCell>>(
            DisplayInfo{.name = "FlagSet",
                        .description = "Insertion-ordered set of flags (names on the wire)"});
        return d;
    }
};

}  // namespace parcel

PARCEL_DEFAULT_CELL(parcel::FlagCell);
PARCEL_DEFAULT_CELL(parcel::FlagSetCell);
