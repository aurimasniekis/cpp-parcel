#pragma once

/**
 * @file ext/chrono.h
 * @brief Cell adapters for `std::chrono` types.
 *
 * Adds three cells for the most common time payloads:
 *   - `UnixMillisCell`     — `sys_time<milliseconds>`, wire is i64 ms epoch.
 *   - `DurationMsCell`     — `std::chrono::milliseconds`, wire is i64.
 *   - `YmdCell`            — `std::chrono::year_month_day`, wire is ISO `"YYYY-MM-DD"`.
 *   - `SystemTimePointCell`— `sys_seconds`, wire is i64 epoch seconds.
 *
 * `TimestampMsCell` is exposed as an alias of `UnixMillisCell` for callers
 * who prefer a wire-shape name. The recommended default for cross-language
 * timestamps is `UnixMillisCell` (matches JavaScript `Date.now()` and Java
 * `Instant.toEpochMilli()`).
 */

#include <parcel/cell.h>
#include <parcel/defaults.h>
#include <parcel/descriptor.h>
#include <parcel/json.h>

#include <charconv>
#include <chrono>
#include <memory>
#include <string>
#include <string_view>

namespace parcel {

/**
 * @brief Wall-clock time at second resolution (ISO-8601-friendly).
 *
 * Wire kind: `"time:sys_seconds"`. Value is i64 epoch seconds — no string
 * round-trip on the hot path.
 */
class SystemTimePointCell : public BaseCell<SystemTimePointCell, std::chrono::sys_seconds> {
    using base_t = BaseCell<SystemTimePointCell, std::chrono::sys_seconds>;

public:
    using base_t::base_t;
    using base_t::operator=;

    static constexpr std::string_view kind_id = "time:sys_seconds";

    [[nodiscard]] std::string to_string() const override {
        return std::to_string(this->value.time_since_epoch().count());
    }

    [[nodiscard]] json_t to_json() const override {
        json_t j{
            {ICell::KEY_KIND, kind_id},
            {ICell::KEY_VALUE, static_cast<std::int64_t>(this->value.time_since_epoch().count())},
        };
        this->inject_meta(j);
        return j;
    }

    static cell_t from_json(json_t const& j, ParcelRegistry const&) {
        const auto epoch = base_t::cell_from_json<std::int64_t>(j, kind_id);
        auto cell = std::make_shared<SystemTimePointCell>(
            std::chrono::sys_seconds{std::chrono::seconds{epoch}});
        base_t::absorb_meta(j, cell);
        return cell;
    }

    static cell_type_descriptor_t descriptor() {
        static const auto d =
            std::make_shared<SimpleCellTypeDescriptor<SystemTimePointCell>>(descriptor::MetaInfo{
                .name = "SystemTimePoint", .description = "Wall-clock time, second resolution"});
        return d;
    }
};

/**
 * @brief Wall-clock time at millisecond resolution.
 *
 * Wire kind: `"time:unix_ms"`. Value is i64 epoch ms — matches JS
 * `Date.now()`, Java `Instant.toEpochMilli()`, Python
 * `time.time_ns()/1_000_000`.
 */
class UnixMillisCell
    : public BaseCell<UnixMillisCell, std::chrono::sys_time<std::chrono::milliseconds>> {
    using base_t = BaseCell<UnixMillisCell, std::chrono::sys_time<std::chrono::milliseconds>>;

public:
    using base_t::base_t;
    using base_t::operator=;

    static constexpr std::string_view kind_id = "time:unix_ms";

    [[nodiscard]] std::string to_string() const override {
        return std::to_string(this->value.time_since_epoch().count()) + "ms";
    }

    [[nodiscard]] json_t to_json() const override {
        json_t j{
            {ICell::KEY_KIND, kind_id},
            {ICell::KEY_VALUE, static_cast<std::int64_t>(this->value.time_since_epoch().count())},
        };
        this->inject_meta(j);
        return j;
    }

    static cell_t from_json(json_t const& j, ParcelRegistry const&) {
        const auto epoch = base_t::cell_from_json<std::int64_t>(j, kind_id);
        auto cell = std::make_shared<UnixMillisCell>(
            std::chrono::sys_time<std::chrono::milliseconds>{std::chrono::milliseconds{epoch}});
        base_t::absorb_meta(j, cell);
        return cell;
    }

    static cell_type_descriptor_t descriptor() {
        static const auto d =
            std::make_shared<SimpleCellTypeDescriptor<UnixMillisCell>>(descriptor::MetaInfo{
                .name = "UnixMillis", .description = "Wall-clock time as Unix epoch milliseconds"});
        return d;
    }
};

/** @brief Wire-shape alias of `UnixMillisCell` for code that thinks in ms. */
using TimestampMsCell = UnixMillisCell;

/**
 * @brief Duration in milliseconds.
 *
 * Wire kind: `"time:ms"`. Value is i64.
 */
class DurationMsCell : public BaseCell<DurationMsCell, std::chrono::milliseconds> {
    using base_t = BaseCell<DurationMsCell, std::chrono::milliseconds>;

public:
    using base_t::base_t;
    using base_t::operator=;

    static constexpr std::string_view kind_id = "time:ms";

