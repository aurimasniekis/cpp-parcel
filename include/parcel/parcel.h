#pragma once

/**
 * @file parcel.h
 * @brief Umbrella header — pulls in every public parcel module and defines version constants.
 *
 * Include this single header to get the full public API: cells, descriptors,
 * the registry, the builtins helpers, and the field-type defaults. See the
 * README for a narrative overview.
 */

#include <parcel/builtins.h>
#include <parcel/cell.h>
#include <parcel/common.h>
#include <parcel/commons.h>
#include <parcel/defaults.h>
#include <parcel/descriptor.h>
#include <parcel/error.h>
#include <parcel/format.h>
#include <parcel/json_io.h>
#include <parcel/list.h>
#include <parcel/map.h>
#include <parcel/primitive.h>
#include <parcel/registry.h>
#include <parcel/struct.h>
#include <parcel/union.h>
#include <parcel/version.h>
#include <parcel/walk.h>

#include <string_view>

namespace parcel {

/** @brief Library version as a dotted `MAJOR.MINOR.PATCH` string. */
inline constexpr std::string_view version = PARCEL_VERSION_STRING;

/** @brief Major version component. */
inline constexpr int version_major = PARCEL_VERSION_MAJOR;
/** @brief Minor version component. */
inline constexpr int version_minor = PARCEL_VERSION_MINOR;
/** @brief Patch version component. */
inline constexpr int version_patch = PARCEL_VERSION_PATCH;

}  // namespace parcel
