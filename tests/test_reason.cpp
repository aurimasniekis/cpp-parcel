#include <parcel/parcel.h>

#include <gtest/gtest.h>

#include <memory>
#include <string>

namespace {

// A third-party reason kind, defined and self-registered outside commons —
// proves the open-set decode resolves kinds that registered via the macro.
COMMONS_DEFINE_REASON(CustomTestReason, "test.custom_reason");
COMMONS_DEFINE_FAILURE_REASON(CustomTestFailure, "test.custom_failure");

}  // namespace

// ---------------------------------------------------------------------------
// Registration

TEST(ReasonCell, registered_in_default_registry) {
    const parcel::ParcelRegistry reg;
    EXPECT_TRUE(reg.contains("reason"));
    EXPECT_TRUE(reg.contains("failure_reason"));
}

TEST(ReasonCell, can_be_disabled_via_options) {
    const parcel::ParcelRegistry reg{parcel::BuiltinsOptions{.commons = false}};
    EXPECT_FALSE(reg.contains("reason"));
    EXPECT_FALSE(reg.contains("failure_reason"));
}

// ---------------------------------------------------------------------------
// ReasonCell

TEST(ReasonCell, basics) {
    const auto cell = parcel::ReasonCell::of(comms::make_reason<comms::GenericReason>(42, "nope"));
    EXPECT_EQ(cell->kind(), "reason");
    EXPECT_EQ(cell->to_string(), "generic");
    EXPECT_EQ(cell->to_json()["v"]["kind"], "generic");
    EXPECT_EQ(cell->to_json()["v"]["code"], 42);
    EXPECT_EQ(cell->to_json()["v"]["message"], "nope");
}

TEST(ReasonCell, null_value) {
    const auto cell = parcel::ReasonCell::of(comms::ReasonPtr{});
    EXPECT_EQ(cell->to_string(), "null");
    EXPECT_TRUE(cell->to_json()["v"].is_null());
}

TEST(ReasonCell, round_trip_builtin_kinds) {
    const parcel::ParcelRegistry reg;
    for (const auto factory :
         {+[]() -> comms::ReasonPtr { return comms::make_reason<comms::GenericReason>(1, "g"); },
          +[]() -> comms::ReasonPtr { return comms::make_reason<comms::UnknownReason>(2, "u"); }}) {
        const auto cell = parcel::ReasonCell::of(factory());
        const auto restored = reg.cell_from_json(cell->to_json());
        EXPECT_EQ(*restored, *cell);
        EXPECT_EQ(restored->to_string(), cell->to_string());
    }
}

TEST(ReasonCell, round_trip_third_party_kind) {
    const parcel::ParcelRegistry reg;
    auto reason = comms::make_reason<CustomTestReason>(7, "custom");
    const auto cell = parcel::ReasonCell::of(std::move(reason));
    EXPECT_EQ(cell->to_string(), "test.custom_reason");

    const auto restored = reg.cell_from_json(cell->to_json());
    EXPECT_EQ(*restored, *cell);
    auto* rc = dynamic_cast<parcel::ReasonCell*>(restored.get());
    ASSERT_NE(rc, nullptr);
    ASSERT_TRUE(rc->value);
    EXPECT_EQ(rc->value->kind(), "test.custom_reason");
    EXPECT_EQ(rc->value->code, 7);
    EXPECT_EQ(rc->value->message, "custom");
}

TEST(ReasonCell, clone_is_deep) {
    const auto cell = parcel::ReasonCell::of(comms::make_reason<comms::GenericReason>("g"));
    const auto cloned = cell->clone();
    EXPECT_EQ(*cloned, *cell);
    auto* rc = dynamic_cast<parcel::ReasonCell*>(cloned.get());
    ASSERT_NE(rc, nullptr);
    ASSERT_TRUE(rc->value);
    EXPECT_NE(rc->value.get(), dynamic_cast<parcel::ReasonCell*>(cell.get())->value.get());
}

TEST(ReasonCell, unknown_kind_throws) {
    const parcel::ParcelRegistry reg;
    const parcel::json_t j{{"k", "reason"}, {"v", {{"kind", "never_registered"}}}};
    EXPECT_ANY_THROW((void)reg.cell_from_json(j));
}

TEST(ReasonCell, cell_factory_infers_wrapper) {
    const auto handle = parcel::cell(comms::make_reason<comms::GenericReason>("x"));
    EXPECT_EQ(handle->kind(), "reason");
}

// ---------------------------------------------------------------------------
// FailureReasonCell

TEST(FailureReasonCell, round_trip_builtin_kinds) {
    const parcel::ParcelRegistry reg;
    auto fr = comms::make_failure_reason<comms::GenericFailureReason>(9, "boom");
    const auto cell = parcel::FailureReasonCell::of(std::move(fr));
    EXPECT_EQ(cell->kind(), "failure_reason");
    EXPECT_EQ(cell->to_string(), "generic_failure");

    const auto restored = reg.cell_from_json(cell->to_json());
    EXPECT_EQ(*restored, *cell);
    auto* fc = dynamic_cast<parcel::FailureReasonCell*>(restored.get());
    ASSERT_NE(fc, nullptr);
    ASSERT_TRUE(fc->value);
    EXPECT_EQ(fc->value->kind(), "generic_failure");
    EXPECT_EQ(fc->value->code, 9);
}

TEST(FailureReasonCell, round_trip_third_party_kind) {
    const parcel::ParcelRegistry reg;
    const auto cell =
        parcel::FailureReasonCell::of(comms::make_failure_reason<CustomTestFailure>(3, "x"));
    const auto restored = reg.cell_from_json(cell->to_json());
    EXPECT_EQ(*restored, *cell);
}

TEST(FailureReasonCell, clone_is_deep_and_typed) {
    const auto cell =
        parcel::FailureReasonCell::of(comms::make_failure_reason<comms::GenericFailureReason>("g"));
    const auto cloned = cell->clone();
    EXPECT_EQ(*cloned, *cell);
    auto* fc = dynamic_cast<parcel::FailureReasonCell*>(cloned.get());
    ASSERT_NE(fc, nullptr);
    ASSERT_TRUE(fc->value);
    // The clone keeps the IFailureReason type and is a distinct heap object.
    EXPECT_NE(fc->value.get(), dynamic_cast<parcel::FailureReasonCell*>(cell.get())->value.get());
}

TEST(FailureReasonCell, non_failure_kind_in_failure_slot_throws) {
    const parcel::ParcelRegistry reg;
    // "generic" is a plain reason, not a failure reason — decoding into a
    // failure slot must throw (commons' adl_serializer<FailureReasonPtr>).
    const parcel::json_t j{{"k", "failure_reason"}, {"v", {{"kind", "generic"}}}};
    EXPECT_ANY_THROW((void)reg.cell_from_json(j));
}

TEST(FailureReasonCell, unknown_kind_throws) {
    const parcel::ParcelRegistry reg;
    const parcel::json_t j{{"k", "failure_reason"}, {"v", {{"kind", "never_registered"}}}};
    EXPECT_ANY_THROW((void)reg.cell_from_json(j));
}
