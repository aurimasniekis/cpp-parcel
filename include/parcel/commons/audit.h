#pragma once

/**
 * @file audit.h
 * @brief Parcel adapters for the `comms::AuditRecord` family.
 *
 *   - `AuditRecordCell`        — `comms::AuditRecord`               → `"audit_record"`.
 *   - `AuditRecordsCell`       — `comms::AuditRecords`             → `"audit_records"`.
 *   - `ChangeAuditRecordCell<P>`  — `comms::ChangeAuditRecord<P::storage_t>`
 *     → `"change_audit_record:" + P::kind_id`.
 *   - `ChangeAuditRecordsCell<P>` — `comms::ChangeAuditRecords<P::storage_t>`
 *     → `"change_audit_records:" + P::kind_id`.
 *
 * The two non-templated cells are pre-registered in the default registry; the
 * templated `ChangeAuditRecord*` cells are opt-in per element cell `P` (register
 * via `ChangeAuditRecordCell<SomeCell>::descriptor()`), exactly like
 * `TypedListCell<P>`.
 */

#include <parcel/cell.h>
#include <parcel/common.h>
#include <parcel/defaults.h>
#include <parcel/descriptor.h>
#include <parcel/json.h>

#include <commons/audit_record.hpp>
#include <commons/identity.hpp>
#include <commons/json/audit_record.hpp>

#include <cstddef>
#include <string>
#include <string_view>

namespace parcel {

/**
 * @brief Cell wrapping `comms::AuditRecord`. Wire kind `"audit_record"`.
 *
 * `AuditRecord` is copyable and JSON-serializable, so this is an ordinary
 * `BaseCell`; the inherited `to_json` / `from_json_strict` route through
 * commons' ADL `to_json` / `from_json`.
 */
class AuditRecordCell : public BaseCell<AuditRecordCell, comms::AuditRecord> {
    using base_t = BaseCell<AuditRecordCell, comms::AuditRecord>;

public:
    using base_t::base_t;
    using base_t::operator=;

    static constexpr std::string_view kind_id = "audit_record";

    [[nodiscard]] std::string to_string() const override {
        return this->value.identity ? comms::to_string(*this->value.identity) : std::string{"—"};
    }

    static cell_t from_json(json_t const& j, ParcelRegistry const&) {
        return base_t::from_json_strict(j);
    }

    static cell_type_descriptor_t descriptor() {
        static const auto d = std::make_shared<SimpleCellTypeDescriptor<AuditRecordCell>>(
            DisplayInfo{.name = "AuditRecord",
                        .description = "Audit-trail entry (who did what, when, from where)"});
        return d;
    }
};

/**
 * @brief Cell wrapping `comms::AuditRecords` (= `comms::AuditLog<comms::AuditRecord>`).
 *        Wire kind `"audit_records"`.
 *
 * The log's capacity is a build constant and is **not** serialized; a
 * within-capacity log round-trips exactly.
 */
class AuditRecordsCell : public BaseCell<AuditRecordsCell, comms::AuditRecords> {
    using base_t = BaseCell<AuditRecordsCell, comms::AuditRecords>;

public:
    using base_t::base_t;
    using base_t::operator=;

    static constexpr std::string_view kind_id = "audit_records";

    [[nodiscard]] std::string to_string() const override {
        return "[" + std::to_string(this->value.size()) + " audit records]";
    }

    static cell_t from_json(json_t const& j, ParcelRegistry const&) {
        return base_t::from_json_strict(j);
    }

    static cell_type_descriptor_t descriptor() {
        static const auto d = std::make_shared<SimpleCellTypeDescriptor<AuditRecordsCell>>(
            DisplayInfo{.name = "AuditRecords",
                        .description = "Capped, insertion-ordered log of audit records"});
        return d;
    }
};

/**
 * @brief Cell wrapping `comms::ChangeAuditRecord<typename P::storage_t>`, the
 *        before/after change refinement of `AuditRecord`. Wire kind
 *        `"change_audit_record:" + P::kind_id`.
 *
 * @tparam P Element cell type whose storage is the changed value's type. `P`
 *           being a cell guarantees `P::storage_t` is JSON-serializable, which is
 *           what commons' templated `ChangeAuditRecord<T>` JSON hooks require.
 *
 * Opt-in: not auto-registered. Register one instantiation via
 * `ChangeAuditRecordCell<SomeCell>::descriptor()`, mirroring `TypedListCell<P>`.
 */
template <CellLike P>
class ChangeAuditRecordCell
    : public BaseCell<ChangeAuditRecordCell<P>, comms::ChangeAuditRecord<typename P::storage_t>> {
    using base_t = BaseCell<ChangeAuditRecordCell, comms::ChangeAuditRecord<typename P::storage_t>>;

public:
    using element_type = P;

    using base_t::base_t;
    using base_t::operator=;

    static constexpr std::string_view kind_id = prefixed_id_v<"change_audit_record:", P::kind_id>;

    [[nodiscard]] std::string to_string() const override {
        return this->value.identity ? comms::to_string(*this->value.identity) : std::string{"—"};
    }

    static cell_t from_json(json_t const& j, ParcelRegistry const&) {
        return base_t::from_json_strict(j);
    }

    static cell_type_descriptor_t descriptor() {
        static const auto d = std::make_shared<SimpleCellTypeDescriptor<ChangeAuditRecordCell>>(
            DisplayInfo{.name = "ChangeAuditRecord",
                        .description = std::string("Audit-trail change entry (before/after ") +
                                       std::string(P::kind_id) + ")"});
        return d;
    }
};

/**
 * @brief Cell wrapping `comms::ChangeAuditRecords<typename P::storage_t>` (=
 *        `comms::AuditLog<comms::ChangeAuditRecord<typename P::storage_t>>`).
 *        Wire kind `"change_audit_records:" + P::kind_id`.
 *
 * @tparam P Element cell type whose storage is the changed value's type.
 *
 * Opt-in: not auto-registered (see `ChangeAuditRecordCell`).
 */
template <CellLike P>
class ChangeAuditRecordsCell
    : public BaseCell<ChangeAuditRecordsCell<P>, comms::ChangeAuditRecords<typename P::storage_t>> {
    using base_t =
        BaseCell<ChangeAuditRecordsCell, comms::ChangeAuditRecords<typename P::storage_t>>;

public:
    using element_type = P;

    using base_t::base_t;
    using base_t::operator=;

    static constexpr std::string_view kind_id = prefixed_id_v<"change_audit_records:", P::kind_id>;

    [[nodiscard]] std::string to_string() const override {
        return "[" + std::to_string(this->value.size()) + " change audit records]";
    }

    static cell_t from_json(json_t const& j, ParcelRegistry const&) {
        return base_t::from_json_strict(j);
    }

    static cell_type_descriptor_t descriptor() {
        static const auto d =
            std::make_shared<SimpleCellTypeDescriptor<ChangeAuditRecordsCell>>(DisplayInfo{
                .name = "ChangeAuditRecords",
                .description = std::string("Capped log of change audit records (before/after ") +
                               std::string(P::kind_id) + ")"});
        return d;
    }
};

}  // namespace parcel

PARCEL_DEFAULT_CELL(parcel::AuditRecordCell);
PARCEL_DEFAULT_CELL(parcel::AuditRecordsCell);
