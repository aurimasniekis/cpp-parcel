#pragma once

/**
 * @file meta.h
 * @brief `descriptor::MetaInfo` — optional name/description/icon/color carried by every cell.
 *
 * Every `ICell` may carry an optional `MetaInfo` payload, serialized under
 * the JSON key `"d"`. This is purely descriptive metadata for UI/tooling
 * use and never affects the wire identity (`"k"`) or value (`"v"`) of a
 * cell. See the README "Meta payload" section.
 */

#include <parcel/common.h>

#include <optional>
#include <string>

namespace parcel::descriptor {

/**
 * @brief Descriptive metadata attached to a cell or descriptor.
 *
 * Carried inline on every `ICell` via `ICell::meta()` and used as the public
 * face of cell-type descriptors. Every field is optional; absent fields are
 * omitted from the JSON object on serialization.
 */
struct MetaInfo {
    /** @brief Human-readable name. */
    std::optional<std::string> name = std::nullopt;
    /** @brief Long-form description. */
    std::optional<std::string> description = std::nullopt;
    /** @brief Icon identifier (interpretation up to the consumer). */
    std::optional<std::string> icon = std::nullopt;
    /** @brief Color identifier (interpretation up to the consumer). */
    std::optional<std::string> color = std::nullopt;

    /**
     * @brief Serialize to a JSON object with only the present fields.
     * @return JSON object containing each non-null field.
     */
    [[nodiscard]] json_t to_json() const {
        json_t j = json_t::object();
        if (name) {
            j["name"] = *name;
        }
        if (description) {
            j["description"] = *description;
        }
        if (icon) {
            j["icon"] = *icon;
        }
        if (color) {
            j["color"] = *color;
        }
        return j;
    }
};

/**
 * @brief ADL deserializer for `MetaInfo`.
 *
 * Tolerant — any subset of the four optional keys may be present. Absent
 * keys leave the corresponding optional empty.
 *
 * @param j    Input JSON object.
 * @param meta Result populated in place.
 */
inline void from_json(const json_t& j, MetaInfo& meta) {
    if (j.contains("name")) {
        j.at("name").get_to(meta.name);
    }
    if (j.contains("description")) {
        j.at("description").get_to(meta.description);
    }
    if (j.contains("icon")) {
        j.at("icon").get_to(meta.icon);
    }
    if (j.contains("color")) {
        j.at("color").get_to(meta.color);
    }
}

}  // namespace parcel::descriptor
