// Minimal downstream consumer: build a primitive cell, serialize it, parse it
// back through a registry. If this binary builds and runs, the FetchContent
// integration is wired correctly.

#include <parcel/parcel.h>

#include <iostream>

int main() {
    parcel::ParcelRegistry registry;

    const parcel::I32Cell answer = 42;
    const auto wire = answer.to_json();

    std::cout << "parcel version: " << parcel::version << '\n';
    std::cout << "wire payload  : " << wire.dump() << '\n';

    const auto restored = registry.cell_from_json(wire);
    std::cout << "restored kind : " << restored->kind() << '\n';
    std::cout << "restored value: " << restored << '\n';

    return 0;
}
