// Demo consumer of "mylib". Compiles in two modes:
//   - With parcel on the include path (default): exercises mylib::ColorCell.
//   - Without parcel: prints the plain mylib::Color and proves mylib still
//     works on its own.
//
// The mode is decided by mylib/parcel.h's __has_include gate; this file
// just branches on the resulting MYLIB_HAS_PARCEL macro.

#include <iostream>

#include <mylib/color.h>
#include <mylib/parcel.h>

int main() {
    mylib::Color c{0xff, 0x99, 0x00};
#if MYLIB_HAS_PARCEL
    auto cell = mylib::ColorCell::of(c);
    std::cout << "with parcel: " << cell->to_json().dump() << '\n';
#else
    std::cout << "without parcel: rgb("  //
              << +c.r << ',' << +c.g << ',' << +c.b << ")\n";
#endif
    return 0;
}
