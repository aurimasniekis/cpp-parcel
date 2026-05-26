// mylib/parcel.h — optional parcel adapter for mylib types.
//
// Including this header is always safe. If parcel is on the include path
// (i.e. <parcel/parcel.h> resolves), it grows a `mylib::ColorCell` wrapper
// and sets MYLIB_HAS_PARCEL=1. Otherwise it sets MYLIB_HAS_PARCEL=0 and
// emits no other declarations.
#pragma once

#if __has_include(<parcel/parcel.h>)
#include <parcel/parcel.h>

#include <mylib/color.h>

namespace mylib {

class ColorCell : public parcel::StructCell<ColorCell, Color, "mylib.color"> {
public:
    using StructCell::StructCell;

    static parcel::DisplayInfo meta_info() {
        return {.name = "Color"};
    }

    static auto field_descriptors() {
        return parcel::FieldsBuilder<Color>{}
            .field<&Color::r>("r")
            .field<&Color::g>("g")
            .field<&Color::b>("b")
            .build();
    }
};

}  // namespace mylib

PARCEL_DEFAULT_CELL(mylib::ColorCell);

#define MYLIB_HAS_PARCEL 1
#else
#define MYLIB_HAS_PARCEL 0
#endif
