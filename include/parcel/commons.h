#pragma once

/**
 * @file commons.h
 * @brief Parcel cell adapters for the `aurimasniekis/cpp-commons` vocabulary types.
 *
 * Carries commons' `Color`, `Icon`, `DisplayInfo`, flag, and versioning /
 * provenance types on the wire:
 *   - `ColorCell`             — `comms::Color`             → `"color"` (hex string).
 *   - `IconCell`              — `comms::Icon`              → `"icon"` (`"set:name"` string).
 *   - `DisplayInfoCell`       — `comms::DisplayInfo`       → `"display_info"` (JSON object).
 *   - `FlagCell`              — `comms::FlagRef`           → `"flag"` (flag name string).
 *   - `FlagSetCell`           — `comms::FlagSet`           → `"flag_set"` (array of names).
 *   - `SemVerCell`            — `comms::SemVer`            → `"semver"` (version string).
 *   - `VersionConstraintCell` — `comms::VersionConstraint` → `"version_constraint"` (range string).
 *   - `OriginCell`            — `comms::OriginPtr`         → `"origin"` (`{"kind",…}` object).
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
#include <commons/origin.hpp>
#include <commons/semver.hpp>
#include <commons/version_constraint.hpp>

#include <compare>
#include <memory>
#include <optional>
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

/**
 * @brief Cell wrapping `comms::OriginPtr` (a `std::unique_ptr<comms::IOrigin>`).
 *        Wire kind `"origin"`; value is a `{"kind", …fields}` object.
 *
 * `OriginPtr` is move-only, so this cell derives directly from `ICell` rather
 * than `BaseCell` (whose `clone()` copies the storage). The held origin
 * serializes through commons' `nlohmann::adl_serializer<comms::OriginPtr>`;
 * decoding resolves the `"kind"` discriminator against
 * `comms::GlobalOriginRegistry`, throwing on an unknown kind.
 */
class OriginCell : public ICell {
public:
    using storage_t = comms::OriginPtr;

    static constexpr std::string_view kind_id = "origin";

    /** @brief Held origin (may be null). */
    comms::OriginPtr value;

    OriginCell() = default;
    explicit OriginCell(comms::OriginPtr v) : value(std::move(v)) {}

    /** @brief Construct a `shared_ptr<OriginCell>` owning @p v. */
    static cell_t of(comms::OriginPtr v) {
        return std::make_shared<OriginCell>(std::move(v));
    }

    [[nodiscard]] std::string_view kind() const override {
        return kind_id;
    }

    [[nodiscard]] std::optional<DisplayInfo> const& overridden_display_info() const override {
        return display_info_;
    }

    [[nodiscard]] std::string to_string() const override {
        return value ? std::string{value->kind()} : std::string{"null"};
    }

    [[nodiscard]] json_t to_json() const override {
        json_t j{{KEY_KIND, kind_id}, {KEY_VALUE, value}};
        if (display_info_) {
            j[KEY_DESCRIPTION] = *display_info_;
        }
        return j;
    }

    [[nodiscard]] cell_t clone() const override {
        auto c = std::make_shared<OriginCell>(value ? value->clone() : comms::OriginPtr{});
        c->display_info_ = display_info_;
        return c;
    }

    [[nodiscard]] std::partial_ordering compare(ICell const& other) const override {
        if (const auto k_cmp = this->kind() <=> other.kind(); k_cmp != 0) {
            return k_cmp;
        }
        const auto* o = dynamic_cast<OriginCell const*>(&other);
        if (o == nullptr) {
            return std::partial_ordering::unordered;
        }
        return json_t(value) == json_t(o->value) ? std::partial_ordering::equivalent
                                                 : std::partial_ordering::unordered;
    }

    static cell_t from_json(json_t const& j, ParcelRegistry const&) {
        // Mirror BaseCell::cell_from_json's {"k","v"} validation; OriginPtr is
        // move-only so we cannot route through the BaseCell helper.
        if (!j.is_object()) {
            throw InvalidJsonException("Expected JSON object for ICell", std::string(kind_id));
        }
        const auto it_kind = j.find(KEY_KIND);
        if (it_kind == j.end() || !it_kind->is_string()) {
            throw InvalidJsonException("Missing or invalid 'k' field in ICell JSON",
                                       std::string(kind_id));
        }
        if (const auto k = it_kind->get<std::string_view>(); k != kind_id) {
            throw KindMismatchException("Unexpected kind in ICell JSON: expected '" +
                                            std::string(kind_id) + "', got '" + std::string(k) +
                                            "'",
                                        std::string(kind_id));
        }
        const auto it_value = j.find(KEY_VALUE);
        if (it_value == j.end()) {
            throw InvalidJsonException("Missing 'v' field in ICell JSON", std::string(kind_id));
        }
        // Resolves "kind" against comms::GlobalOriginRegistry; unknown kind throws.
        auto cell = std::make_shared<OriginCell>(it_value->get<comms::OriginPtr>());
        if (const auto it_d = j.find(KEY_DESCRIPTION); it_d != j.end()) {
            cell->set_display_info(it_d->get<DisplayInfo>());
        }
        return cell;
    }

    static cell_type_descriptor_t descriptor() {
        static const auto d = std::make_shared<SimpleCellTypeDescriptor<OriginCell>>(DisplayInfo{
            .name = "Origin",
            .description = "Polymorphic provenance envelope ({\"kind\", …fields} on the wire)"});
        return d;
    }

protected:
    void set_display_info(std::optional<DisplayInfo> m) override {
        display_info_ = std::move(m);
    }

private:
    std::optional<DisplayInfo> display_info_;
};

}  // namespace parcel

PARCEL_DEFAULT_CELL(parcel::ColorCell);
PARCEL_DEFAULT_CELL(parcel::IconCell);
PARCEL_DEFAULT_CELL(parcel::DisplayInfoCell);
PARCEL_DEFAULT_CELL(parcel::FlagCell);
PARCEL_DEFAULT_CELL(parcel::FlagSetCell);
PARCEL_DEFAULT_CELL(parcel::SemVerCell);
PARCEL_DEFAULT_CELL(parcel::VersionConstraintCell);
PARCEL_DEFAULT_CELL(parcel::OriginCell);
