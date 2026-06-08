#pragma once

/**
 * @file metadata.h
 * @brief Parcel adapters for the `comms::md` dynamic value tree.
 *
 * Wire kinds:
 *   - `md:v` — `ValueCell`  (storage: `comms::md::Value`)
 *   - `md:o` — `ObjectCell` (storage: `comms::md::Object`)
 *   - `md:a` — `ArrayCell`  (storage: `comms::md::Array`)
 *
 * Ported from the standalone `metadata` sibling library's parcel adapter,
 * re-namespaced to the `comms::md::*` types absorbed into commons.
 */

#include <parcel/cell.h>
#include <parcel/defaults.h>
#include <parcel/descriptor.h>
#include <parcel/json.h>

#include <commons/color.hpp>
#include <commons/icons.hpp>
#include <commons/json/metadata.hpp>  // brings ADL to_json/from_json for comms::md types
#include <commons/metadata.hpp>       // types + operator<< for to_string()

#include <sstream>
#include <string>
#include <string_view>
#include <utility>

namespace parcel {

/**
 * @brief Cell wrapping a `comms::md::Value`. Wire kind `md:v`.
 *
 * The `"v"` slot carries the value in its natural JSON shape via the
 * `<commons/json/metadata.hpp>` ADL hooks.
 */
class ValueCell : public BaseCell<ValueCell, comms::md::Value> {
    using base_t = BaseCell<ValueCell, comms::md::Value>;

public:
    using base_t::base_t;
    using base_t::operator=;

    static constexpr std::string_view kind_id = "md:v";

    [[nodiscard]] std::string to_string() const override {
        std::ostringstream os;
        os << this->value;
        return os.str();
    }

    static cell_t from_json(json_t const& j, ParcelRegistry const&) {
        return base_t::from_json_strict(j);
    }

    static cell_type_descriptor_t descriptor() {
        static const auto d = std::make_shared<SimpleCellTypeDescriptor<ValueCell>>(DisplayInfo{
            .name = "md::Value",
            .description = "Dynamic JSON-shaped value from the commons metadata tree.",
            .icon = comms::Icons::mdi::variable,
            .color = comms::Colors::mui::deep_purple[500],
        });
        return d;
    }
};

/**
 * @brief Cell wrapping a `comms::md::Object` (a.k.a. `comms::Metadata`). Wire kind `md:o`.
 *
 * The `"v"` slot is a JSON object whose values are themselves serialized through
 * the `comms::md::Value` JSON adapter.
 */
class ObjectCell : public BaseCell<ObjectCell, comms::md::Object> {
    using base_t = BaseCell<ObjectCell, comms::md::Object>;

public:
    using base_t::base_t;
    using base_t::operator=;

    /**
     * @brief Brace-init passthrough so `ObjectCell{{"k", 1}, {"k2", "v"}}`
     *        mirrors `comms::md::Object{...}` exactly.
     */
    ObjectCell(const std::initializer_list<comms::md::Object::value_type> il)
        : base_t(comms::md::Object(il)) {}

    static constexpr std::string_view kind_id = "md:o";

    [[nodiscard]] std::string to_string() const override {
        std::ostringstream os;
        os << this->value;
        return os.str();
    }

    static cell_t from_json(json_t const& j, ParcelRegistry const&) {
        return base_t::from_json_strict(j);
    }

    static cell_type_descriptor_t descriptor() {
        static const auto d = std::make_shared<SimpleCellTypeDescriptor<ObjectCell>>(DisplayInfo{
            .name = "md::Object",
            .description = "Unordered string-keyed map of md::Value (md::Object/Metadata).",
            .icon = comms::Icons::mdi::code_braces,
            .color = comms::Colors::mui::amber[700],
        });
        return d;
    }
};

/**
 * @brief Cell wrapping a `comms::md::Array` (`std::vector<comms::md::Value>`). Wire kind `md:a`.
 *
 * The `"v"` slot is a JSON array whose elements are serialized through the
 * `comms::md::Value` JSON adapter.
 */
class ArrayCell : public BaseCell<ArrayCell, comms::md::Array> {
    using base_t = BaseCell<ArrayCell, comms::md::Array>;

public:
    using base_t::base_t;
    using base_t::operator=;

    /**
     * @brief Brace-init passthrough so `ArrayCell{"a", "b", "c"}` mirrors
     *        `comms::md::Array{...}`. Constrained to ≥2 elements so that
     *        `ArrayCell{some_array}` continues to forward to `BaseCell` —
     *        `Array → Value` is implicit, so a 1-element overload would silently
     *        wrap the array as a single-element array.
     */
    template <class T0, class T1, class... Rest>
        requires(std::constructible_from<comms::md::Value, T0 &&> &&
                 std::constructible_from<comms::md::Value, T1 &&> &&
                 (std::constructible_from<comms::md::Value, Rest &&> && ...))
    ArrayCell(T0&& a, T1&& b, Rest&&... rest)
        : base_t(comms::md::Array{comms::md::Value(std::forward<T0>(a)),
                                  comms::md::Value(std::forward<T1>(b)),
                                  comms::md::Value(std::forward<Rest>(rest))...}) {}

    static constexpr std::string_view kind_id = "md:a";

    [[nodiscard]] std::string to_string() const override {
        std::ostringstream os;
        os << this->value;
        return os.str();
    }

    static cell_t from_json(json_t const& j, ParcelRegistry const&) {
        return base_t::from_json_strict(j);
    }

    static cell_type_descriptor_t descriptor() {
        static const auto d = std::make_shared<SimpleCellTypeDescriptor<ArrayCell>>(DisplayInfo{
            .name = "md::Array",
            .description = "Heterogeneous vector of md::Value (md::Array).",
            .icon = comms::Icons::mdi::code_brackets,
            .color = comms::Colors::mui::green[600],
        });
        return d;
    }
};

}  // namespace parcel

PARCEL_DEFAULT_CELL(parcel::ValueCell);
PARCEL_DEFAULT_CELL(parcel::ObjectCell);
PARCEL_DEFAULT_CELL(parcel::ArrayCell);
