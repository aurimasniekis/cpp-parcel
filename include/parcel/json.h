#pragma once

/**
 * @file json.h
 * @brief `nlohmann::json` typedef shared across cell types.
 *
 * Provides the `parcel::json_t` alias and pulls in commons' nlohmann/json
 * integration (`commons/json.hpp`), which supplies the ADL serializers parcel
 * relies on.
 */

#include <nlohmann/json.hpp>

#include <commons/json.hpp>

namespace parcel {

/** @brief Project-wide alias for `nlohmann::json`. */
using json_t = nlohmann::json;

}  // namespace parcel
