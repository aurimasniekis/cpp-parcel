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
    URL      https://github.com/aurimasniekis/cpp-commons/archive/refs/tags/v0.1.3.tar.gz
    URL_HASH SHA256=2f5615ac96a1a1dddda5424ed75c0d1a0142f115f215502562f479ef138fc30d
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
    FIND_PACKAGE_ARGS 0.1
)
FetchContent_MakeAvailable(commons)

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
