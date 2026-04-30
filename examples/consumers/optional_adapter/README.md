# consumer — optional parcel adapter in a downstream library

This is a **standalone downstream project** that plays the role of a small
library `mylib` shipping an *optional* parcel adapter. It exists to document
the canonical pattern: one set of headers that compile fine without parcel,
and grow `Cell` wrappers automatically when parcel is on the consumer's
include path.

## The pattern

There are exactly two headers in `include/mylib/`:

- `color.h` — the plain mylib type. Zero parcel dependency.
- `parcel.h` — the optional adapter. Always safe to include; gated on
  `#if __has_include(<parcel/parcel.h>)`. When parcel is found, it grows a
  `mylib::ColorCell` wrapper and `#define`s `MYLIB_HAS_PARCEL 1`. When it
  isn't, it `#define`s `MYLIB_HAS_PARCEL 0` and emits nothing else.

Conventions encoded by this example:

1. **Adapter header lives at `<libname>/parcel.h`** — discoverable, follows
   the same naming pattern other ecosystems use (e.g. `fmt/ostream.h`).
2. **`__has_include(<parcel/parcel.h>)` is the only gate.** No CMake option,
   no preprocessor define from the build system. Consumers that have parcel
   on their include path get the adapter; consumers that don't get a no-op
   header.
3. **Always `#define MYLIB_HAS_PARCEL` to 0 or 1**, never leave it
   undefined. Consumer code can then write `#if MYLIB_HAS_PARCEL` without
   tripping `-Wundef`.
4. **Wire kind is namespaced** (`"mylib.color"`, not `"color"`) so multiple
   downstream libraries can coexist in one `ParcelRegistry`.
5. **`PARCEL_DEFAULT_CELL` is invoked at namespace scope inside the gated
   block.** No new parcel-side helper required — it is the existing macro
   from `<parcel/defaults.h>`.

## Run it — both modes

The `MYLIB_WITH_PARCEL` CMake option below is **only for this example**, so
we can flip parcel on/off from one tree. Real downstream libraries don't
need it; `__has_include` does the work.

### Mode 1: with parcel adapter compiled in (default)

From the parcel repo root:

```sh
cmake -S examples/consumers/optional_adapter \
      -B /tmp/optional-adapter-on \
      -DFETCHCONTENT_SOURCE_DIR_CPP-PARCEL=$PWD
cmake --build /tmp/optional-adapter-on
/tmp/optional-adapter-on/consumer
# with parcel: {"k":"s:mylib.color","v":{"b":{"k":"u8","v":0},"g":{"k":"u8","v":153},"r":{"k":"u8","v":255}}}
```

`FETCHCONTENT_SOURCE_DIR_CPP-PARCEL=$PWD` makes `FetchContent` skip the
network and use the in-tree source. Drop it once a release tag exists.

### Mode 2: parcel adapter disabled

This proves mylib still compiles and works without parcel anywhere on the
include path:

```sh
cmake -S examples/consumers/optional_adapter \
      -B /tmp/optional-adapter-off \
      -DMYLIB_WITH_PARCEL=OFF
cmake --build /tmp/optional-adapter-off
/tmp/optional-adapter-off/consumer
# without parcel: rgb(255,153,0)
```
