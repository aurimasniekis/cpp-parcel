gtest_dep      = dependency('gtest',      fallback: ['gtest', 'gtest_dep'])
gtest_main_dep = dependency('gtest_main', fallback: ['gtest', 'gtest_main_dep'])

parcel_tests = executable('parcel_tests',
    sources: [
        'test_parcel.cpp',
        'test_primitive.cpp',
        'test_list.cpp',
        'test_map.cpp',
        'test_struct.cpp',
        'test_union.cpp',
        'test_registry.cpp',
        'test_meta.cpp',
        'test_compare.cpp',
        'test_cell_helpers.cpp',
        'test_ergonomics.cpp',
        'test_std_interop.cpp',
        'test_walk.cpp',
    ],
    dependencies: [parcel_dep, gtest_dep, gtest_main_dep],
)

test('parcel', parcel_tests, protocol: 'gtest')
