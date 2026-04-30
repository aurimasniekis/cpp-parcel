// Demo: UnionCell<Ts...> — a closed-set polymorphic cell. Storage is a
// std::variant<typename Ts::storage_t...>; the wire encodes every
// alternative kind into "k" (e.g. "u:i32,string,bool") and the active
// alternative is recoverable from the inner v.k

#include <parcel/parcel.h>

#include <iostream>
#include <string>

namespace {

using IntStrBool = parcel::UnionCell<parcel::I32Cell, parcel::StringCell, parcel::BoolCell>;

void print_section(const std::string_view title) {
    std::cout << "\n--- " << title << " ---\n";
}

void describe(IntStrBool const& u) {
    std::cout << "  index()=" << u.index() << "  active_kind()=" << u.active_kind()
              << "  to_string()=" << u << '\n';
}

}  // namespace

int main() {
    parcel::ParcelRegistry registry;
    registry.register_kind(IntStrBool::descriptor());

    print_section("Construct from each alternative");
    const IntStrBool a = std::int32_t{42};
    const IntStrBool b = std::string{"hello"};
    const IntStrBool c = true;
    describe(a);
    describe(b);
    describe(c);

    print_section("Switch alternatives via operator=");
    IntStrBool u = std::int32_t{1};
    describe(u);
    u = std::string{"world"};
    describe(u);
    u = false;
    describe(u);

    print_section("get<I>() and get<S>()");
    IntStrBool v = std::string{"yo"};
    std::cout << "v.get<1>()           = " << v.get<1>() << '\n';
    std::cout << "v.get<std::string>() = " << v.get<std::string>() << '\n';

    print_section("to_json shape (k=\"u:i32,string,bool\", v=wrapped {k,v})");
    std::cout << a.to_json().dump(2) << "\n";
    std::cout << b.to_json().dump(2) << "\n";

    print_section("Round-trip via registry");
    const auto restored = registry.cell_from_json(b.to_json());
    const auto* typed = dynamic_cast<IntStrBool*>(restored.get());
    std::cout << "restored.kind()        = " << typed->kind() << '\n';
    std::cout << "restored.active_kind() = " << typed->active_kind() << '\n';
    std::cout << "restored.to_string()   = " << typed << '\n';

    print_section("descriptor()->to_json()");
    std::cout << IntStrBool::descriptor()->to_json().dump(2) << '\n';

    return 0;
}
