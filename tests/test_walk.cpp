#include <parcel/parcel.h>
#include <parcel/walk.h>

#include <gtest/gtest.h>

#include <map>
#include <string>
#include <utility>
#include <vector>

// Focused tests for `parcel::walk` that complement the broader Walk.* suite in
// test_std_interop.cpp. Two contracts pinned here:
//   * The lazy `walk` generator yields the *same* sequence as `walk_to_vector`
//     for a non-trivial nested tree — guards against any future drift between
//     the eager oracle and the coroutine implementation.
//   * `UnionCell` is treated as a leaf (matching `StructCell`).

namespace {

using IntStr = parcel::UnionCell<parcel::I32Cell, parcel::StringCell>;

}  // namespace

#if PARCEL_HAS_GENERATOR
TEST(Walk, generator_matches_walk_to_vector_for_nested_tree) {
    auto inner_list = parcel::ListCell::of(std::vector<parcel::cell_t>{
        parcel::I32Cell::of(10),
        parcel::I32Cell::of(20),
    });
    auto inner_map = parcel::MapCell::of(std::map<std::string, parcel::cell_t>{
        {"nested", inner_list},
        {"leaf", parcel::StringCell::of("hi")},
    });
    auto root = parcel::ListCell::of(std::vector<parcel::cell_t>{
        parcel::BoolCell::of(true),
        inner_map,
    });

    const auto eager = parcel::walk_to_vector(root);

    std::vector<parcel::WalkEntry> lazy;
    for (auto entry : parcel::walk(root)) {
        lazy.push_back(std::move(entry));
    }

    ASSERT_EQ(lazy.size(), eager.size());
    for (std::size_t i = 0; i < eager.size(); ++i) {
        EXPECT_EQ(lazy[i].path, eager[i].path) << "at index " << i;
        EXPECT_EQ(lazy[i].cell, eager[i].cell) << "at index " << i;
    }
}
#endif

TEST(Walk, vector_treats_union_as_leaf) {
    auto u = IntStr::of(std::int32_t{5});
    auto root = parcel::ListCell::of(std::vector<parcel::cell_t>{u});

    const auto entries = parcel::walk_to_vector(root);
    ASSERT_EQ(entries.size(), 2U);
    EXPECT_EQ(entries[0].path, "");
    EXPECT_EQ(entries[1].path, "/0");
    EXPECT_EQ(entries[1].cell, u);
}
