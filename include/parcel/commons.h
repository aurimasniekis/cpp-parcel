#pragma once

/**
 * @file commons.h
 * @brief Parcel cell adapters for the `aurimasniekis/cpp-commons` vocabulary types.
 *
 * Carries commons' `Color`, `Icon`, `DisplayInfo`, and flag types on the wire:
 *   - `ColorCell`       — `comms::Color`,       wire kind `"color"`        (hex string).
 *   - `IconCell`        — `comms::Icon`,        wire kind `"icon"`         (`"set:name"` string).
 *   - `DisplayInfoCell` — `comms::DisplayInfo`, wire kind `"display_info"` (JSON object).
 *   - `FlagCell`        — `comms::FlagRef`,     wire kind `"flag"`         (flag name string).
 *   - `FlagSetCell`     — `comms::FlagSet`,     wire kind `"flag_set"`     (array of names).
 *
 * Each cell reuses commons' ADL `to_json` / `from_json` (from `commons/json.hpp`,
 * auto-enabled because nlohmann/json is on the include path), so the inherited
 * `BaseCell::to_json` / `cell_from_json` just work — only `to_string`,
 * `kind_id`, `from_json`, and `descriptor()` are defined here, following the
 * `ext/filesystem.h` `PathCell` pattern.
 *
 * Decoding a `FlagCell` / `FlagSetCell` resolves each flag name against
 * `comms::GlobalFlagRegistry`; an unregistered name throws (commons behavior).
 */

#include <parcel/cell.h>
#include <parcel/defaults.h>
#include <parcel/descriptor.h>
#include <parcel/json.h>

#include <commons/color.hpp>
#include <commons/display_info.hpp>
#include <commons/flag.hpp>
#include <commons/icon.hpp>
#include <commons/json.hpp>

#include <memory>
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

PARCEL_DEFAULT_CELL(parcel::ColorCell);
PARCEL_DEFAULT_CELL(parcel::IconCell);
PARCEL_DEFAULT_CELL(parcel::DisplayInfoCell);
PARCEL_DEFAULT_CELL(parcel::FlagCell);
PARCEL_DEFAULT_CELL(parcel::FlagSetCell);
