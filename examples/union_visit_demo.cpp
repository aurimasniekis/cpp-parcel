// Demo: visiting and inspecting alternatives of a UnionCell<Ts...>.
//   - member union.visit(F) (mutable + const) and free parcel::visit(F, union)
//   - parcel::Overload{...} for lambda-overload visitors
//   - get_if<I> / get_if<S> (member + free, including the null-pointer path)
//   - holds_alternative<I> / holds<S>
//   - free parcel::get<I>(u) / parcel::get<S>(u)

#include <parcel/parcel.h>

#include <iostream>
#include <string>
#include <string_view>
#include <type_traits>

namespace {

using IntStr = parcel::UnionCell<parcel::I32Cell, parcel::StringCell>;

void print_section(const std::string_view title) {
    std::cout << "\n--- " << title << " ---\n";
}

}  // namespace

int main() {
    print_section("Build a union holding the i32 alternative");
    IntStr int_u;
    int_u.value.emplace<0>(42);
    std::cout << "index=" << int_u.index() << "  active_kind=" << int_u.active_kind()
              << "  to_string=" << int_u << '\n';

    print_section("union.visit(F) — member, with parcel::Overload set");
    auto stringify = parcel::Overload{
        [](const std::int32_t i) -> std::string { return "int:" + std::to_string(i); },
        [](std::string const& s) -> std::string { return "string:" + s; },
    };
    std::cout << "visit(int)    -> " << int_u.visit(stringify) << '\n';

    IntStr str_u;
    str_u.value.emplace<1>(std::string("hello"));
    std::cout << "visit(string) -> " << str_u.visit(stringify) << '\n';

    print_section("Free parcel::visit(F, union) — same dispatch on a const reference");
    IntStr const& cu = str_u;
    std::cout << "free visit(string) -> " << parcel::visit(stringify, cu) << '\n';

    print_section("Generic-lambda visitor handles every alternative with one body");
    auto sniff = []<typename T0>(T0 const& v) -> std::string {
        if constexpr (std::is_same_v<std::decay_t<T0>, std::int32_t>) {
            return "i32:" + std::to_string(v);
        } else {
            return "string:" + v;
        }
    };
    std::cout << "sniff(int)    -> " << parcel::visit(parcel::Overload{sniff}, int_u) << '\n';
    std::cout << "sniff(string) -> " << parcel::visit(parcel::Overload{sniff}, str_u) << '\n';

    print_section("union.get_if<I>() / get_if<S>() — nullptr on miss");
    if (auto* p = int_u.get_if<0>()) {
        std::cout << "get_if<0> on int : *" << *p << '\n';
    }
    std::cout << "get_if<1> on int : " << (int_u.get_if<1>() == nullptr ? "nullptr" : "set")
              << '\n';
    if (auto* p = str_u.get_if<std::string>()) {
        std::cout << "get_if<std::string> on string : *" << *p << '\n';
    }
    std::cout << "get_if<std::int32_t> on string : "
              << (str_u.get_if<std::int32_t>() == nullptr ? "nullptr" : "set") << '\n';

    print_section("Free parcel::get_if — same shape, plus null-pointer safe path");
    {
        IntStr* up = nullptr;
        std::cout << "free get_if<0>(nullptr) : "
                  << (parcel::get_if<0>(up) == nullptr ? "nullptr" : "set") << '\n';
    }
    if (auto const* p = parcel::get_if<std::string>(&str_u)) {
        std::cout << "free get_if<std::string>(&str_u) : *" << *p << '\n';
    }

    print_section("union.holds_alternative<I>() / holds<S>()");
    std::cout << "int_u.holds_alternative<0>()       = " << std::boolalpha
              << int_u.holds_alternative<0>() << '\n';
    std::cout << "int_u.holds<std::int32_t>()        = " << int_u.holds<std::int32_t>() << '\n';
    std::cout << "str_u.holds_alternative<1>()       = " << str_u.holds_alternative<1>() << '\n';
    std::cout << "str_u.holds<std::string>()         = " << str_u.holds<std::string>() << '\n';
    std::cout << "str_u.holds<std::int32_t>()        = " << str_u.holds<std::int32_t>() << '\n';

    print_section("Free parcel::get<I>(u) / parcel::get<S>(u)");
    std::cout << "parcel::get<0>(int_u)              = " << parcel::get<0>(int_u) << '\n';
    std::cout << "parcel::get<std::int32_t>(int_u)   = " << parcel::get<std::int32_t>(int_u)
              << '\n';
    std::cout << "parcel::get<1>(str_u)              = " << parcel::get<1>(str_u) << '\n';
    std::cout << "parcel::get<std::string>(str_u)    = " << parcel::get<std::string>(str_u) << '\n';

    return 0;
}
