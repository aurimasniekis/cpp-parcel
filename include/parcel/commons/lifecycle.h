#pragma once

/**
 * @file lifecycle.h
 * @brief Parcel adapters for the `comms::Lifecycle` family (status history).
 *
 *   - `LifecycleStatusCell`           — `comms::LifecycleStatus`     → `"lifecycle_status"`.
 *   - `StatusTransitionCell<P>`       — `comms::StatusTransition<P::storage_t>`
 *     → `"status_transition:" + P::kind_id`.
 *   - `StatusReportCell<P>`           — `comms::StatusReport<P::storage_t>`
 *     → `"status_report:" + P::kind_id`.
 *   - `StatusTransitionTimelineCell<P>` — the transition history of a
 *     `comms::StatusTransitionTimeline<P::storage_t>`
 *     → `"status_transition_timeline:" + P::kind_id`.
 *
 * The lifecycle vocabulary types are *generic* on a status `T` (not a
 * polymorphic open set). `T` defaults to the built-in `comms::LifecycleStatus`,
 * and so do these cells' template parameter `P` (which defaults to
 * `LifecycleStatusCell`). The `P = LifecycleStatusCell` instantiations —
 * `StatusTransitionCell<>` etc. — are pre-registered in the default registry;
 * any other status cell `P` is **opt-in**, registered per element cell via
 * `StatusTransitionCell<MyStatusCell>::descriptor()`, exactly like
 * `TypedListCell<P>` / `ChangeAuditRecordCell<P>`.
 *
 * The status `T = P::storage_t` must satisfy `comms::StatusType` (default-
 * constructible, copyable, equality-comparable — any cell storage does) and be
 * JSON-serializable (so the inherited `to_json` / `from_json` route through
 * commons' templated hooks). Statuses are rendered for `to_string()` through the
 * element cell `P`, so a custom status cell's own `to_string()` is reused.
 *
 * `comms::StatusTransitionTimeline<T>` owns a `std::mutex`, so it is
 * **non-copyable and non-movable** and cannot itself be a cell's storage (a cell
 * must `clone()`). `StatusTransitionTimelineCell<P>` therefore carries the
 * timeline's serializable projection — its `std::vector<StatusTransition<T>>`
 * history, which is exactly what commons encodes on the wire (a JSON array of
 * transitions). Bridge to/from a live timeline with the constructor that takes a
 * `StatusTransitionTimeline` and `load_into()` (subscribers are transient and
 * never serialized, matching commons).
 *
 * Timestamps and durations encode as epoch / count milliseconds (like
 * `audit.h`), so sub-millisecond `system_clock` ticks are truncated; use
 * ms-aligned values when an exact round-trip matters.
 */

#include <parcel/cell.h>
#include <parcel/common.h>
#include <parcel/defaults.h>
#include <parcel/descriptor.h>
#include <parcel/json.h>

#include <commons/json/lifecycle.hpp>
#include <commons/lifecycle.hpp>

#include <compare>
#include <string>
#include <string_view>
#include <vector>

namespace parcel {

/**
 * @brief Cell wrapping `comms::LifecycleStatus`. Wire kind `"lifecycle_status"`;
 *        value is the status name string.
 *
 * This is the built-in status type and the default `P` for the templated
 * lifecycle cells below.
 */
class LifecycleStatusCell : public BaseCell<LifecycleStatusCell, comms::LifecycleStatus> {
    using base_t = BaseCell<LifecycleStatusCell, comms::LifecycleStatus>;

public:
    using base_t::base_t;
    using base_t::operator=;

    static constexpr std::string_view kind_id = "lifecycle_status";

    [[nodiscard]] std::string to_string() const override {
        return this->value.name();
    }

    static cell_t from_json(json_t const& j, ParcelRegistry const&) {
        return base_t::from_json_strict(j);
    }

