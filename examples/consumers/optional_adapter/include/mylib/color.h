// mylib/color.h — plain mylib type, zero parcel dependency.
#pragma once

#include <cstdint>

namespace mylib {

struct Color {
    std::uint8_t r{};
    std::uint8_t g{};
    std::uint8_t b{};
};

}  // namespace mylib
