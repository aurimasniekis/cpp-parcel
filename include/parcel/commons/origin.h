#pragma once

/**
 * @file origin.h
 * @brief `OriginCell` — parcel adapter for the polymorphic `comms::OriginPtr`.
 */

#include <parcel/cell.h>
#include <parcel/defaults.h>
#include <parcel/descriptor.h>
#include <parcel/json.h>

#include <commons/json/origin.hpp>
#include <commons/origin.hpp>

#include <compare>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

namespace parcel {

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

PARCEL_DEFAULT_CELL(parcel::OriginCell);
