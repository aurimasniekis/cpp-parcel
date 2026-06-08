include_guard(GLOBAL)
include(FetchContent)

# nlohmann/json v3.12.0 — GitHub release tarball, no git required.
set(JSON_BuildTests OFF CACHE INTERNAL "")
set(JSON_Install    OFF CACHE INTERNAL "")
FetchContent_Declare(
    nlohmann_json
    URL      https://github.com/nlohmann/json/archive/refs/tags/v3.12.0.tar.gz
    URL_HASH SHA256=4b92eb0c06d10683f7447ce9406cb97cd4b453be18d7279320f7b2f025c10187
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
    FIND_PACKAGE_ARGS 3.12.0
)
FetchContent_MakeAvailable(nlohmann_json)

# aurimasniekis/cpp-commons — header-only vocabulary types (Color, Icon,
# DisplayInfo, Flag/FlagSet, FixedString). Declared after nlohmann/json so the
# commons headers see <nlohmann/json.hpp> on the include path and self-enable
# their JSON hooks (COMMONS_WITH_NLOHMANN_JSON auto-detects via __has_include);
# we leave COMMONS_WITH_NLOHMANN_JSON OFF so commons does not fetch its own copy.
FetchContent_Declare(
    commons
    URL      https://github.com/aurimasniekis/cpp-commons/archive/refs/tags/v0.1.6.tar.gz
    URL_HASH SHA256=04047f92aca576555346b17f628e516b9fe5caab1dbdc8a853245a3a83f27be0
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
    FIND_PACKAGE_ARGS 0.1
)
FetchContent_MakeAvailable(commons)

# aurimasniekis/cpp-ulid — optional, only when PARCEL_WITH_ULID is ON. Linking
# ulid::ulid lets <parcel/ext/ulid.h>'s UlidCell compile and (because commons
# auto-detects <ulid/ulid.h>) enables commons' own COMMONS_WITH_ULID too. Same
# coordinates commons fetches.
if(PARCEL_WITH_ULID)
    FetchContent_Declare(
        ulid
        URL      https://github.com/aurimasniekis/cpp-ulid/archive/refs/tags/v1.0.0.tar.gz
        URL_HASH SHA256=2c7cb1b67be889fd3a783aa1188b72545e1becc6f36cb25c7a7c6361177f1515
        DOWNLOAD_EXTRACT_TIMESTAMP TRUE
        FIND_PACKAGE_ARGS 1.0.0
    )
    FetchContent_MakeAvailable(ulid)
endif()

if(PARCEL_BUILD_TESTS)
    set(INSTALL_GTEST OFF CACHE INTERNAL "")
    set(BUILD_GMOCK   OFF CACHE INTERNAL "")
    FetchContent_Declare(
        googletest
        URL      https://github.com/google/googletest/archive/refs/tags/v1.15.2.tar.gz
        URL_HASH SHA256=7b42b4d6ed48810c5362c265a17faebe90dc2373c885e5216439d37927f02926
        DOWNLOAD_EXTRACT_TIMESTAMP TRUE
        FIND_PACKAGE_ARGS NAMES GTest
    )
    FetchContent_MakeAvailable(googletest)
endif()
