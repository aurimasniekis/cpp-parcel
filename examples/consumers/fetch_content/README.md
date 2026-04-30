# consumer — `FetchContent` integration demo

This is a **standalone downstream project** that depends on parcel. It is not
built by the parent project; it exists to document the integration story
end-to-end.

The `CMakeLists.txt` in this directory shows two ways to pull parcel:

1. **Tarball + `URL_HASH` (preferred for production).** Reproducible — the
   build pins both a tag *and* a checksum. The block is commented out today
   because no release tarball exists yet; uncomment it and replace the
   `v0.1.0` / `<REPLACE_WITH_RELEASE_TARBALL_SHA256>` placeholders once a
   release is cut.
2. **Git tag (fallback).** Active in this file because it works today without
   a published release. Less reproducible: the tag could be re-pointed, and
   you would not notice.

## Run it before a release exists

The repo has no published tag yet, so pulling from GitHub will fail until one
is cut. To validate the snippet against the local tree, override the source
directory at configure time — `FETCHCONTENT_SOURCE_DIR_<NAME>` makes
`FetchContent` skip the network and use the path you provide:

```sh
# from the repo root
cmake -S examples/consumers/fetch_content \
      -B /tmp/parcel-consumer-build \
      -DCMAKE_BUILD_TYPE=Release \
      -DFETCHCONTENT_SOURCE_DIR_CPP-PARCEL=$PWD
cmake --build /tmp/parcel-consumer-build
/tmp/parcel-consumer-build/consumer
```

Expected output (something close to):

```
parcel version: 0.1.0
wire payload  : {"k":"i32","v":42}
restored kind : i32
restored value: 42
```

## Run it once a release exists

Drop the override and let `FetchContent` resolve the tag/tarball normally:

```sh
cmake -S examples/consumers/fetch_content -B /tmp/parcel-consumer-build
cmake --build /tmp/parcel-consumer-build
/tmp/parcel-consumer-build/consumer
```
