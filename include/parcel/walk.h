#pragma once

/**
 * @file walk.h
 * @brief `std::generator`-based walk over a cell tree.
 *
 * Recurses into `ListCell` and `MapCell` depth-first, yielding each
 * visited cell paired with a JSON-pointer-style path (RFC 6901, with
 * `~` and `/` escaped to `~0` / `~1`). Every other cell — including
 * `StructCell` and `UnionCell` — is treated as a leaf.
 *
 * Requires C++23 `<generator>`. When the standard header is unavailable
 * (libc++ shipped with older Apple Clang, for example) this header
 * exposes a fallback `walk_to_vector` that materializes the same nodes
 * eagerly into a vector.
 */

#include <parcel/cell.h>
#include <parcel/list.h>
#include <parcel/map.h>
#include <parcel/struct.h>
#include <parcel/union.h>

#include <string>
#include <utility>
#include <vector>

#if defined(__cpp_lib_generator) && __cpp_lib_generator >= 202207L
#include <generator>
#define PARCEL_HAS_GENERATOR 1
#else
#define PARCEL_HAS_GENERATOR 0
#endif

namespace parcel {

/** @brief One node yielded by a tree walk: `(json-pointer-path, cell)`. */
struct WalkEntry {
    /** @brief JSON-pointer-style path to the cell (`""` for the root). */
    std::string path;
    /** @brief The cell at that path. */
    cell_t cell;
};

namespace detail {

// Escape a path component per RFC 6901: '~' -> "~0", '/' -> "~1". Order
// matters — '~' must be substituted before '/' so the substituted '/' is
// not double-escaped on the next pass.
inline std::string jp_escape(const std::string_view k) {
    std::string out;
    out.reserve(k.size());
    for (const char c : k) {
        if (c == '~') {
            out += "~0";
        } else if (c == '/') {
            out += "~1";
        } else {
            out += c;
        }
    }
    return out;
}

inline void walk_collect(cell_t const& root, const std::string& path, std::vector<WalkEntry>& out) {
    if (!root) {
        return;
    }
    out.push_back({path, root});

    if (auto const* list = dynamic_cast<ListCell const*>(root.get()); list != nullptr) {
        std::size_t i = 0;
        for (auto const& el : list->value) {
            std::string sub = path;
            sub += '/';
            sub += std::to_string(i);
            walk_collect(el, sub, out);
            ++i;
        }
        return;
    }

    if (auto const* map = dynamic_cast<MapCell const*>(root.get()); map != nullptr) {
        for (auto const& [k, v] : map->value) {
            std::string sub = path;
            sub += '/';
            sub += jp_escape(k);
            walk_collect(v, sub, out);
        }
        return;
    }
}

}  // namespace detail

/**
 * @brief Eagerly collect every cell in @p root's tree, depth-first.
 *
 * Independent of the `<generator>` header so it always compiles. Prefer
 * `walk` (below) when targeting a toolchain with C++23 generator support.
 *
 * @param root Root cell.
 * @return Vector of `(path, cell)` entries.
 */
[[nodiscard]] inline std::vector<WalkEntry> walk_to_vector(cell_t const& root) {
    std::vector<WalkEntry> out;
    detail::walk_collect(root, "", out);
    return out;
}

#if PARCEL_HAS_GENERATOR
namespace detail {

// Recursive coroutine helper for `walk`. Mirrors `walk_collect`'s contract
// but yields each node lazily as the consumer pulls — no full materialization.
inline std::generator<WalkEntry> walk_node(cell_t root, std::string path) {
    if (!root) {
        co_return;
    }
    co_yield WalkEntry{path, root};

    if (auto const* list = dynamic_cast<ListCell const*>(root.get()); list != nullptr) {
        std::size_t i = 0;
        for (auto const& el : list->value) {
            std::string sub = path;
            sub += '/';
            sub += std::to_string(i);
            for (auto&& entry : walk_node(el, std::move(sub))) {
                co_yield std::move(entry);
            }
            ++i;
        }
        co_return;
    }

    if (auto const* map = dynamic_cast<MapCell const*>(root.get()); map != nullptr) {
        for (auto const& [k, v] : map->value) {
            std::string sub = path;
            sub += '/';
            sub += jp_escape(k);
            for (auto&& entry : walk_node(v, std::move(sub))) {
                co_yield std::move(entry);
            }
        }
        co_return;
    }
}

}  // namespace detail

/**
 * @brief Coroutine generator that yields every cell in @p root's tree.
 *
 * Walks `ListCell` and `MapCell` recursively; treats every other cell as
 * a leaf. Order is depth-first, root before children.
 *
 * Truly lazy: each pull from the generator advances exactly one node — the
 * tree is never fully materialized into a vector. For an eager equivalent,
 * see `walk_to_vector`.
 *
 * @param root Root cell.
 * @return `std::generator<WalkEntry>` — empty when @p root is null.
 */
inline std::generator<WalkEntry> walk(cell_t root) {
    return detail::walk_node(std::move(root), "");
}
#endif

}  // namespace parcel
