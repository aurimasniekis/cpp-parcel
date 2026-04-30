cmake_minimum_required(VERSION 3.25)

project(mylib_optional_adapter
        VERSION 0.1.0
        DESCRIPTION "Standalone downstream demo: ship an optional parcel adapter"
        LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# ----------------------------------------------------------------------------
# This option is ONLY for this example, so we can demonstrate both modes from
# one tree. Real downstream libraries don't need it: mylib/parcel.h gates on
# `#if __has_include(<parcel/parcel.h>)`, so the only thing the build system
# decides is whether parcel ends up on the include path.
# ----------------------------------------------------------------------------
option(MYLIB_WITH_PARCEL "Build mylib's parcel adapter" ON)

if(MYLIB_WITH_PARCEL)
    include(FetchContent)

    # Local-validation tip: while testing this snippet from inside the parcel
    # repo, point CMake at the in-tree source tree to skip the network:
    #
    #   cmake -S examples/consumers/optional_adapter \
    #         -B /tmp/optional-adapter-on \
    #         -DFETCHCONTENT_SOURCE_DIR_CPP-PARCEL=$PWD
    FetchContent_Declare(
            cpp-parcel
            GIT_REPOSITORY https://github.com/aurimasniekis/cpp-parcel.git
            GIT_TAG main
            GIT_SHALLOW TRUE
    )
    FetchContent_MakeAvailable(cpp-parcel)
endif()

add_executable(consumer main.cpp)
target_include_directories(consumer PRIVATE include)
if(MYLIB_WITH_PARCEL)
    target_link_libraries(consumer PRIVATE parcel::parcel)
endif()
