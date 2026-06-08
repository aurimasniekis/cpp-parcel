#include <parcel/parcel.h>

#include <gtest/gtest.h>

#include <chrono>
#include <string>

namespace {

// A fixed, millisecond-aligned timestamp so JSON round-trips are exact (the wire
// encoding truncates sub-millisecond system_clock ticks).
comms::LifecycleClock::time_point fixed_ts(const std::int64_t ms) {
    return comms::LifecycleClock::time_point{std::chrono::milliseconds{ms}};
}

}  // namespace

// ---------------------------------------------------------------------------
// Registration

TEST(LifecycleCell, registered_in_default_registry) {
    const parcel::ParcelRegistry reg;
    EXPECT_TRUE(reg.contains("lifecycle_status"));
    EXPECT_TRUE(reg.contains("status_transition:lifecycle_status"));
    EXPECT_TRUE(reg.contains("status_report:lifecycle_status"));
    EXPECT_TRUE(reg.contains("status_transition_timeline:lifecycle_status"));
}

TEST(LifecycleCell, default_kind_ids) {
    EXPECT_EQ(parcel::StatusTransitionCell<>::kind_id,
              std::string_view{"status_transition:lifecycle_status"});
    EXPECT_EQ(parcel::StatusReportCell<>::kind_id,
              std::string_view{"status_report:lifecycle_status"});
    EXPECT_EQ(parcel::StatusTransitionTimelineCell<>::kind_id,
              std::string_view{"status_transition_timeline:lifecycle_status"});
}

TEST(LifecycleCell, can_be_disabled_via_options) {
    const parcel::ParcelRegistry reg{parcel::BuiltinsOptions{.commons = false}};
    EXPECT_FALSE(reg.contains("lifecycle_status"));
    EXPECT_FALSE(reg.contains("status_transition_timeline:lifecycle_status"));
}

// ---------------------------------------------------------------------------
// LifecycleStatusCell

TEST(LifecycleCell, status_round_trip) {
    const parcel::ParcelRegistry reg;
    const parcel::LifecycleStatusCell cell{comms::LifecycleStatus{"active"}};
    EXPECT_EQ(cell.kind(), "lifecycle_status");
    EXPECT_EQ(cell.to_string(), "active");
    EXPECT_EQ(cell.to_json()["v"], "active");

    const auto restored = reg.cell_from_json(cell.to_json());
    const auto* sc = dynamic_cast<parcel::LifecycleStatusCell*>(restored.get());
    ASSERT_NE(sc, nullptr);
    EXPECT_EQ(sc->value, comms::LifecycleStatus{"active"});
}

// ---------------------------------------------------------------------------
// StatusTransitionCell

TEST(LifecycleCell, transition_round_trip_with_previous) {
    const parcel::ParcelRegistry reg;
    comms::StatusTransition<comms::LifecycleStatus> tr;
    tr.previous = comms::LifecycleStatus{"pending"};
    tr.status = comms::LifecycleStatus{"active"};
    tr.timestamp = fixed_ts(1'700'000'000'000);
    tr.previous_timestamp = fixed_ts(1'699'999'999'000);

    const parcel::StatusTransitionCell<> cell{tr};
    EXPECT_EQ(cell.kind(), "status_transition:lifecycle_status");
    EXPECT_EQ(cell.to_string(), "pending \xe2\x86\x92 active");

    const auto restored = reg.cell_from_json(cell.to_json());
    const auto* tc = dynamic_cast<parcel::StatusTransitionCell<>*>(restored.get());
    ASSERT_NE(tc, nullptr);
    EXPECT_EQ(tc->value, tr);
}

TEST(LifecycleCell, first_transition_has_no_previous) {
    comms::StatusTransition<comms::LifecycleStatus> tr;
    tr.status = comms::LifecycleStatus{"created"};
    tr.timestamp = fixed_ts(1'700'000'000'000);

    const parcel::StatusTransitionCell<> cell{tr};
    EXPECT_EQ(cell.to_string(), "created");
    EXPECT_FALSE(cell.to_json()["v"].contains("previous"));
}

// ---------------------------------------------------------------------------
// StatusReportCell

