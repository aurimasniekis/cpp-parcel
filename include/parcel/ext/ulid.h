#pragma once

/**
 * @file ext/ulid.h
 * @brief Optional cell adapter for `ulid::Ulid` (from `aurimasniekis/cpp-ulid`).
 *
 * Gated on `PARCEL_HAS_ULID`: a build-predefined value (set by the
 * `PARCEL_WITH_ULID` CMake option / Meson `ulid` option) always wins;
 * otherwise it is auto-detected via `__has_include(<ulid/ulid.h>)`, mirroring
 * commons' own `COMMONS_WITH_ULID` gate. The header is inert (defines nothing)
 * when ULID is unavailable, so it is safe to include unconditionally — which
 * `<parcel/builtins.h>` does to keep the umbrella header complete.
 *
 * Wire shape: `{"k": "ulid", "v": "<26-char Crockford base32>"}`.
 */

#if !defined(PARCEL_HAS_ULID)
#if defined(__has_include)
#if __has_include(<ulid/ulid.h>)
#define PARCEL_HAS_ULID 1
#else
#define PARCEL_HAS_ULID 0
#endif
#else
#define PARCEL_HAS_ULID 0
#endif
#endif

#if PARCEL_HAS_ULID

#include <parcel/cell.h>
#include <parcel/defaults.h>
#include <parcel/descriptor.h>
#include <parcel/json.h>

#include <memory>
#include <string>
#include <string_view>

#include <ulid/ulid.h>

namespace parcel {

/**
 * @brief Cell wrapping `ulid::Ulid`. Wire kind `"ulid"`; value is a Crockford
 *        base32 string.
 *
 * `ulid::Ulid` is a copyable value type with `operator<=>` and nlohmann hooks
 * (self-enabled because nlohmann/json is on the include path), so the inherited
 * `BaseCell::to_json` / `compare` / `clone` just work — only `to_string`,
 * `kind_id`, `from_json`, and `descriptor()` are defined here.
 */
class UlidCell : public BaseCell<UlidCell, ulid::Ulid> {
    using base_t = BaseCell<UlidCell, ulid::Ulid>;

public:
    using base_t::base_t;
    using base_t::operator=;

    static constexpr std::string_view kind_id = "ulid";

    [[nodiscard]] std::string to_string() const override {
        return this->value.string();
    }

    static cell_t from_json(json_t const& j, ParcelRegistry const&) {
        auto v = base_t::cell_from_json<ulid::Ulid>(j, kind_id);
        auto cell = std::make_shared<UlidCell>(v);
        base_t::absorb_display_info(j, cell);
        return cell;
    }

    static cell_type_descriptor_t descriptor() {
        static const auto d = std::make_shared<SimpleCellTypeDescriptor<UlidCell>>(DisplayInfo{
            .name = "ULID", .description = "ULID (26-char Crockford base32 string on the wire)"});
        return d;
    }
};

}  // namespace parcel

PARCEL_DEFAULT_CELL(parcel::UlidCell);

#endif  // PARCEL_HAS_ULID
