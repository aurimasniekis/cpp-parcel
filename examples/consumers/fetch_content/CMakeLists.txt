cmake_minimum_required(VERSION 3.25)

project(parcel_consumer_fetch_content
        VERSION 0.1.0
        DESCRIPTION "Standalone downstream demo: pull parcel via FetchContent"
        LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(FetchContent)

# Pin a published release tarball + its SHA256. Reproducible, no git required
# at configure time, fastest to fetch on CI. To compute SHA256 yourself:
#
#   curl -sL https://github.com/aurimasniekis/cpp-parcel/archive/refs/tags/v0.2.2.tar.gz \
#     | shasum -a 256
FetchContent_Declare(
        cpp-parcel
        URL      https://github.com/aurimasniekis/cpp-parcel/archive/refs/tags/v0.2.2.tar.gz
        URL_HASH SHA256=7e4277f9f57b5bbea815f029c086e194d6ceedb6fa370a358ef0c9e22e1d30e5
)

# Fallback: pull by git tag. Less reproducible than the tarball form (the tag
# could be re-pointed; checksum drift goes unnoticed). Uncomment if you need
# to track an unreleased commit.
#
# FetchContent_Declare(
#         cpp-parcel
#         GIT_REPOSITORY https://github.com/aurimasniekis/cpp-parcel.git
#         GIT_TAG v0.2.2
#         GIT_SHALLOW TRUE
# )

# Local-validation tip: to test this snippet against an in-tree checkout,
# point CMake at the source tree:
#
#   cmake -S examples/consumers/fetch_content \
#         -B /tmp/parcel-consumer-build \
#         -DCMAKE_BUILD_TYPE=Release \
#         -DFETCHCONTENT_SOURCE_DIR_CPP-PARCEL=$PWD
#
# That makes FetchContent skip the network and use $PWD instead.

FetchContent_MakeAvailable(cpp-parcel)

add_executable(consumer main.cpp)
target_link_libraries(consumer PRIVATE parcel::parcel)
