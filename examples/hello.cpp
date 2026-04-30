// Smallest runnable parcel program. Mirrors the README's Hello world snippet:
// build a primitive cell, serialize it to JSON, and round-trip it through the
// registry. Run with `make example` (or `./build/examples/parcel_hello`).

#include <parcel/parcel.h>

#include <iostream>

int main() {
    const parcel::ParcelRegistry registry;  // ships with all builtins
    const parcel::I32Cell answer = 42;

    const auto wire = answer.to_json();  // {"k":"i32","v":42}
    const auto restored = registry.cell_from_json(wire);

    std::cout << restored->kind() << " = "  // "i32 = 42"
              << restored << '\n';

    return 0;
}
