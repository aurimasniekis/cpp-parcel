// Demo: working with a cell_t handle you already have. Strict and lenient
// casts, the std::optional view, free to_json/to_string helpers, hashing,
// and walking a cell tree depth-first.

#include <parcel/parcel.h>

#include <iostream>
#include <map>
#include <memory>
#include <string>
#include <string_view>
#include <unordered_set>
#include <vector>

namespace {

void print_section(const std::string_view title) {
    std::cout << "\n--- " << title << " ---\n";
}

}  // namespace

int main() {
    parcel::ParcelRegistry registry;

    print_section("cell_cast<C> — strict, throws on null/mismatch");
    parcel::cell_t i = parcel::I32Cell::of(42);
    const auto strict = parcel::cell_cast<parcel::I32Cell>(i);
    std::cout << "strict->value = " << strict->value << '\n';
    try {
        (void)parcel::cell_cast<parcel::StringCell>(i);
    } catch (std::runtime_error const& e) {
        std::cout << "kind mismatch: " << e.what() << '\n';
    }
    try {
        const parcel::cell_t null_handle;
        (void)parcel::cell_cast<parcel::I32Cell>(null_handle);
    } catch (std::runtime_error const& e) {
        std::cout << "null handle: " << e.what() << '\n';
    }

    print_section("cell_cast_or<C> — fallback on miss");
    const auto fallback = parcel::I32Cell::of(99);
    const parcel::cell_t s = parcel::StringCell::of(std::string("hi"));
    const auto or_fallback = parcel::cell_cast_or<parcel::I32Cell>(s, fallback);
    std::cout << "or_fallback->value = " << or_fallback->value << '\n';

    print_section("cell_cast_value<C> — cast and unwrap");
    const auto unwrapped = parcel::cell_cast_value<parcel::StringCell>(s);
    std::cout << "unwrapped = " << unwrapped << '\n';

#if PARCEL_HAS_EXPECTED
    print_section("try_cell_cast<C> — std::expected");
    if (auto r = parcel::try_cell_cast<parcel::I32Cell>(i); r.has_value()) {
        std::cout << "ok: " << r.value()->value << '\n';
    }
    if (auto r = parcel::try_cell_cast<parcel::I32Cell>(s); !r.has_value()) {
        std::cout << "miss: code=" << static_cast<int>(r.error().code)
                  << " message=" << r.error().message << '\n';
    }
#endif

    print_section("as<C> — std::optional view");
    if (const auto v = parcel::as<parcel::I32Cell>(i)) {
        std::cout << "as<I32Cell>(i) = " << *v << '\n';
    }
    if (!parcel::as<parcel::I32Cell>(s).has_value()) {
        std::cout << "as<I32Cell>(s) = nullopt\n";
    }

    print_section("value_or<C> — extract storage with fallback");
    std::cout << "value_or(i, 0)        = " << parcel::value_or<parcel::I32Cell>(i, 0) << '\n';
    std::cout << "value_or(s, 0)        = " << parcel::value_or<parcel::I32Cell>(s, 0) << '\n';
    const parcel::cell_t null_handle;
    std::cout << "value_or(null, 7)     = " << parcel::value_or<parcel::I32Cell>(null_handle, 7)
              << '\n';

    print_section("free to_json / to_string / to_string_pretty");
    std::cout << "to_json(i).dump()         = " << parcel::to_json(i).dump() << '\n';
    std::cout << "to_json(null).dump()      = " << parcel::to_json(null_handle).dump() << '\n';
    std::cout << "to_string(s)              = " << parcel::to_string(s) << '\n';
    std::cout << "to_string(null)           = " << parcel::to_string(null_handle) << '\n';
    std::cout << "to_string_pretty(i)       = " << parcel::to_string_pretty(i) << '\n';
    std::cout << "to_string_pretty(null)    = " << parcel::to_string_pretty(null_handle) << '\n';

    print_section("std::hash<ICell> / std::hash<cell_t> / hash_cell");
    parcel::I32Cell raw_a{42};
    parcel::I32Cell raw_b{42};
    std::cout << "hash(I32Cell{42}) == hash(I32Cell{42}) : "
              << (std::hash<parcel::ICell>{}(raw_a) == std::hash<parcel::ICell>{}(raw_b) ? "yes"
                                                                                         : "no")
              << '\n';
    std::cout << "hash_cell(i)                            = " << parcel::hash_cell(i) << '\n';
    std::cout << "hash_cell(null)                         = " << parcel::hash_cell(null_handle)
              << '\n';

    std::unordered_set<parcel::cell_t> set;
    set.insert(parcel::I32Cell::of(1));
    set.insert(parcel::I32Cell::of(2));
    set.insert(parcel::I32Cell::of(1));
    std::cout << "std::unordered_set<cell_t> size after 3 inserts = " << set.size() << '\n';

    print_section("BaseCell::unique factory");
    auto u = parcel::I32Cell::unique(123);
    std::cout << "unique(123)->value = " << u->value << '\n';

    print_section("from_json_strict primitive shorthand");
    const auto j = parcel::I32Cell::of(7)->with_name("answer")->to_json();
    std::cout << "wire = " << j.dump() << '\n';
    const auto rebuilt = parcel::I32Cell::from_json_strict(j);
    const auto* typed = dynamic_cast<parcel::I32Cell*>(rebuilt.get());
    std::cout << "rebuilt.value = " << typed->value << '\n';
    std::cout << "rebuilt.meta.name = " << typed->meta()->name.value_or("<unset>") << '\n';

    print_section("walk_to_vector — depth-first (path, kind) over a tree");
    auto inner = parcel::MapCell::of(std::map<std::string, parcel::cell_t>{
        {"id", parcel::cell(1)},
        {"label", parcel::cell(std::string("alpha"))},
    });
    auto root = parcel::ListCell::of(std::vector<parcel::cell_t>{
        inner,
        parcel::cell(true),
    });
    for (const auto& [path, cell] : parcel::walk_to_vector(root)) {
        std::cout << "  " << (path.empty() ? std::string{"<root>"} : path) << " : " << cell->kind()
                  << '\n';
    }

#if PARCEL_HAS_GENERATOR
    print_section("walk — coroutine generator (lazy)");
    std::size_t count = 0;
    for (auto&& entry : parcel::walk(root)) {
        ++count;
        std::cout << "  yielded " << (entry.path.empty() ? std::string{"<root>"} : entry.path)
                  << " (" << entry.cell->kind() << ")\n";
    }
    std::cout << "yielded " << count << " entries\n";
#else
    print_section("walk — coroutine generator unavailable on this toolchain");
    std::cout << "walk_to_vector is the eager fallback\n";
#endif

    return 0;
}
