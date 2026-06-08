cmake_minimum_required(VERSION 3.25)

project(parcel
    VERSION     0.2.3
    DESCRIPTION "Wrappable, wire-transferable C++23 value system with JSON serialization"
    LANGUAGES   CXX
)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

set(CMAKE_CXX_STANDARD          23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS        OFF)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Debug CACHE STRING "Build type" FORCE)
    set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS
        Debug Release RelWithDebInfo MinSizeRel)
endif()

option(PARCEL_BUILD_TESTS         "Build parcel tests"                  ${PROJECT_IS_TOP_LEVEL})
option(PARCEL_BUILD_EXAMPLES      "Build parcel examples"               ${PROJECT_IS_TOP_LEVEL})
option(PARCEL_BUILD_DOCS          "Build Doxygen API documentation"     OFF)
option(PARCEL_ENABLE_CLANG_TIDY   "Run clang-tidy during build"         OFF)
option(PARCEL_ENABLE_SANITIZERS   "Build with ASan + UBSan (Debug)"     OFF)
option(PARCEL_ENABLE_COVERAGE     "Build with Clang source-based coverage" OFF)
option(PARCEL_WARNINGS_AS_ERRORS  "Treat compiler warnings as errors"   ${PROJECT_IS_TOP_LEVEL})
option(PARCEL_INSTALL             "Generate install rules for parcel"   ${PROJECT_IS_TOP_LEVEL})
option(PARCEL_WITH_ULID           "Enable the ULID cell (fetches aurimasniekis/cpp-ulid)" OFF)

include(CompilerWarnings)
include(Sanitizers)
include(Coverage)
include(Dependencies)

# Generate parcel/version.h from the project version (single source of truth).
configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/include/parcel/version.h.in"
    "${CMAKE_CURRENT_BINARY_DIR}/include/parcel/version.h"
    @ONLY
)

add_library(parcel INTERFACE)
add_library(parcel::parcel ALIAS parcel)

target_include_directories(parcel
    INTERFACE
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

target_compile_features(parcel INTERFACE cxx_std_23)

target_link_libraries(parcel INTERFACE nlohmann_json::nlohmann_json commons::commons)

# Optional ULID cell. Linking ulid also auto-enables commons' COMMONS_WITH_ULID
# (commons detects <ulid/ulid.h> on the include path).
if(PARCEL_WITH_ULID AND TARGET ulid::ulid)
    target_link_libraries(parcel INTERFACE ulid::ulid)
    target_compile_definitions(parcel INTERFACE PARCEL_HAS_ULID=1)
endif()

if(PARCEL_ENABLE_CLANG_TIDY)
    find_program(PARCEL_CLANG_TIDY_EXE NAMES clang-tidy REQUIRED)
    set(_parcel_tidy_cmd
        "${PARCEL_CLANG_TIDY_EXE}"
        "--config-file=${CMAKE_CURRENT_SOURCE_DIR}/.clang-tidy"
    )
    # clang-tidy parses with clang's frontend, which does its OWN libstdc++
    # detection — independent of CMAKE_CXX_COMPILER. With multiple gcc
    # versions installed it can land on the wrong one (e.g. Ubuntu 24.04
    # ships gcc-13 but we compile with gcc-14), and an older libstdc++ may
    # ship `<expected>` as a header without exposing `std::expected`. Pin
    # tidy to the same gcc install the real compile uses.
    if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
        execute_process(
            COMMAND "${CMAKE_CXX_COMPILER}" -print-search-dirs
            OUTPUT_VARIABLE _parcel_gcc_search_dirs
            OUTPUT_STRIP_TRAILING_WHITESPACE
        )
        if(_parcel_gcc_search_dirs MATCHES "install: ([^\n]+)")
            list(APPEND _parcel_tidy_cmd
                "--extra-arg-before=--gcc-install-dir=${CMAKE_MATCH_1}")
            message(STATUS
                "parcel: pinning clang-tidy to gcc install dir ${CMAKE_MATCH_1}")
        endif()
    endif()
    set(CMAKE_CXX_CLANG_TIDY "${_parcel_tidy_cmd}")
    message(STATUS "parcel: clang-tidy enabled (${PARCEL_CLANG_TIDY_EXE})")
endif()

if(PARCEL_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

if(PARCEL_BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()

if(PARCEL_BUILD_DOCS)
    find_package(Doxygen REQUIRED)
    set(PARCEL_DOXYGEN_OUTPUT_DIR "${CMAKE_BINARY_DIR}/docs")
    configure_file(
        "${CMAKE_CURRENT_SOURCE_DIR}/docs/Doxyfile.in"
        "${CMAKE_CURRENT_BINARY_DIR}/Doxyfile"
        @ONLY
    )
    add_custom_target(parcel_docs
        COMMAND ${DOXYGEN_EXECUTABLE} "${CMAKE_CURRENT_BINARY_DIR}/Doxyfile"
        WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
        COMMENT "Generating Doxygen documentation"
        VERBATIM
    )
endif()

if(PARCEL_INSTALL AND (DEFINED nlohmann_json_SOURCE_DIR OR DEFINED commons_SOURCE_DIR OR DEFINED ulid_SOURCE_DIR))
    message(STATUS
        "parcel: PARCEL_INSTALL disabled — a dependency (nlohmann_json and/or commons) "
        "was built via FetchContent and cannot be re-exported. Install the dependency "
        "from a system package (or use find_package) to re-enable parcel's install rules."
    )
    set(PARCEL_INSTALL OFF)
endif()

if(PARCEL_INSTALL)
    include(GNUInstallDirs)
    include(CMakePackageConfigHelpers)

    install(TARGETS parcel
        EXPORT  parcelTargets
        INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
    )

    install(DIRECTORY include/parcel
        DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
        FILES_MATCHING PATTERN "*.h"
    )

    install(FILES
        "${CMAKE_CURRENT_BINARY_DIR}/include/parcel/version.h"
        DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/parcel
    )

    install(EXPORT parcelTargets
        FILE        parcelTargets.cmake
        NAMESPACE   parcel::
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/parcel
    )

    write_basic_package_version_file(
        "${CMAKE_CURRENT_BINARY_DIR}/parcelConfigVersion.cmake"
        VERSION       ${PROJECT_VERSION}
        COMPATIBILITY SameMajorVersion
        ARCH_INDEPENDENT
    )

    configure_package_config_file(
        "${CMAKE_CURRENT_SOURCE_DIR}/cmake/parcelConfig.cmake.in"
        "${CMAKE_CURRENT_BINARY_DIR}/parcelConfig.cmake"
        INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/parcel
    )

    install(FILES
        "${CMAKE_CURRENT_BINARY_DIR}/parcelConfig.cmake"
        "${CMAKE_CURRENT_BINARY_DIR}/parcelConfigVersion.cmake"
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/parcel
    )
endif()