    [[nodiscard]] std::string to_string() const override {
        return std::to_string(this->value.count()) + "ms";
    }

    [[nodiscard]] json_t to_json() const override {
        json_t j{
            {ICell::KEY_KIND, kind_id},
            {ICell::KEY_VALUE, static_cast<std::int64_t>(this->value.count())},
        };
        this->inject_meta(j);
        return j;
    }

    static cell_t from_json(json_t const& j, ParcelRegistry const&) {
        const auto v = base_t::cell_from_json<std::int64_t>(j, kind_id);
        auto cell = std::make_shared<DurationMsCell>(std::chrono::milliseconds{v});
        base_t::absorb_meta(j, cell);
        return cell;
    }

    static cell_type_descriptor_t descriptor() {
        static const auto d = std::make_shared<SimpleCellTypeDescriptor<DurationMsCell>>(
            descriptor::MetaInfo{.name = "DurationMs", .description = "Duration in milliseconds"});
        return d;
    }
};

/**
 * @brief Calendar date (no time of day).
 *
 * Wire kind: `"time:ymd"`. Value is ISO-8601 `"YYYY-MM-DD"` string.
 */
class YmdCell : public BaseCell<YmdCell, std::chrono::year_month_day> {
    using base_t = BaseCell<YmdCell, std::chrono::year_month_day>;

public:
    using base_t::base_t;
    using base_t::operator=;

    static constexpr std::string_view kind_id = "time:ymd";

    [[nodiscard]] std::string to_string() const override {
        return format_iso(this->value);
    }

    [[nodiscard]] json_t to_json() const override {
        json_t j{
            {ICell::KEY_KIND, kind_id},
            {ICell::KEY_VALUE, format_iso(this->value)},
        };
        this->inject_meta(j);
        return j;
    }

    static cell_t from_json(json_t const& j, ParcelRegistry const&) {
        const auto s = base_t::cell_from_json<std::string>(j, kind_id);
        auto cell = std::make_shared<YmdCell>(parse_iso(s));
        base_t::absorb_meta(j, cell);
        return cell;
    }

    static cell_type_descriptor_t descriptor() {
        static const auto d =
            std::make_shared<SimpleCellTypeDescriptor<YmdCell>>(descriptor::MetaInfo{
                .name = "YearMonthDay", .description = "Calendar date (ISO-8601 YYYY-MM-DD)"});
        return d;
    }

private:
    static std::string format_iso(std::chrono::year_month_day const& ymd) {
        std::string out;
        out.reserve(10);
        const int y = static_cast<int>(ymd.year());
        const unsigned m = static_cast<unsigned>(ymd.month());
        const unsigned d = static_cast<unsigned>(ymd.day());
        const bool neg = y < 0;
        const auto ay = static_cast<unsigned>(neg ? -y : y);
        // %04u allows 10-digit unsigned, plus sign + separators + null → up
        // to 18 bytes; round to 24 to keep gcc's truncation analysis quiet.
        char buf[24];  // NOLINT(modernize-avoid-c-arrays) — fixed-size local scratch for snprintf
        // Hand-rolled to avoid <format>'s chrono extensions on older libc++.
        // NOLINTNEXTLINE(cppcoreguidelines-pro-type-vararg, cert-err33-c)
        std::snprintf(buf, sizeof(buf), "%s%04u-%02u-%02u", neg ? "-" : "", ay, m, d);
        out = buf;
        return out;
    }

    static std::chrono::year_month_day parse_iso(std::string const& s) {
        // Accept "YYYY-MM-DD" or "-YYYY-MM-DD" (BCE).
        const std::string_view sv{s};

        auto fail = [&s] {
            throw InvalidJsonException("YmdCell: invalid ISO-8601 date '" + s + "'",
                                       std::string(YmdCell::kind_id));
        };

        int y = 0;
        unsigned m = 0;
        unsigned d = 0;

        std::size_t first_dash = 4;
        if (!sv.empty() && sv.front() == '-') {
            first_dash = 5;
        }

        if (sv.size() < first_dash + 4 || sv[first_dash] != '-') {
            fail();
        }

        auto const year_part = sv.substr(0, first_dash);
        auto const month_part = sv.substr(first_dash + 1, 2);

        if (sv[first_dash + 3] != '-') {
            fail();
        }

        auto const day_part = sv.substr(first_dash + 4);

        auto parse_int = [](const std::string_view part, auto& out) {
            auto const* first = part.data();
            auto const* last = part.data() + part.size();
            auto const [ptr, ec] = std::from_chars(first, last, out);
            return ec == std::errc{} && ptr == last;
        };

        if (!parse_int(year_part, y) || !parse_int(month_part, m) || !parse_int(day_part, d)) {
            fail();
        }

        auto const result = std::chrono::year_month_day{
            std::chrono::year{y}, std::chrono::month{m}, std::chrono::day{d}};

        if (!result.ok()) {
            fail();
        }

        return result;
    }
};

}  // namespace parcel

PARCEL_DEFAULT_CELL(parcel::SystemTimePointCell);
PARCEL_DEFAULT_CELL(parcel::UnixMillisCell);
PARCEL_DEFAULT_CELL(parcel::DurationMsCell);
PARCEL_DEFAULT_CELL(parcel::YmdCell);
