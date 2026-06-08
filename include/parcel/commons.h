#pragma once

/**
 * @file commons.h
 * @brief Umbrella header for the parcel cell adapters wrapping the
 *        `aurimasniekis/cpp-commons` vocabulary types.
 *
 * Each cell family lives in its own focused header under `parcel/commons/`;
 * this umbrella `#include`s them all (the same shape as `<parcel/parcel.h>`).
 * Include `<parcel/commons.h>` to get every commons adapter at once.
 *
 *   - `ColorCell`             — `comms::Color`             → `"color"` (hex string).
 *   - `IconCell`              — `comms::Icon`              → `"icon"` (`"set:name"` string).
 *   - `DisplayInfoCell`       — `comms::DisplayInfo`       → `"display_info"` (JSON object).
 *   - `FlagCell`              — `comms::FlagRef`           → `"flag"` (flag name string).
 *   - `FlagSetCell`           — `comms::FlagSet`           → `"flag_set"` (array of names).
 *   - `SemVerCell`            — `comms::SemVer`            → `"semver"` (version string).
 *   - `VersionConstraintCell` — `comms::VersionConstraint` → `"version_constraint"` (range string).
 *   - `OriginCell`            — `comms::OriginPtr`         → `"origin"` (`{"kind",…}` object).
 *   - `ReasonCell`            — `comms::ReasonPtr`         → `"reason"` (`{"kind",…}` object).
 *   - `FailureReasonCell`     — `comms::FailureReasonPtr`  → `"failure_reason"` (`{"kind",…}`).
 *   - `AbilityCell`           — `comms::AbilityPtr`        → `"ability"` (`{"kind",…}` object).
 *   - `IdentityCell`          — `comms::IdentityPtr`       → `"identity"` (`{"kind",…}` object).
 *   - `AuditRecordCell`       — `comms::AuditRecord`       → `"audit_record"` (JSON object).
 *   - `AuditRecordsCell`      — `comms::AuditRecords`      → `"audit_records"` (JSON array).
 *   - `ChangeAuditRecordCell<P>`  → `"change_audit_record:" + P::kind_id` (opt-in).
 *   - `ChangeAuditRecordsCell<P>` → `"change_audit_records:" + P::kind_id` (opt-in).
 *   - `ValueCell` / `ObjectCell` / `ArrayCell` — `comms::md::*` → `"md:v"` / `"md:o"` / `"md:a"`.
 *   - `LifecycleStatusCell`             — `comms::LifecycleStatus`     → `"lifecycle_status"` (string).
 *   - `StatusTransitionCell<P>`         — `comms::StatusTransition<P::storage_t>`     → `"status_transition:" + P::kind_id` (object).
 *   - `StatusReportCell<P>`             — `comms::StatusReport<P::storage_t>`         → `"status_report:" + P::kind_id` (object).
 *   - `StatusTransitionTimelineCell<P>` — the transition history       → `"status_transition_timeline:" + P::kind_id` (array).
 *     (`P` defaults to `LifecycleStatusCell`; the `<>` instantiations are registered, other `P` are opt-in.)
 *
 * Each cell reuses commons' ADL `to_json` / `from_json` (auto-enabled because
 * nlohmann/json is on the include path), so the inherited serialization just
 * works — only `to_string`, `kind_id`, `from_json`, and `descriptor()` are
 * defined per cell, following the `ext/filesystem.h` `PathCell` pattern.
 *
 * The polymorphic cells (`OriginCell` / `ReasonCell` / `FailureReasonCell` /
 * `AbilityCell` / `IdentityCell`) are open sets: decoding resolves the inner
 * `"kind"` against the matching commons global registry and throws on an unknown
 * kind, so third-party kinds that self-register decode correctly too.
 */

#include <parcel/commons/ability.h>
#include <parcel/commons/audit.h>
#include <parcel/commons/color.h>
#include <parcel/commons/display_info.h>
#include <parcel/commons/flag.h>
#include <parcel/commons/icon.h>
#include <parcel/commons/identity.h>
#include <parcel/commons/lifecycle.h>
#include <parcel/commons/metadata.h>
#include <parcel/commons/origin.h>
#include <parcel/commons/reason.h>
#include <parcel/commons/semver.h>
#include <parcel/commons/version_constraint.h>