TEST(LifecycleCell, report_round_trip) {
    const parcel::ParcelRegistry reg;
    comms::StatusReport<comms::LifecycleStatus> rep;
    rep.previous = comms::LifecycleStatus{"pending"};
    rep.status = comms::LifecycleStatus{"active"};
    rep.duration = std::chrono::milliseconds{5'000};
    rep.total_duration = std::chrono::milliseconds{12'000};

    const parcel::StatusReportCell<> cell{rep};
    EXPECT_EQ(cell.kind(), "status_report:lifecycle_status");
    EXPECT_EQ(cell.to_string(), "active");

    const auto restored = reg.cell_from_json(cell.to_json());
    const auto* rc = dynamic_cast<parcel::StatusReportCell<>*>(restored.get());
    ASSERT_NE(rc, nullptr);
    EXPECT_EQ(rc->value, rep);
}

// ---------------------------------------------------------------------------
// StatusTransitionTimelineCell — bridges to/from a live timeline

TEST(LifecycleCell, timeline_round_trip_and_bridge) {
    const parcel::ParcelRegistry reg;

    comms::StatusTransitionTimeline<comms::LifecycleStatus> tl{comms::LifecycleStatus{"created"},
                                                               fixed_ts(1'700'000'000'000)};
    tl.transition_to(comms::LifecycleStatus{"active"}, fixed_ts(1'700'000'001'000));
    tl.transition_to(comms::LifecycleStatus{"closed"}, fixed_ts(1'700'000'002'000));

    const parcel::StatusTransitionTimelineCell<> cell{tl};
    EXPECT_EQ(cell.kind(), "status_transition_timeline:lifecycle_status");
    EXPECT_EQ(cell.to_string(), "[3 status transitions]");
    EXPECT_EQ(cell.to_json()["v"].size(), 3u);

    const auto restored = reg.cell_from_json(cell.to_json());
    const auto* lc = dynamic_cast<parcel::StatusTransitionTimelineCell<>*>(restored.get());
    ASSERT_NE(lc, nullptr);
    EXPECT_EQ(lc->value, cell.value);

    // Bridge back into a live timeline and confirm the history is restored.
    comms::StatusTransitionTimeline<comms::LifecycleStatus> rebuilt;
    lc->load_into(rebuilt);
    ASSERT_EQ(rebuilt.size(), 3u);
    EXPECT_EQ(rebuilt.current_status(), comms::LifecycleStatus{"closed"});
    EXPECT_EQ(rebuilt.first_status(), comms::LifecycleStatus{"created"});
}

TEST(LifecycleCell, timeline_compare_equal_by_value) {
    comms::StatusTransitionTimeline<comms::LifecycleStatus> tl{comms::LifecycleStatus{"a"},
                                                               fixed_ts(1'700'000'000'000)};
    tl.transition_to(comms::LifecycleStatus{"b"}, fixed_ts(1'700'000'001'000));

    const parcel::StatusTransitionTimelineCell<> c1{tl};
    const parcel::StatusTransitionTimelineCell<> c2{tl};
    EXPECT_EQ(c1, c2);

    parcel::StatusTransitionTimelineCell<> c3{tl};
    c3.value.pop_back();
    EXPECT_NE(c1, c3);
}

// ---------------------------------------------------------------------------
// Custom status type T — parameterize on any status cell P (here integer status
// codes via I32Cell). Opt-in: not auto-registered, register per element cell.

TEST(LifecycleCell, custom_status_type_not_auto_registered) {
    const parcel::ParcelRegistry reg;
    EXPECT_FALSE(reg.contains("status_transition:i32"));
    EXPECT_FALSE(reg.contains("status_transition_timeline:i32"));
}

TEST(LifecycleCell, custom_status_type_round_trip) {
    using TransitionCell = parcel::StatusTransitionCell<parcel::I32Cell>;
    using TimelineCell = parcel::StatusTransitionTimelineCell<parcel::I32Cell>;
    EXPECT_EQ(TransitionCell::kind_id, std::string_view{"status_transition:i32"});
    EXPECT_EQ(TimelineCell::kind_id, std::string_view{"status_transition_timeline:i32"});

    parcel::ParcelRegistry reg;
    reg.register_kind(TransitionCell::descriptor());
    reg.register_kind(TimelineCell::descriptor());

    // A live timeline over std::int32_t status codes.
    comms::StatusTransitionTimeline<std::int32_t> tl{1, fixed_ts(1'700'000'000'000)};
    tl.transition_to(2, fixed_ts(1'700'000'001'000));

    const TransitionCell tr_cell{tl.current_transition()};
    EXPECT_EQ(tr_cell.to_string(), "1 \xe2\x86\x92 2");  // rendered through I32Cell
    const auto tr_restored = reg.cell_from_json(tr_cell.to_json());
    ASSERT_NE(dynamic_cast<TransitionCell*>(tr_restored.get()), nullptr);
    EXPECT_EQ(parcel::cell_cast_value<TransitionCell>(tr_restored), tl.current_transition());

    const TimelineCell tl_cell{tl};
    const auto tl_restored = reg.cell_from_json(tl_cell.to_json());
    const auto* lc = dynamic_cast<TimelineCell*>(tl_restored.get());
    ASSERT_NE(lc, nullptr);

    comms::StatusTransitionTimeline<std::int32_t> rebuilt;
    lc->load_into(rebuilt);
    ASSERT_EQ(rebuilt.size(), 2u);
    EXPECT_EQ(rebuilt.current_status(), 2);
}
