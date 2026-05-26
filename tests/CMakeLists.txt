include(GoogleTest)

add_executable(parcel_tests
    test_parcel.cpp
    test_primitive.cpp
    test_list.cpp
    test_map.cpp
    test_struct.cpp
    test_union.cpp
    test_registry.cpp
    test_meta.cpp
    test_compare.cpp
    test_cell_helpers.cpp
    test_ergonomics.cpp
    test_std_interop.cpp
    test_walk.cpp
    test_commons.cpp
)

target_link_libraries(parcel_tests
    PRIVATE
        parcel::parcel
        GTest::gtest_main
)

parcel_set_warnings(parcel_tests PRIVATE)
parcel_enable_sanitizers(parcel_tests)
parcel_enable_coverage(parcel_tests)

# libstdc++ 14's <generator> trips -Wnull-dereference at -O3 inside its own
# coroutine machinery (`__builtin_coro_promise`, `__h.promise()._M_nest…`).
# The diagnostic fires at the point of instantiation, so the suppression has
# to live on the TUs that actually pull in the generator.
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    set_source_files_properties(test_walk.cpp test_std_interop.cpp
        PROPERTIES COMPILE_OPTIONS -Wno-null-dereference)
    # libstdc++ 14's <variant> move-ctor trips -Wmaybe-uninitialized at -O3
    # when a struct containing std::variant is move-constructed into a
    # BaseCell — a false positive in libstdc++'s own union-storage init.
    set_source_files_properties(test_registry.cpp test_union.cpp
        PROPERTIES COMPILE_OPTIONS -Wno-maybe-uninitialized)
endif()

gtest_discover_tests(parcel_tests)
