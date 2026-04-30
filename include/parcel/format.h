#pragma once

/**
 * @file format.h
 * @brief `std::formatter` specializations for `parcel::ICell` and `parcel::cell_t`.
 *
 * Lets `std::format("{}", cell)` work on any cell. The empty spec calls
 * `to_string()` (compact); the alt-form `#` flag calls
 * `to_formatted_string()` (multi-line). Concrete cells derived from `ICell`
 * are picked up via a constrained specialization.
 *
 * Format specs supported on `ICell` / `cell_t`:
 *   - `{}`     — compact `to_string()`
 *   - `{:#}`   — multi-line `to_formatted_string()`
 *   - `{:j}`   — compact JSON via `to_json().dump()`
 *   - `{:j2}`  — pretty JSON via `to_json().dump(2)`
 *   - `{:k}`   — kind id only
 */

#include <parcel/cell.h>

#include <ostream>
#include <utility>

namespace parcel {

inline std::ostream& operator<<(std::ostream& os, ICell const& v) {
    return os << v.to_string();
}

inline std::ostream& operator<<(std::ostream& os, cell_t const& v) {
    return os << (v ? v->to_string() : std::string{"<null>"});
}

}  // namespace parcel

#if defined(__cpp_lib_format) && __cpp_lib_format >= 201907L

#include <concepts>
#include <format>

template <>
struct std::formatter<parcel::ICell, char> {
    enum class Mode { Compact, Formatted, JsonCompact, JsonPretty, KindOnly };
    Mode mode = Mode::Compact;

    constexpr auto parse(const std::format_parse_context& ctx) {
        // std::format_parse_context::iterator is implementation-defined
        // NOLINTNEXTLINE(readability-qualified-auto)
        auto it = ctx.begin();
        // std::format_parse_context::iterator is implementation-defined
        // NOLINTNEXTLINE(readability-qualified-auto)
        const auto end = ctx.end();
        if (it != end && *it == '#') {
            mode = Mode::Formatted;
            ++it;
        } else if (it != end && *it == 'j') {
            ++it;
            if (it != end && *it == '2') {
                mode = Mode::JsonPretty;
                ++it;
            } else {
                mode = Mode::JsonCompact;
            }
        } else if (it != end && *it == 'k') {
            mode = Mode::KindOnly;
            ++it;
        }
        if (it != end && *it != '}') {
            throw std::format_error(
                "parcel::ICell formatter: expected one of '{}', '{:#}', '{:j}', '{:j2}', '{:k}'");
        }
        return it;
    }

    auto format(parcel::ICell const& v, std::format_context& ctx) const {
        switch (mode) {
        case Mode::Formatted:
            return std::format_to(ctx.out(), "{}", v.to_formatted_string());
        case Mode::JsonCompact:
            return std::format_to(ctx.out(), "{}", v.to_json().dump());
        case Mode::JsonPretty:
            return std::format_to(ctx.out(), "{}", v.to_json().dump(2));
        case Mode::KindOnly:
            return std::format_to(ctx.out(), "{}", v.kind());
        case Mode::Compact:
        default:
            return std::format_to(ctx.out(), "{}", v.to_string());
        }
    }
};

template <typename T>
    requires std::derived_from<T, parcel::ICell> && (!std::same_as<T, parcel::ICell>)
struct std::formatter<T, char> : std::formatter<parcel::ICell, char> {
    auto format(T const& v, std::format_context& ctx) const {
        return std::formatter<parcel::ICell, char>::format(static_cast<parcel::ICell const&>(v),
                                                           ctx);
    }
};

template <>
struct std::formatter<parcel::cell_t, char> : std::formatter<parcel::ICell, char> {
    auto format(parcel::cell_t const& v, std::format_context& ctx) const {
        if (!v) {
            return std::format_to(ctx.out(), "<null>");
        }
        return std::formatter<parcel::ICell, char>::format(*v, ctx);
    }
};

#endif  // __cpp_lib_format

#if defined(__cpp_lib_print) && __cpp_lib_print >= 202207L
#include <print>

namespace parcel {

/**
 * @brief Forward to `std::print` — exists so `parcel::print` is a stable
 *        target alongside `parcel::format` for cell output.
 */
template <class... Args>
void print(std::format_string<Args...> fmt, Args&&... args) {
    std::print(fmt, std::forward<Args>(args)...);
}

/** @brief Forward to `std::println`. */
template <class... Args>
void println(std::format_string<Args...> fmt, Args&&... args) {
    std::println(fmt, std::forward<Args>(args)...);
}

}  // namespace parcel
#endif
