#pragma once

/**
 * @file identity.h
 * @brief `IdentityCell` — parcel adapter for the polymorphic `comms::IdentityPtr`.
 */

#include <parcel/cell.h>
#include <parcel/defaults.h>
#include <parcel/descriptor.h>
#include <parcel/json.h>

#include <commons/identity.hpp>
#include <commons/json/identity.hpp>

#include <compare>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

namespace parcel {

/**
 * @brief Cell wrapping `comms::IdentityPtr` (a `std::unique_ptr<comms::IIdentity>`).
 *        Wire kind `"identity"`; value is a `{"kind", …fields}` object.
 *
 * `IdentityPtr` is move-only, so this cell derives directly from `ICell` rather
 * than `BaseCell` (whose `clone()` copies the storage). The held identity
 * serializes through commons' `nlohmann::adl_serializer<comms::IdentityPtr>`;
 * decoding resolves the `"kind"` discriminator against
 * `comms::GlobalIdentityRegistry`, throwing on an unknown kind. The held
 * abilities round-trip automatically via `adl_serializer<comms::AbilityPtr>`.
 */
class IdentityCell : public ICell {
public:
    using storage_t = comms::IdentityPtr;

    static constexpr std::string_view kind_id = "identity";

    /** @brief Held identity (may be null). */
    comms::IdentityPtr value;

    IdentityCell() = default;
    explicit IdentityCell(comms::IdentityPtr v) : value(std::move(v)) {}

    /** @brief Construct a `shared_ptr<IdentityCell>` owning @p v. */
    static cell_t of(comms::IdentityPtr v) {
        return std::make_shared<IdentityCell>(std::move(v));
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
        auto c = std::make_shared<IdentityCell>(value ? value->clone() : comms::IdentityPtr{});
        c->display_info_ = display_info_;
        return c;
    }

    [[nodiscard]] std::partial_ordering compare(ICell const& other) const override {
        if (const auto k_cmp = this->kind() <=> other.kind(); k_cmp != 0) {
            return k_cmp;
        }
        const auto* o = dynamic_cast<IdentityCell const*>(&other);
        if (o == nullptr) {
            return std::partial_ordering::unordered;
        }
        return json_t(value) == json_t(o->value) ? std::partial_ordering::equivalent
                                                 : std::partial_ordering::unordered;
    }

    static cell_t from_json(json_t const& j, ParcelRegistry const&) {
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
        // Resolves "kind" against comms::GlobalIdentityRegistry; unknown kind throws.
        auto cell = std::make_shared<IdentityCell>(it_value->get<comms::IdentityPtr>());
        if (const auto it_d = j.find(KEY_DESCRIPTION); it_d != j.end()) {
            cell->set_display_info(it_d->get<DisplayInfo>());
        }
        return cell;
    }

    static cell_type_descriptor_t descriptor() {
        static const auto d = std::make_shared<SimpleCellTypeDescriptor<IdentityCell>>(DisplayInfo{
            .name = "Identity",
            .description = "Polymorphic \"who\" envelope ({\"kind\", …fields} on the wire)"});
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

PARCEL_DEFAULT_CELL(parcel::IdentityCell);
