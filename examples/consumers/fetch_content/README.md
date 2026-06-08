# consumer — `FetchContent` integration demo

This is a **standalone downstream project** that depends on parcel. It is not
built by the parent project; it exists to document the integration story
end-to-end.

The `CMakeLists.txt` in this directory shows two ways to pull parcel:

1. **Tarball + `URL_HASH` (active, preferred for production).** Reproducible —
   the build pins both a tag *and* a checksum. The current pin is `v0.2.3`
   with SHA256
   `<REPLACE_WITH_RELEASE_TARBALL_SHA256>`.
2. **Git tag (commented fallback).** Useful if you need to track an unreleased
   commit. Less reproducible: the tag could be re-pointed and you would not
   notice.

## Run it

```sh
cmake -S examples/consumers/fetch_content -B /tmp/parcel-consumer-build
cmake --build /tmp/parcel-consumer-build
/tmp/parcel-consumer-build/consumer
```

Expected output (something close to):

```
parcel version: 0.2.3
wire payload  : {"k":"i32","v":42}
restored kind : i32
restored value: 42
```

## Validate against a local checkout

To exercise the snippet against the in-tree source tree (skipping the network
fetch), override the source directory at configure time —
`FETCHCONTENT_SOURCE_DIR_<NAME>` makes `FetchContent` use the path you
provide:

```sh
# from the repo root
cmake -S examples/consumers/fetch_content \
      -B /tmp/parcel-consumer-build \
      -DCMAKE_BUILD_TYPE=Release \
      -DFETCHCONTENT_SOURCE_DIR_CPP-PARCEL=$PWD
cmake --build /tmp/parcel-consumer-build
/tmp/parcel-consumer-build/consumer
```
