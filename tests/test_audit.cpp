#include <parcel/parcel.h>

#include <gtest/gtest.h>

#include <chrono>
#include <memory>
#include <string>

namespace {

// A fixed, millisecond-aligned timestamp so JSON round-trips are exact (the wire
// encoding truncates sub-millisecond system_clock ticks).
comms::AuditClock::time_point fixed_ts(const std::int64_t ms) {
    return comms::AuditClock::time_point{std::chrono::milliseconds{ms}};
}

}  // namespace

// ---------------------------------------------------------------------------
// Registration

TEST(AuditCell, registered_in_default_registry) {
    const parcel::ParcelRegistry reg;
    EXPECT_TRUE(reg.contains("audit_record"));
    EXPECT_TRUE(reg.contains("audit_records"));
}

TEST(AuditCell, can_be_disabled_via_options) {
    const parcel::ParcelRegistry reg{parcel::BuiltinsOptions{.commons = false}};
    EXPECT_FALSE(reg.contains("audit_record"));
    EXPECT_FALSE(reg.contains("audit_records"));
}

// ---------------------------------------------------------------------------
// AuditRecordCell

TEST(AuditCell, record_round_trip_full) {
    const parcel::ParcelRegistry reg;
    comms::AuditRecord rec;
    rec.set_identity(comms::make_identity<comms::UserIdentity>("alice"));
    rec.timestamp = fixed_ts(1'700'000'000'000);
    rec.ip = "10.0.0.1";
    rec.related_ids["order"] = "ord-123";
    rec.metadata = comms::md::Object{{"attempt", 2}};

    const parcel::AuditRecordCell cell{rec};
    EXPECT_EQ(cell.kind(), "audit_record");
    EXPECT_EQ(cell.to_string(), comms::to_string(*rec.identity));

    const auto restored = reg.cell_from_json(cell.to_json());
    const auto* rc = dynamic_cast<parcel::AuditRecordCell*>(restored.get());
    ASSERT_NE(rc, nullptr);
    EXPECT_EQ(rc->value, rec);
}

TEST(AuditCell, record_default_identity_is_none) {
    const parcel::ParcelRegistry reg;
    comms::AuditRecord rec;
    rec.timestamp = fixed_ts(1'700'000'000'000);
    const parcel::AuditRecordCell cell{rec};

    const auto restored = reg.cell_from_json(cell.to_json());
    const auto* rc = dynamic_cast<parcel::AuditRecordCell*>(restored.get());
    ASSERT_NE(rc, nullptr);
    ASSERT_TRUE(rc->value.identity);
    EXPECT_EQ(rc->value.identity->kind(), "none");
}

// ---------------------------------------------------------------------------
// AuditRecordsCell — within-capacity log round-trips exactly

TEST(AuditCell, records_within_capacity_round_trip) {
    const parcel::ParcelRegistry reg;
    comms::AuditRecords log;  // default capacity (>= 3)
    for (std::int64_t i = 0; i < 3; ++i) {
        comms::AuditRecord rec;
        rec.timestamp = fixed_ts(1'700'000'000'000 + i);
        rec.session_id = "s-" + std::to_string(i);
        log.push(rec);
    }
    ASSERT_EQ(log.size(), 3u);

    const parcel::AuditRecordsCell cell{log};
    EXPECT_EQ(cell.kind(), "audit_records");

    const auto restored = reg.cell_from_json(cell.to_json());
    const auto* rc = dynamic_cast<parcel::AuditRecordsCell*>(restored.get());
    ASSERT_NE(rc, nullptr);
    ASSERT_EQ(rc->value.size(), 3u);
    EXPECT_EQ(rc->value, log);
}

// ---------------------------------------------------------------------------
// ChangeAuditRecordCell<P> — before/after round-trip (opt-in registration)

TEST(AuditCell, change_record_before_after_round_trip) {
    using ChangeCell = parcel::ChangeAuditRecordCell<parcel::StringCell>;
    EXPECT_EQ(ChangeCell::kind_id, std::string_view{"change_audit_record:string"});

    parcel::ParcelRegistry reg;
    reg.register_kind(ChangeCell::descriptor());

    comms::ChangeAuditRecord<std::string> rec;
    rec.timestamp = fixed_ts(1'700'000'000'000);
    rec.before = "old";
    rec.after = "new";

    const ChangeCell cell{rec};
    EXPECT_EQ(cell.to_json()["v"]["before"], "old");
    EXPECT_EQ(cell.to_json()["v"]["after"], "new");

    const auto restored = reg.cell_from_json(cell.to_json());
    const auto* cc = dynamic_cast<ChangeCell*>(restored.get());
    ASSERT_NE(cc, nullptr);
    EXPECT_EQ(cc->value, rec);
    ASSERT_TRUE(cc->value.before.has_value());
    EXPECT_EQ(*cc->value.before, "old");
    ASSERT_TRUE(cc->value.after.has_value());
    EXPECT_EQ(*cc->value.after, "new");
}

TEST(AuditCell, change_record_create_has_no_before) {
    using ChangeCell = parcel::ChangeAuditRecordCell<parcel::StringCell>;
    parcel::ParcelRegistry reg;
    reg.register_kind(ChangeCell::descriptor());

    comms::ChangeAuditRecord<std::string> rec;
    rec.timestamp = fixed_ts(1'700'000'000'000);
    rec.after = "created";  // a create: no before

    const ChangeCell cell{rec};
    EXPECT_FALSE(cell.to_json()["v"].contains("before"));

    const auto restored = reg.cell_from_json(cell.to_json());
    const auto* cc = dynamic_cast<ChangeCell*>(restored.get());
    ASSERT_NE(cc, nullptr);
    EXPECT_FALSE(cc->value.before.has_value());
    ASSERT_TRUE(cc->value.after.has_value());
    EXPECT_EQ(*cc->value.after, "created");
}

TEST(AuditCell, change_records_log_round_trip) {
    using ChangeLogCell = parcel::ChangeAuditRecordsCell<parcel::StringCell>;
    EXPECT_EQ(ChangeLogCell::kind_id, std::string_view{"change_audit_records:string"});

    parcel::ParcelRegistry reg;
    reg.register_kind(ChangeLogCell::descriptor());

    comms::ChangeAuditRecords<std::string> log;
    for (std::int64_t i = 0; i < 2; ++i) {
        comms::ChangeAuditRecord<std::string> rec;
        rec.timestamp = fixed_ts(1'700'000'000'000 + i);
        rec.after = "v" + std::to_string(i);
        log.push(rec);
    }

    const ChangeLogCell cell{log};
    const auto restored = reg.cell_from_json(cell.to_json());
    const auto* cc = dynamic_cast<ChangeLogCell*>(restored.get());
    ASSERT_NE(cc, nullptr);
    EXPECT_EQ(cc->value, log);
}

TEST(AuditCell, change_record_cells_not_auto_registered) {
    const parcel::ParcelRegistry reg;
    EXPECT_FALSE(reg.contains("change_audit_record:string"));
    EXPECT_FALSE(reg.contains("change_audit_records:string"));
}
