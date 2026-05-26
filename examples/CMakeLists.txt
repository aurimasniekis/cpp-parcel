function(parcel_add_example target src)
    add_executable(${target} ${src})
    target_link_libraries(${target} PRIVATE parcel::parcel)
    parcel_set_warnings(${target} PRIVATE)
    parcel_enable_sanitizers(${target})
endfunction()

parcel_add_example(parcel_hello                  hello.cpp)
parcel_add_example(parcel_primitive_demo         primitive_demo.cpp)
parcel_add_example(parcel_list_demo              list_demo.cpp)
parcel_add_example(parcel_map_demo               map_demo.cpp)
parcel_add_example(parcel_struct_demo            struct_demo.cpp)
parcel_add_example(parcel_struct_extras_demo     struct_extras_demo.cpp)
parcel_add_example(parcel_union_demo             union_demo.cpp)
parcel_add_example(parcel_registry_demo          registry_demo.cpp)
parcel_add_example(parcel_display_info_demo      display_info_demo.cpp)
parcel_add_example(parcel_commons_demo           commons_demo.cpp)
parcel_add_example(parcel_error_handling_demo    error_handling_demo.cpp)
parcel_add_example(parcel_defaults_demo          defaults_demo.cpp)
parcel_add_example(parcel_custom_primitive_demo  custom_primitive_demo.cpp)
parcel_add_example(parcel_custom_struct_demo     custom_struct_demo.cpp)
parcel_add_example(parcel_nested_struct_demo     nested_struct_demo.cpp)
parcel_add_example(parcel_struct_inheritance_demo struct_inheritance_demo.cpp)
parcel_add_example(parcel_intrusive_struct_demo  intrusive_struct_demo.cpp)
parcel_add_example(parcel_cell_handle_demo       cell_handle_demo.cpp)
parcel_add_example(parcel_format_io_demo         format_io_demo.cpp)
parcel_add_example(parcel_ergonomics_demo        ergonomics_demo.cpp)
parcel_add_example(parcel_union_visit_demo       union_visit_demo.cpp)
parcel_add_example(parcel_std_interop_demo       std_interop_demo.cpp)

# libstdc++ 14's <generator> trips -Wnull-dereference at -O3 inside its own
# coroutine machinery; suppress for the demo that pulls in walk's generator.
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    set_source_files_properties(cell_handle_demo.cpp
        PROPERTIES COMPILE_OPTIONS -Wno-null-dereference)
endif()

# libstdc++ 14's <variant> move-ctor trips -Wmaybe-uninitialized at -O3 when
# a struct containing std::variant is move-constructed into a BaseCell — a
# false positive in libstdc++'s own union-storage init.
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    set_source_files_properties(
            defaults_demo.cpp
            custom_struct_demo.cpp
            union_demo.cpp
        PROPERTIES COMPILE_OPTIONS -Wno-maybe-uninitialized)
endif()