    static cell_type_descriptor_t descriptor() {
        static const auto d = std::make_shared<SimpleCellTypeDescriptor<LifecycleStatusCell>>(
            DisplayInfo{.name = "LifecycleStatus",
                        .description = "Lifecycle status (string name on the wire)"});
        return d;
    }
};

/**
 * @brief Cell wrapping `comms::StatusTransition<typename P::storage_t>`, a single
 *        transition event. Wire kind `"status_transition:" + P::kind_id`.
 *
 * @tparam P Status cell whose storage is the status type `T`. Defaults to
 *           `LifecycleStatusCell`; `StatusTransitionCell<>` is pre-registered,
 *           other `P` are opt-in.
 */
template <CellLike P = LifecycleStatusCell>
class StatusTransitionCell
    : public BaseCell<StatusTransitionCell<P>, comms::StatusTransition<typename P::storage_t>> {
    using base_t = BaseCell<StatusTransitionCell, comms::StatusTransition<typename P::storage_t>>;

    [[nodiscard]] static std::string render(const typename P::storage_t& s) {
        return P{s}.to_string();
    }

public:
    using element_type = P;

    using base_t::base_t;
    using base_t::operator=;

    static constexpr std::string_view kind_id = prefixed_id_v<"status_transition:", P::kind_id>;

    [[nodiscard]] std::string to_string() const override {
        return this->value.previous
                   ? render(*this->value.previous) + " \xe2\x86\x92 " + render(this->value.status)
                   : render(this->value.status);
    }

    static cell_t from_json(json_t const& j, ParcelRegistry const&) {
        return base_t::from_json_strict(j);
    }

    static cell_type_descriptor_t descriptor() {
        static const auto d = std::make_shared<SimpleCellTypeDescriptor<StatusTransitionCell>>(
            DisplayInfo{.name = "StatusTransition",
                        .description = std::string("Lifecycle transition event (status of ") +
                                       std::string(P::kind_id) + ")"});
        return d;
    }
};

/**
 * @brief Cell wrapping `comms::StatusReport<typename P::storage_t>`, a per-status
 *        period summary. Wire kind `"status_report:" + P::kind_id`.
 *
 * @tparam P Status cell whose storage is the status type `T` (see
 *           `StatusTransitionCell`).
 */
template <CellLike P = LifecycleStatusCell>
class StatusReportCell
    : public BaseCell<StatusReportCell<P>, comms::StatusReport<typename P::storage_t>> {
    using base_t = BaseCell<StatusReportCell, comms::StatusReport<typename P::storage_t>>;

public:
    using element_type = P;

    using base_t::base_t;
    using base_t::operator=;

    static constexpr std::string_view kind_id = prefixed_id_v<"status_report:", P::kind_id>;

    [[nodiscard]] std::string to_string() const override {
        return P{this->value.status}.to_string();
    }

    static cell_t from_json(json_t const& j, ParcelRegistry const&) {
        return base_t::from_json_strict(j);
    }

    static cell_type_descriptor_t descriptor() {
        static const auto d = std::make_shared<SimpleCellTypeDescriptor<StatusReportCell>>(
            DisplayInfo{.name = "StatusReport",
                        .description = std::string("Per-status period summary (status of ") +
                                       std::string(P::kind_id) + ")"});
        return d;
    }
};

/**
 * @brief Cell carrying the serializable history of a
 *        `comms::StatusTransitionTimeline<typename P::storage_t>`. Wire kind
 *        `"status_transition_timeline:" + P::kind_id` (JSON array of transitions).
 *
 * @tparam P Status cell whose storage is the status type `T` (see
 *           `StatusTransitionCell`).
 *
 * The storage is the timeline's `std::vector<StatusTransition<T>>` snapshot —
 * the live timeline is non-copyable/non-movable (it owns a mutex) and so cannot
 * be a cell's storage. The wire shape is identical to the commons timeline's own
 * JSON (a bare array of transitions), wrapped in the parcel `{"k","v"}` envelope.
 */
template <CellLike P = LifecycleStatusCell>
class StatusTransitionTimelineCell
    : public BaseCell<StatusTransitionTimelineCell<P>,
                      std::vector<comms::StatusTransition<typename P::storage_t>>> {
    using base_t = BaseCell<StatusTransitionTimelineCell,
                            std::vector<comms::StatusTransition<typename P::storage_t>>>;

public:
    using element_type = P;
    /** @brief The status type carried by each transition. */
    using status_t = typename P::storage_t;
    /** @brief The element type of the stored history. */
    using transition_t = comms::StatusTransition<status_t>;
    /** @brief The live timeline type this cell projects. */
    using timeline_t = comms::StatusTransitionTimeline<status_t>;

    using base_t::base_t;
    using base_t::operator=;

    /** @brief Snapshot a live timeline's transition history into the cell. */
    explicit StatusTransitionTimelineCell(const timeline_t& tl) : base_t(tl.transitions()) {}

    static constexpr std::string_view kind_id =
        prefixed_id_v<"status_transition_timeline:", P::kind_id>;

    /**
     * @brief Restore the stored history into a live timeline via
     *        `load_transitions` (no replay — subscribers are not notified and
     *        links are taken verbatim, matching commons' `from_json`).
     * @param tl Timeline to overwrite with this cell's history.
     */
    void load_into(timeline_t& tl) const {
        tl.load_transitions(this->value);
    }

    [[nodiscard]] std::string to_string() const override {
        return "[" + std::to_string(this->value.size()) + " status transitions]";
    }

    // std::vector storage opts out of BaseCell's default compare (see list.h), so
    // compare element-wise here — StatusTransition is equality-comparable only.
    [[nodiscard]] std::partial_ordering compare(ICell const& other) const override {
        if (const auto k = this->kind() <=> other.kind(); k != 0) {
            return k;
        }
        const auto* o = dynamic_cast<StatusTransitionTimelineCell const*>(&other);
        if (o == nullptr) {
            return std::partial_ordering::unordered;
        }
        return this->value == o->value ? std::partial_ordering::equivalent
                                       : std::partial_ordering::unordered;
    }

    static cell_t from_json(json_t const& j, ParcelRegistry const&) {
        return base_t::from_json_strict(j);
    }

    static cell_type_descriptor_t descriptor() {
        static const auto d =
            std::make_shared<SimpleCellTypeDescriptor<StatusTransitionTimelineCell>>(DisplayInfo{
                .name = "StatusTransitionTimeline",
                .description = std::string("Lifecycle transition history (status of ") +
                               std::string(P::kind_id) + ")"});
        return d;
    }
};

}  // namespace parcel

PARCEL_DEFAULT_CELL(parcel::LifecycleStatusCell);
PARCEL_DEFAULT_CELL(parcel::StatusTransitionCell<parcel::LifecycleStatusCell>);
PARCEL_DEFAULT_CELL(parcel::StatusReportCell<parcel::LifecycleStatusCell>);
PARCEL_DEFAULT_CELL(parcel::StatusTransitionTimelineCell<parcel::LifecycleStatusCell>);
