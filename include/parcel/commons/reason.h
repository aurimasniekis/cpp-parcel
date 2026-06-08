#pragma once

/**
 * @file reason.h
 * @brief `ReasonCell` / `FailureReasonCell` — parcel adapters for the polymorphic
 *        `comms::ReasonPtr` / `comms::FailureReasonPtr`.
 */

#include <parcel/cell.h>
#include <parcel/defaults.h>
#include <parcel/descriptor.h>
#include <parcel/json.h>

#include <commons/json/reason.hpp>
#include <commons/reason.hpp>

#include <compare>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

namespace parcel {

/**
 * @brief Cell wrapping `comms::ReasonPtr` (a `std::unique_ptr<comms::IReason>`).
 *        Wire kind `"reason"`; value is a `{"kind", …fields}` object.
 *
 * `ReasonPtr` is move-only, so this cell derives directly from `ICell` rather
 * than `BaseCell` (whose `clone()` copies the storage). The held reason
 * serializes through commons' `nlohmann::adl_serializer<comms::ReasonPtr>`;
 * decoding resolves the `"kind"` discriminator against
 * `comms::GlobalReasonRegistry`, throwing on an unknown kind.
 */
class ReasonCell : public ICell {
public:
    using storage_t = comms::ReasonPtr;

    static constexpr std::string_view kind_id = "reason";

    /** @brief Held reason (may be null). */
    comms::ReasonPtr value;

    ReasonCell() = default;
    explicit ReasonCell(comms::ReasonPtr v) : value(std::move(v)) {}

    /** @brief Construct a `shared_ptr<ReasonCell>` owning @p v. */
    static cell_t of(comms::ReasonPtr v) {
        return std::make_shared<ReasonCell>(std::move(v));
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
        auto c = std::make_shared<ReasonCell>(value ? value->clone() : comms::ReasonPtr{});
        c->display_info_ = display_info_;
        return c;
    }

    [[nodiscard]] std::partial_ordering compare(ICell const& other) const override {
        if (const auto k_cmp = this->kind() <=> other.kind(); k_cmp != 0) {
            return k_cmp;
        }
        const auto* o = dynamic_cast<ReasonCell const*>(&other);
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
        // Resolves "kind" against comms::GlobalReasonRegistry; unknown kind throws.
        auto cell = std::make_shared<ReasonCell>(it_value->get<comms::ReasonPtr>());
        if (const auto it_d = j.find(KEY_DESCRIPTION); it_d != j.end()) {
            cell->set_display_info(it_d->get<DisplayInfo>());
        }
        return cell;
    }

    static cell_type_descriptor_t descriptor() {
        static const auto d = std::make_shared<SimpleCellTypeDescriptor<ReasonCell>>(DisplayInfo{
            .name = "Reason",
            .description = "Polymorphic \"why\" envelope ({\"kind\", …fields} on the wire)"});
        return d;
    }

protected:
    void set_display_info(std::optional<DisplayInfo> m) override {
        display_info_ = std::move(m);
    }

private:
    std::optional<DisplayInfo> display_info_;
};

/**
 * @brief Cell wrapping `comms::FailureReasonPtr` (a `std::unique_ptr<comms::IFailureReason>`).
 *        Wire kind `"failure_reason"`; value is a `{"kind", …fields}` object.
 *
 * Like `ReasonCell`, but decoding resolves through commons'
 * `nlohmann::adl_serializer<comms::FailureReasonPtr>`, which additionally
 * verifies the resolved kind is an `IFailureReason` — a plain reason kind in a
 * failure slot throws.
 */
class FailureReasonCell : public ICell {
public:
    using storage_t = comms::FailureReasonPtr;

    static constexpr std::string_view kind_id = "failure_reason";

    /** @brief Held failure reason (may be null). */
    comms::FailureReasonPtr value;

    FailureReasonCell() = default;
    explicit FailureReasonCell(comms::FailureReasonPtr v) : value(std::move(v)) {}

    /** @brief Construct a `shared_ptr<FailureReasonCell>` owning @p v. */
    static cell_t of(comms::FailureReasonPtr v) {
        return std::make_shared<FailureReasonCell>(std::move(v));
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
        // IFailureReason::clone() returns a ReasonPtr whose dynamic type is the
        // failure subclass; recover the typed pointer (provably safe downcast).
        comms::FailureReasonPtr copy;
        if (value) {
            comms::IReason* raw = value->clone().release();
            // NOLINTNEXTLINE(cppcoreguidelines-pro-type-static-cast-downcast)
            copy = comms::FailureReasonPtr{static_cast<comms::IFailureReason*>(raw)};
        }
        auto c = std::make_shared<FailureReasonCell>(std::move(copy));
        c->display_info_ = display_info_;
        return c;
    }

    [[nodiscard]] std::partial_ordering compare(ICell const& other) const override {
        if (const auto k_cmp = this->kind() <=> other.kind(); k_cmp != 0) {
            return k_cmp;
        }
        const auto* o = dynamic_cast<FailureReasonCell const*>(&other);
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
        // Resolves "kind" against comms::GlobalReasonRegistry and verifies the
        // kind is an IFailureReason; unknown / non-failure kind throws.
        auto cell = std::make_shared<FailureReasonCell>(it_value->get<comms::FailureReasonPtr>());
        if (const auto it_d = j.find(KEY_DESCRIPTION); it_d != j.end()) {
            cell->set_display_info(it_d->get<DisplayInfo>());
        }
        return cell;
    }

    static cell_type_descriptor_t descriptor() {
        static const auto d =
            std::make_shared<SimpleCellTypeDescriptor<FailureReasonCell>>(DisplayInfo{
                .name = "FailureReason",
                .description =
                    "Polymorphic developer-controlled failure ({\"kind\", …fields} on the wire)"});
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

PARCEL_DEFAULT_CELL(parcel::ReasonCell);
PARCEL_DEFAULT_CELL(parcel::FailureReasonCell);
