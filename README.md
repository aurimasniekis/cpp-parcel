# parcel

Wrappable, wire-transferable C++23 value system with JSON serialization.

[![CI](https://github.com/aurimasniekis/cpp-parcel/actions/workflows/ci.yml/badge.svg)](https://github.com/aurimasniekis/cpp-parcel/actions/workflows/ci.yml)
[![Docs](https://github.com/aurimasniekis/cpp-parcel/actions/workflows/docs.yml/badge.svg)](https://aurimasniekis.github.io/cpp-parcel/)

## What is parcel?

A *cell* is a typed value that knows how to serialize itself to JSON and
back. parcel ships a small family of cells — primitives, lists, maps,
hash-maps, structs, unions — that all share one wire shape:
`{"k": <kind>, "v": <value>, "d": <optional display info>}`. A `ParcelRegistry`
dispatches incoming JSON back to the right cell type so heterogeneous
payloads round-trip safely.

It is not an RPC framework. It just gives you the value layer: typed C++
values on one side, self-describing JSON on the wire, and a registry that
glues the two together.

## Key features

- Header-only, single umbrella include `<parcel/parcel.h>`.
- Self-describing wire envelope (`{k, v, d}`) for every cell.
- Built-in primitives, ordered & hash-backed maps, typed & heterogeneous
  containers, tagged unions, and a CRTP `StructCell` base for your own
  structs.
- Strict JSON deserialization: every documented field is required and the
  `"k"` tag is verified against the static `kind_id`.
- Non-throwing `try_*` parsing surface returning `std::expected<T,
  ParcelError>` (when `<expected>` is available).
- `std::format`, `std::hash`, `operator<=>`, `std::generator` walk
  helpers, and `std::expected` integration.
- `std::chrono`, `std::filesystem::path`, `std::array`, `std::deque`,
  `std::list`, `std::set`, `std::unordered_map<std::string, T>`,
  `std::variant`, and `std::optional` adapters out of the box.
- Doxygen reference, CMake + Meson builds, ASan/UBSan, clang-tidy and
  coverage gates wired into the Makefile and CI.

## Requirements

- C++23 (GCC 14+, Clang 18+, Apple Clang).
- CMake ≥ 3.25 *or* Meson ≥ 1.3.
- `nlohmann_json` 3.12.0 and GoogleTest 1.15.2 are fetched automatically;
  you do not need to install them.

## Install

### CMake

parcel is header-only. Pull it into your CMake project with `FetchContent`,
pinned to a release tarball:

```cmake
include(FetchContent)
FetchContent_Declare(
        cpp-parcel
        URL https://github.com/aurimasniekis/cpp-parcel/archive/refs/tags/v0.2.2.tar.gz
        URL_HASH SHA256=7e4277f9f57b5bbea815f029c086e194d6ceedb6fa370a358ef0c9e22e1d30e5
)
FetchContent_MakeAvailable(cpp-parcel)

add_executable(my_app main.cpp)
target_link_libraries(my_app PRIVATE parcel::parcel)
```

A complete worked example, including the override pattern used for local
development, lives in [`examples/consumers/fetch_content/`](examples/consumers/fetch_content/).

### Meson

parcel ships a `meson.build` so it can be consumed as a Meson subproject.
Drop a wrap file in `subprojects/parcel.wrap`:

```ini
[wrap-file]
directory       = cpp-parcel-0.2.2
source_url      = https://github.com/aurimasniekis/cpp-parcel/archive/refs/tags/v0.2.2.tar.gz
source_filename = cpp-parcel-0.2.2.tar.gz
source_hash     = 7e4277f9f57b5bbea815f029c086e194d6ceedb6fa370a358ef0c9e22e1d30e5

[provide]
parcel = parcel_dep
```

Then in your `meson.build`:

```meson
parcel_dep = dependency('parcel', version: '>=0.2.2')

executable('my_app', 'main.cpp', dependencies: [parcel_dep])
```

`nlohmann_json` is pulled in transitively via parcel's own wrap, the same
way CMake's `FetchContent` pulls it for that build.

## Hello world

Construct a primitive cell, serialize it, and read it back through the
registry. *(The exact snippet below lives at
[`examples/hello.cpp`](examples/hello.cpp) and runs via `make example`;
see [`examples/primitive_demo.cpp`](examples/primitive_demo.cpp) for a
longer tour.)*

```cpp
#include <parcel/parcel.h>
#include <iostream>

int main() {
    parcel::ParcelRegistry registry;             // ships with all builtins
    parcel::I32Cell answer = 42;

    auto wire     = answer.to_json();            // {"k":"i32","v":42}
    auto restored = registry.cell_from_json(wire);

    std::cout << restored->kind() << " = "       // "i32 = 42"
              << restored << '\n';
}
```

`<parcel/parcel.h>` is the only include you need for the public API.
It pulls in the umbrella registry, the std-adapter cells (chrono,
filesystem, hash maps), and the formatting and walking helpers.

## Wire format

Every cell serializes to `{"k": <kind>, "v": <value>, "d": <display info>}`. The
`"d"` block is omitted when no display info is set. Inside a struct cell, every
field is itself a `{k, v}` cell:

```json
{
  "k": "s:person",
  "v": {
    "id":   {"k": "i32",    "v": 1},
    "name": {"k": "string", "v": "Alice"}
  }
}
```

128-bit integers are encoded as decimal strings because JSON has no native
integer type wider than 53 bits. Deserialization is **strict**: every
documented field must be present and `"k"` is verified against the cell's
static `kind_id`. Run `make docs` for the full reference.

## Built-in cell types

A default-constructed `ParcelRegistry` already contains every primitive,
the heterogeneous and typed list/map/hash-map families, plus the chrono
and filesystem adapter cells. Pass a `BuiltinsOptions{}` to opt out of
any of those four batches.

### Primitives

| C++ type        | cell alias   | wire kind | notes                                                  |
|-----------------|--------------|-----------|--------------------------------------------------------|
| `bool`          | `BoolCell`   | `bool`    |                                                        |
| `char`          | `CharCell`   | `char`    | one-character JSON string                              |
| `std::int8_t`   | `I8Cell`     | `i8`      |                                                        |
| `std::int16_t`  | `I16Cell`    | `i16`     |                                                        |
| `std::int32_t`  | `I32Cell`    | `i32`     |                                                        |
| `std::int64_t`  | `I64Cell`    | `i64`     |                                                        |
| `parcel::i128`  | `I128Cell`   | `i128`    | encoded as a decimal string (JSON has no 128-bit ints) |
| `std::uint8_t`  | `U8Cell`     | `u8`      |                                                        |
| `std::uint16_t` | `U16Cell`    | `u16`     |                                                        |
| `std::uint32_t` | `U32Cell`    | `u32`     |                                                        |
| `std::uint64_t` | `U64Cell`    | `u64`     |                                                        |
| `parcel::u128`  | `U128Cell`   | `u128`    | encoded as a decimal string                            |
| `float`         | `FloatCell`  | `f32`     |                                                        |
| `double`        | `DoubleCell` | `f64`     |                                                        |
| `std::string`   | `StringCell` | `string`  |                                                        |

The 128-bit primitives are gated on `COMMONS_HAS_INT128` (from commons) and are
present for any compiler that exposes `__int128`.

### Containers

For each primitive `T` listed above, the corresponding aliases exist:

| family             | example aliases                                             | wire kind   |
|--------------------|-------------------------------------------------------------|-------------|
| typed list         | `I32ListCell`, `StringListCell`, `BoolListCell`, …          | `l:<elem>`  |
| typed map          | `I32MapCell`, `StringMapCell`, `BoolMapCell`, …             | `m:<elem>`  |
| typed hash map     | `I32HashMapCell`, `StringHashMapCell`, `BoolHashMapCell`, … | `hm:<elem>` |
| heterogeneous list | `ListCell`                                                  | `l`         |
| heterogeneous map  | `MapCell` (string keys, ordered)                            | `m`         |
| heterogeneous hash | `HashMapCell` (string keys, unordered storage)              | `hm`        |

Hash-map cells use `std::unordered_map` storage. Iteration in storage is
unspecified, but the wire form is canonical (sorted keys) so equal maps
always serialize identically.

### Std-adapter cells

| cell                  | wire kind          | wire shape                     |
|-----------------------|--------------------|--------------------------------|
| `SystemTimePointCell` | `time:sys_seconds` | i64 epoch seconds              |
| `UnixMillisCell`      | `time:unix_ms`     | i64 epoch milliseconds         |
| `TimestampMsCell`     | `time:unix_ms`     | alias of `UnixMillisCell`      |
| `DurationMsCell`      | `time:ms`          | i64 milliseconds               |
| `YmdCell`             | `time:ymd`         | ISO-8601 `"YYYY-MM-DD"` string |
| `PathCell`            | `fs:path`          | UTF-8 portable path string     |

### Commons vocabulary cells

Adapters for the [`aurimasniekis/cpp-commons`](https://github.com/aurimasniekis/cpp-commons)
value types live in `<parcel/commons.h>` and are pre-registered when
`BuiltinsOptions::commons` is true (the default):

| cell                    | wire kind             | wire shape                       |
|-------------------------|-----------------------|----------------------------------|
| `ColorCell`             | `color`               | hex string                       |
| `IconCell`              | `icon`                | `"set:name"` string              |
| `DisplayInfoCell`       | `display_info`        | JSON object                      |
| `FlagCell`              | `flag`                | flag name string                 |
| `FlagSetCell`           | `flag_set`            | array of flag names              |
| `SemVerCell`            | `semver`              | canonical version string         |
| `VersionConstraintCell` | `version_constraint`  | npm-style range string           |
| `OriginCell`            | `origin`              | `{"kind", …fields}` object       |
| `ReasonCell`            | `reason`              | `{"kind", …fields}` object       |
| `FailureReasonCell`     | `failure_reason`      | `{"kind", …fields}` object       |
| `AbilityCell`           | `ability`             | `{"kind", …fields}` object       |
| `IdentityCell`          | `identity`            | `{"kind", …fields}` object       |
| `AuditRecordCell`       | `audit_record`        | JSON object                      |
| `AuditRecordsCell`      | `audit_records`       | JSON array of audit records      |
| `ValueCell`             | `md:v`                | dynamic JSON-shaped value        |
| `ObjectCell`            | `md:o`                | string-keyed map of `md::Value`  |
| `ArrayCell`             | `md:a`                | array of `md::Value`             |

`SemVerCell` / `VersionConstraintCell` carry `comms::SemVer` /
`comms::VersionConstraint`; a malformed range throws on decode (commons'
`parse` throws), matching parcel's strict-deserialization policy. `OriginCell`,
`ReasonCell`, `FailureReasonCell`, `AbilityCell`, and `IdentityCell` carry
move-only commons `…Ptr` handles and resolve the inner `"kind"` discriminator
against the matching commons global registry on decode (an unknown kind throws;
`FailureReasonCell` additionally rejects a non-failure reason kind). Because the
sets are open, third-party kinds that self-register decode correctly too.
`AuditRecordCell` / `AuditRecordsCell` carry `comms::AuditRecord` /
`comms::AuditRecords` (a capped log — capacity is not serialized, so a
within-capacity log round-trips exactly). The templated
`ChangeAuditRecordCell<P>` / `ChangeAuditRecordsCell<P>` (wire kinds
`change_audit_record:<P>` / `change_audit_records:<P>`) carry the before/after
change variants and are opt-in per element cell `P` — register one with
`ChangeAuditRecordCell<SomeCell>::descriptor()`, like `TypedListCell<P>`. The
`ValueCell` / `ObjectCell` / `ArrayCell` adapters carry the `comms::md` dynamic
value tree.

Each cell family lives in a focused header under `<parcel/commons/>` (e.g.
`<parcel/commons/reason.h>`); `<parcel/commons.h>` is an umbrella that includes
them all.

User-defined cells live in two more namespaces: structs under `s:`
(e.g. `s:person`) and unions under `u:` with the alternatives joined by
commas (e.g. `u:i32,string`).

## By example

Each section quotes a tight version of the matching `examples/*.cpp`. Open
the file for the full runnable form.

### Define a typed struct

You write the data in plain C++ and a small wrapper class teaches parcel
how to serialize it. `StructCell` is a CRTP base — *Curiously Recurring
Template Pattern*, where the wrapper passes itself in as a template arg so
the base can call back into it. The third template arg is the bare kind
id; parcel prepends `s:`, so the wire kind below is `"s:person"`.

`std::optional<T>` becomes an optional field — omitted from the wire when
empty. *(See [`examples/struct_demo.cpp`](examples/struct_demo.cpp).)*

```cpp
struct Person {
    std::int32_t id{};
    std::string  name;
    std::optional<std::string> email;
    std::vector<std::string>   roles;
};

class PersonCell : public parcel::StructCell<PersonCell, Person, "person"> {
public:
    using StructCell::StructCell;

    static auto field_descriptors() {
        return parcel::FieldsBuilder<Person>{}
            .field<&Person::id>("id")
            .field<&Person::name>("name")
            .field<&Person::email>("email")   // optional<string> — omitted when empty
            .field<&Person::roles>("roles")   // vector<string>   — typed list on the wire
            .build();
    }
};
```

`FieldsBuilder` infers each field's cell type from its C++ type via
`default_cell_for`. The set of supported defaults is documented under
*Standard library interop* below.

**Required statics on a `StructCell` subclass:**

| static                | required? | what it does                                                           |
|-----------------------|-----------|------------------------------------------------------------------------|
| `kind_id`             | auto      | synthesized as `"s:" + StructId` — never declare it manually           |
| `field_descriptors()` | yes       | returns the result of a `FieldsBuilder<Payload>{}.field<…>(…).build()` |
| `display_info()`      | optional  | cell-level display info; defaults to an empty `parcel::DisplayInfo{}`  |
| `allow_extra_fields`  | optional  | `true` opts into lenient deserialization; defaults to `false`          |

### Capture extra struct fields

When `Derived::allow_extra_fields` is `true`, unknown JSON keys are
routed through the registry and retained in `extras` (a
`std::map<std::string, parcel::cell_t>`). The on-the-wire round trip is
preserved. *(See
[`examples/struct_extras_demo.cpp`](examples/struct_extras_demo.cpp).)*

```cpp
class FlexibleEventCell
    : public parcel::StructCell<FlexibleEventCell, FlexibleEvent, "flexible_event"> {
public:
    using StructCell::StructCell;
    static constexpr bool allow_extra_fields = true;

    static auto field_descriptors() {
        return parcel::FieldsBuilder<FlexibleEvent>{}
            .field<&FlexibleEvent::id>("id")
            .build();
    }
};
```

Without that flag, an unknown key throws — matching parcel's strict
deserialization stance.

### Inferred field types and defaults

`FieldsBuilder::field<MemberPtr>(key)` looks the cell type up via
`parcel::default_cell_for<FieldType>` so most ordinary members need no
explicit cell argument. The default-cell resolver covers:

- every primitive backed by a `PrimitiveCell`,
- `std::vector<T>`, `std::array<T, N>`, `std::deque<T>`,
  `std::list<T>`, and `std::set<T>` → `TypedListCell<…>`,
- `std::map<std::string, T>` → `TypedMapCell<…>`,
- `std::unordered_map<std::string, T>` → `TypedMapCell<…>` (sorted on
  the wire),
- `std::optional<T>` → the same wrapper as `T`, with optionality
  handled by struct-field absence,
- `std::variant<Ts...>` → `UnionCell<default_cell_for_t<Ts>...>`,
- any user cell tagged with `PARCEL_DEFAULT_CELL`.

*(See [`examples/defaults_demo.cpp`](examples/defaults_demo.cpp) and
[`examples/std_interop_demo.cpp`](examples/std_interop_demo.cpp).)*

### Make your own primitive cell

Adding a new primitive is a short recipe: define a storage type, give it
JSON conversions, derive `BaseCell`, declare a `kind_id`, and register
it. *(See
[`examples/custom_primitive_demo.cpp`](examples/custom_primitive_demo.cpp).)*

```cpp
struct Uuid { std::uint64_t hi, lo; /* with to_hex / from_hex */ };

void to_json  (parcel::json_t& j, Uuid const& u) { j = u.to_hex(); }
void from_json(parcel::json_t const& j, Uuid& u) { u = Uuid::from_hex(j.get<std::string>()); }

class UuidCell : public parcel::BaseCell<UuidCell, Uuid> {
    using base_t = parcel::BaseCell<UuidCell, Uuid>;
public:
    using base_t::base_t;
    using base_t::operator=;

    static constexpr std::string_view kind_id = "uuid";

    std::string to_string() const override { return value.to_hex(); }

    static parcel::cell_t from_json(parcel::json_t const& j,
                                    parcel::ParcelRegistry const&) {
        auto v = base_t::cell_from_json<Uuid>(j, kind_id);
        auto cell = std::make_shared<UuidCell>(v);
        base_t::absorb_display_info(j, cell);
        return cell;
    }

    static parcel::cell_type_descriptor_t descriptor() {
        static const auto d = std::make_shared<parcel::SimpleCellTypeDescriptor<UuidCell>>(
            parcel::DisplayInfo{.name = "Uuid"});
        return d;
    }
};

// Lets FieldsBuilder and parcel::cell(uuid) pick UuidCell automatically.
PARCEL_DEFAULT_CELL(UuidCell);
```

`PARCEL_DEFAULT_CELL(CellT)` is a one-line macro that specializes
`parcel::default_cell_for<CellT::storage_t>`. Then
`registry.register_kind(UuidCell::descriptor())` and `UuidCell` is a
fully wire-capable cell on the same footing as `I32Cell` or `StringCell`.

### The registry

The registry is what turns "some JSON" into "the right cell". A default-
constructed `ParcelRegistry` already contains every built-in primitive,
list, map, hash map, and the std-adapter cells; you only register your
own kinds. Pass a `BuiltinsOptions{}` if you want a leaner registry —
the four flags (`primitives`, `collections`, `typed_collections`, `std`)
toggle each batch independently. *(See
[`examples/registry_demo.cpp`](examples/registry_demo.cpp).)*

```cpp
// Default — every primitive + ListCell/MapCell/HashMapCell + typed variants
//           + chrono and filesystem cells.
parcel::ParcelRegistry registry;

// Or: opt out of typed collections to shrink the registry.
parcel::ParcelRegistry slim{{.typed_collections = false}};

// Or: skip the std-adapter cells entirely.
parcel::ParcelRegistry no_std{{.std = false}};

registry.register_kind(PersonCell::descriptor());

const std::vector<parcel::json_t> wire {
    parcel::I32Cell{42}.to_json(),
    parcel::StringCell{"hello"}.to_json(),
    PersonCell{Person{.id = 1, .name = "Alice"}}.to_json(),
};
for (auto const& j : wire) {
    auto cell = registry.cell_from_json(j);
    std::cout << '[' << cell->kind() << "] " << cell << '\n';
}
```

The registry also supports introspection and schema export:

```cpp
registry.count();                                       // total kinds registered
registry.contains("s:person");                          // bool
registry.kinds();                                       // vector<string_view>

// All struct kinds; all kinds backed by std::string.
registry.find_by_category(parcel::descriptor::CellCategory::Struct);
registry.find_by_storage<std::string>();

// Transitive schema closure — every kind reachable from "s:person".
auto def = registry.define("s:person");
std::cout << def.to_json().dump(2) << '\n';
```

Variadic registration helpers are also available:

```cpp
registry.register_cells<PersonCell, AddressCell>();        // by cell type
registry.register_kinds(PersonCell::descriptor(), ...);    // by descriptor
```

### Wrap any value with `parcel::cell(...)` or `Cell::of(...)`

Two convenience factories build `cell_t` (a.k.a. `std::shared_ptr<ICell>`)
without spelling out `std::make_shared`:

```cpp
auto a = parcel::cell(42);                  // -> shared_ptr<I32Cell>      (inferred)
auto b = parcel::cell("hi");                // -> shared_ptr<StringCell>   (const char* -> string)
auto c = parcel::cell(Person{.id = 1});     // -> shared_ptr<PersonCell>   (via PARCEL_DEFAULT_CELL)

auto d = parcel::I32Cell::of(7);            // -> shared_ptr<I32Cell>      (explicit cell type)
auto e = PersonCell::of(Person{.id = 2});   // -> shared_ptr<PersonCell>   (forwards to ctor)

auto u = parcel::I32Cell::unique(7);        // -> unique_ptr<I32Cell>      (sole-owner sibling)
```

`parcel::cell(v)` looks the wrapper up via `default_cell_for<T>`; every
built-in primitive plus any cell registered with `PARCEL_DEFAULT_CELL` is
eligible. `Cell::of(args...)` skips the lookup and forwards straight to
`std::make_shared<Cell>`. `Cell::unique(args...)` is the
`std::unique_ptr` equivalent for callers who want sole ownership. *(See
[`examples/cell_handle_demo.cpp`](examples/cell_handle_demo.cpp) for
more on `cell_t` ownership.)*

### Annotate cells with display info

Every cell can carry a small display-info block — `name`, `description`,
`icon`, `color` — that travels with the value under `"d"`. Builders are
immutable: each returns a fresh cell. *(See
[`examples/display_info_demo.cpp`](examples/display_info_demo.cpp).)*

```cpp
auto annotated = parcel::I32Cell::of(42)
    ->with_name("Answer")
    ->with_description("To life, the universe, and everything")
    ->with_icon("star")
    ->with_color("#ffcc00");

std::cout << annotated->to_json().dump(2);
// {"k":"i32","v":42,"d":{"name":"Answer", ...}}
```

`with_display_info(DisplayInfo{...})` replaces the whole block at once
(the accessor and builder are named for the `DisplayInfo` they carry; the
wire key stays the terse `"d"`). Reading goes through
`cell->overridden_display_info()`, which returns a `std::optional<DisplayInfo>`.
Comparison and hashing both ignore display info — two cells with the same
`k`/`v` but different `overridden_display_info()` are equivalent.

### Lists & maps

Two flavours: typed (homogeneous, raw scalars on the wire) and generic
(heterogeneous, full cells on the wire). The free helper
`parcel::cell(x)` wraps any built-in type in the right cell. *(See
[`examples/list_demo.cpp`](examples/list_demo.cpp),
[`examples/map_demo.cpp`](examples/map_demo.cpp).)*

```cpp
parcel::I32ListCell ints{1, 2, 3};                   // kind = "l:i32"
ints.push_back(4);

parcel::ListCell mixed{                              // kind = "l"
    parcel::cell(42),
    parcel::cell("hello"),
    parcel::cell(true),
};

parcel::StringMapCell tags{                          // kind = "m:string"
    {"role", std::string{"admin"}},
    {"env",  std::string{"prod"}},
};
```

### Reuse fields across struct cells

Struct cells can splice another struct's fields in, override one by key,
or drop one. The wire stays flat — every field, inherited or not, lives
at the top of the cell's `"v"` object. *(See
[`examples/struct_inheritance_demo.cpp`](examples/struct_inheritance_demo.cpp).)*

```cpp
class HomeAddressCell : public parcel::StructCell<HomeAddressCell, HomeAddress, "home_address"> {
public:
    using StructCell::StructCell;
    static auto field_descriptors() {
        return parcel::FieldsBuilder<HomeAddress>{}
            .extend<StreetAddressCell>()                // splice street + city
            .remove_field("city")                       // … but drop city
            .field<&StreetAddress::street>("street")    // override street (last-wins)
            .name("StreetOverridden")
            .field<&HomeAddress::label>("label")        // add a new field
            .build();
    }
};
```

### Tagged unions

A `UnionCell<Ts...>` is a closed-set polymorphic cell — it holds exactly
one of a fixed list of alternatives. The wire `"k"` lists every
alternative in template order (e.g. `"u:i32,string,bool"`) and the inner
`"v"` is itself a `{k,v}` cell so the active alternative is always
recoverable. *(See [`examples/union_demo.cpp`](examples/union_demo.cpp)
and [`examples/union_visit_demo.cpp`](examples/union_visit_demo.cpp).)*

```cpp
using IntStrBool = parcel::UnionCell<parcel::I32Cell,
                                     parcel::StringCell,
                                     parcel::BoolCell>;

parcel::ParcelRegistry registry;
registry.register_kind(IntStrBool::descriptor());

IntStrBool u = std::int32_t{42};                  // active = I32Cell
u = std::string{"hello"};                          // switch alternative
u = false;                                         // … and again

std::cout << u.active_kind() << " : "             // "bool : false"
          << u.to_string() << '\n';

// Wire shape: {"k":"u:i32,string,bool","v":{"k":"bool","v":false}}
auto restored = registry.cell_from_json(u.to_json());

// Visit pattern with overload sets:
parcel::visit(parcel::Overload{
    [](std::int32_t)        { /* … */ },
    [](std::string const&)  { /* … */ },
    [](bool)                { /* … */ },
}, u);
```

`u.get<I>()` retrieves by alternative index; `u.get<S>()` retrieves by
the underlying storage type; `u.get_if<I>()` / `u.get_if<S>()` return a
pointer (or `nullptr`) instead of throwing. Free `visit`, `get`, and
`get_if` overloads mirror the member-function counterparts.

### Errors and non-throwing parsing

Strict deserialization throws on malformed input. When `<expected>` is
available, every parsing entry point also has a non-throwing twin that
returns `std::expected<…, parcel::ParcelError>`. `ParcelError` carries
a coarse `code` (`InvalidJson`, `KindMismatch`, `UnknownKind`,
`MissingField`, `TypeError`), a `message`, and (when relevant) the
offending `kind` and `field`. *(See
[`examples/error_handling_demo.cpp`](examples/error_handling_demo.cpp)
and [`examples/format_io_demo.cpp`](examples/format_io_demo.cpp).)*

```cpp
auto cell = registry.try_cell_from_string(payload);
if (!cell) {
    std::cerr << cell.error().to_string() << '\n';
    return 1;
}

if (auto person = parcel::try_cell_cast<PersonCell>(*cell); person) {
    use(*person);
}
```

The full non-throwing surface:

| function                               | header                |
|----------------------------------------|-----------------------|
| `ParcelRegistry::try_cell_from_json`   | `<parcel/registry.h>` |
| `ParcelRegistry::try_cell_from_string` | `<parcel/registry.h>` |
| `parcel::try_cell_cast<C>`             | `<parcel/cell.h>`     |
| `parcel::try_cell_from_stream`         | `<parcel/json_io.h>`  |
| `parcel::try_cell_from_bytes`          | `<parcel/json_io.h>`  |

### Library-style bases *(advanced)*

For library authors who want to ship a CRTP base that owns common fields
and carves its own kind-id namespace, use `SelfStructCell` (the deriving
class is itself the payload) together with `id_join_lit_v` to compose
the prefix. Concrete subclasses then declare a tiny
`event_field_descriptors` hook. *(See
[`examples/intrusive_struct_demo.cpp`](examples/intrusive_struct_demo.cpp).)*

```cpp
template <typename Self, parcel::fixed_string EventId>
class BaseEvent : public parcel::SelfStructCell<Self> {
public:
    static constexpr std::string_view kind_id =
        parcel::id_join_lit_v<"s:event:", EventId>;          // e.g. "s:event:something"

    static auto field_descriptors() {
        auto b = parcel::FieldsBuilder<Self>{};
        return Self::event_field_descriptors(b)
            .template field<&BaseEvent::timestamp_>("timestamp")
            .build();
    }
protected:
    std::int64_t timestamp_{};
};

class SomethingEvent : public BaseEvent<SomethingEvent, "something"> {
public:
    std::string  action;
    std::int32_t weight{};

    static auto& event_field_descriptors(parcel::FieldsBuilder<SomethingEvent>& b) {
        return b.field<&SomethingEvent::action>("action")
                .field<&SomethingEvent::weight>("weight");
    }
};
```

This is heavier C++ than the rest — reach for it only when you genuinely
want a shared base across many struct cells.

**Required statics on a `SelfStructCell` subclass:**

| static                | required? | what it does                                                                |
|-----------------------|-----------|-----------------------------------------------------------------------------|
| `kind_id`             | yes       | declared by the deriving class (often via `id_join_lit_v` in the CRTP base) |
| `field_descriptors()` | yes       | returns the result of a `FieldsBuilder<Derived>{}.field<…>(…).build()`      |
| `display_info()`      | optional  | cell-level display info; defaults to an empty `parcel::DisplayInfo{}`       |
| `allow_extra_fields`  | optional  | `true` opts into lenient deserialization; defaults to `false`               |

### Optional adapters for downstream libraries

If you ship a library and want to offer parcel `Cell` wrappers *without*
forcing every consumer to depend on parcel, ship them in a separate
adapter header (e.g. `<mylib/parcel.h>`) gated on `__has_include`:

```cpp
// mylib/parcel.h
#pragma once
#if __has_include(<parcel/parcel.h>)
    #include <mylib/color.h>
    #include <parcel/parcel.h>
    // ...your StructCell + PARCEL_DEFAULT_CELL declarations...
    #define MYLIB_HAS_PARCEL 1
#else
    #define MYLIB_HAS_PARCEL 0
#endif
```

Conventions: namespace your wire kind ids (`"mylib.color"` not
`"color"`) so multiple libraries coexist in one registry, and always
`#define MYLIB_HAS_PARCEL` to `0` or `1` so consumers can `#if` on it
without `-Wundef` warnings. A complete worked example —
[`examples/consumers/optional_adapter/`](examples/consumers/optional_adapter/) —
builds in both modes from one tree.

## Standard library interop

parcel ships C++23 adapter headers and free helpers that lean into the
standard library so you don't have to wrap every value yourself. All of
them are pulled in by `<parcel/parcel.h>`.

### Comparison and hashing

Every cell supports `operator==` and `operator<=>`
(`std::partial_ordering`, because some storage types — like `double`
with NaN — aren't totally ordered). `std::hash<parcel::cell_t>` and
`std::hash<parcel::ICell>` are specialized too, so cells drop into
`std::set` / `std::unordered_set`. Comparison and hashing both ignore
display info.

### Type-safe casting

```cpp
parcel::cell_t any = registry.cell_from_json(j);

// Throws on null or kind mismatch:
auto i32 = parcel::cell_cast<parcel::I32Cell>(any);

// Like std::optional — present iff the cast succeeds, no exception:
if (auto v = parcel::as<parcel::I32Cell>(any)) { use(*v); }

// Default fallback if missing:
int port = parcel::value_or<parcel::I32Cell>(cfg, 8080);

// std::expected variant:
auto p = parcel::try_cell_cast<PersonCell>(any);
```

### Stream and byte JSON I/O — `<parcel/json_io.h>`

`cell_from_stream`, `cell_from_bytes`, and `cell_to_stream` skip the
`std::string` round-trip when reading/writing JSON. Each has a
non-throwing `try_*` counterpart that returns `std::expected`.

### Tree walks — `<parcel/walk.h>`

`parcel::walk_to_vector(root)` returns every `(json-pointer-path, cell)`
pair in a `ListCell` / `MapCell` tree, depth-first. When `<generator>`
is available, `parcel::walk(root)` returns a truly lazy `std::generator`
of the same shape — each pull advances exactly one node, so the tree is
never fully materialized. `StructCell` and `UnionCell` are leaves in
both walks; descend into struct fields explicitly via descriptor
introspection if you need to.

### Ranges, spans, and `std::from_range`

`TypedListCell` constructs from any `std::ranges::input_range` (via
`std::from_range`) and exposes `as_span()` for read or write views over
its storage. `TypedMapCell` constructs from a paired range and exposes
`keys()` / `values()` views (also on `MapCell`).

### `std::chrono` — `<parcel/ext/chrono.h>`

`SystemTimePointCell`, `UnixMillisCell` (alias `TimestampMsCell`),
`DurationMsCell`, and `YmdCell` cover the typical wire shapes
(see *Std-adapter cells* above). All four are pre-registered when
`BuiltinsOptions::std` is true (the default).

### `std::filesystem::path` — `<parcel/ext/filesystem.h>`

`PathCell` wraps `std::filesystem::path` as a portable UTF-8 string via
`path::generic_string()`. Wire kind: `fs:path`. Pre-registered when
`BuiltinsOptions::std` is true.

### ULID — `<parcel/ext/ulid.h>`

`UlidCell` wraps [`ulid::Ulid`](https://github.com/aurimasniekis/cpp-ulid) as a
26-character Crockford base32 string. Wire kind: `ulid`. Unlike the other
adapters the ULID dependency is **opt-in** — the header is gated on
`PARCEL_HAS_ULID` (auto-detected via `__has_include(<ulid/ulid.h>)`, or
predefined by the build option), so it is inert unless you turn it on:

- **CMake:** `-DPARCEL_WITH_ULID=ON` fetches `aurimasniekis/cpp-ulid` and
  defines `PARCEL_HAS_ULID=1`.
- **Meson:** `-Dulid=true` pulls the `ulid` wrap and adds the same define.

When enabled, `UlidCell` is pre-registered alongside the commons cells (toggle
with `BuiltinsOptions::ulid`); the flag is present but inert when ULID is off.

### Hash-backed maps — `<parcel/unordered_map.h>`

`TypedHashMapCell<T>` and `HashMapCell` are `std::unordered_map`-backed
siblings of `TypedMapCell` / `MapCell`, with wire kinds `hm:<elem>` and
`hm`. Iteration in storage is unspecified, but the wire form is
canonical (sorted keys) so two equal maps always serialize identically.
They are registered alongside the ordered maps when
`BuiltinsOptions::collections` and `typed_collections` are on.

### Heterogeneous helpers

```cpp
auto l = parcel::make_list(1, std::string("hi"), true);   // ListCell
auto m = parcel::make_map({{"x", parcel::cell(1)},
                           {"y", parcel::cell(std::string("hi"))}});
```

### `std::format` integration

Cells plug into `std::format`: `std::format("{}", cell)` produces the
compact `to_string()` form, and `std::format("{:#}", cell)` produces the
multi-line `to_formatted_string()` form. The same specializations cover
`cell_t` (a null `cell_t` renders as `"<null>"`):

| spec    | output                                  |
|---------|-----------------------------------------|
| `{}`    | compact `to_string()`                   |
| `{:#}`  | multi-line `to_formatted_string()`      |
| `{:j}`  | compact JSON via `to_json().dump()`     |
| `{:j2}` | pretty JSON via `to_json().dump(2)`     |
| `{:k}`  | kind id only                            |

`operator<<` for `std::ostream` uses the compact form.

When `<print>` is available (`__cpp_lib_print`), `parcel::print(...)` and
`parcel::println(...)` thinly wrap `std::print` / `std::println` so cell
values can flow into `stdout` with the same format specs above without
pulling `std::print` into every translation unit.

## Project structure

```
include/parcel/         public API headers (parcel.h is the umbrella)
include/parcel/ext/     std::chrono and std::filesystem adapters
examples/               runnable demos, one .cpp per topic
examples/consumers/     standalone downstream integration examples
tests/                  GoogleTest suites (parcel_tests executable)
cmake/                  warning, sanitizer, coverage, install helpers
docs/                   Doxyfile.in for `make docs`
subprojects/            Meson wraps for nlohmann_json and gtest
```

## Build & develop

The CMake build is the source of truth; the `Makefile` just memorizes the
common invocations.

### CMake options

| option                      | default      | effect                                                  |
|-----------------------------|--------------|---------------------------------------------------------|
| `PARCEL_BUILD_TESTS`        | top-level ON | Build `parcel_tests` and register with CTest            |
| `PARCEL_BUILD_EXAMPLES`     | top-level ON | Build `parcel_*_demo` example targets                   |
| `PARCEL_BUILD_DOCS`         | OFF          | Add the `parcel_docs` Doxygen target                    |
| `PARCEL_ENABLE_CLANG_TIDY`  | OFF          | Run clang-tidy during the build                         |
| `PARCEL_ENABLE_SANITIZERS`  | OFF          | Compile with AddressSanitizer + UBSan                   |
| `PARCEL_ENABLE_COVERAGE`    | OFF          | Compile with Clang source-based coverage                |
| `PARCEL_WARNINGS_AS_ERRORS` | top-level ON | `-Werror` / `/WX`                                       |
| `PARCEL_INSTALL`            | top-level ON | Generate install rules and CMake package files          |
| `PARCEL_WITH_ULID`          | OFF          | Enable the ULID cell (fetches `aurimasniekis/cpp-ulid`) |

Configure presets in `CMakePresets.json`: `debug`, `release`,
`relwithdebinfo`, `minsizerel`. Each has a matching build and test
preset so `cmake --preset release && cmake --build --preset release &&
ctest --preset release` works out of the box.

### Make targets

| target          | what it does                                                   |
|-----------------|----------------------------------------------------------------|
| `make build`    | configure + build under `build/`                               |
| `make test`     | configure + build + `ctest`                                    |
| `make example`  | run `build/examples/parcel_hello`                              |
| `make sanitize` | configure + build + test in `build-san/` with ASan + UBSan     |
| `make tidy`     | configure + build in `build-tidy/` with clang-tidy             |
| `make tidy-fix` | same as `tidy` but with `-DPARCEL_CLANG_TIDY_FIX=ON`           |
| `make release`  | configure + build + test in `build-release/` (Release)         |
| `make coverage` | configure + build + test in `build-coverage/`, emit HTML       |
| `make docs`     | build the Doxygen reference into `build-docs/docs/html/`       |
| `make format`   | `clang-format -i` over `include/`, `tests/`, `examples/`       |
| `make ci`       | full pre-push gate (format + tidy + test + ASan + Release)     |

`compile_commands.json` is exported automatically into the build
directory for editor tooling.

### Meson

`meson.options` exposes `tests`, `examples`, and `ulid` toggles (all
default `false` for downstream consumers). To build everything locally:

```
meson setup build-meson -Dtests=true -Dexamples=true
meson test  -C build-meson
```

`-Dulid=true` enables the ULID cell (resolves the `ulid` wrap and defines
`PARCEL_HAS_ULID=1`). Wraps for `nlohmann_json`, `commons`, `ulid`, and
`gtest` are declared under `subprojects/` so Meson can build offline once
fetched.

### Continuous integration

`.github/workflows/ci.yml` runs five jobs on every push and pull
request:

- **build** — Ubuntu and macOS, Debug + Release, default + GCC-14 +
  Clang-20.
- **consumer** — builds `examples/consumers/fetch_content/` against the
  in-tree checkout to catch downstream breakage.
- **sanitizers** — ASan + UBSan run via `make sanitize`.
- **clang-tidy** — `make tidy`.
- **format** — `make format-check`.

## Testing

Run the full test suite with `make test` (or `ctest --test-dir build
--output-on-failure`). The 11 GoogleTest suites under `tests/` cover:

- `test_primitive.cpp` — every primitive cell, including 128-bit ints.
- `test_list.cpp`, `test_map.cpp` — typed and heterogeneous containers.
- `test_struct.cpp` — `StructCell`, optional/vector fields, inheritance,
  `allow_extra_fields`.
- `test_union.cpp` — `UnionCell`, active tracking, `get<I>` / `get<S>`.
- `test_registry.cpp` — registry dispatch, introspection, schema export.
- `test_display_info.cpp` — `DisplayInfo` and immutable `with_*` builders.
- `test_compare.cpp` — `operator==`, `operator<=>`, `std::hash`.
- `test_cell_helpers.cpp` — `parcel::cell()`, `Cell::of()`, `cell_cast`,
  `as`, `value_or`.
- `test_ergonomics.cpp` — `make_list` / `make_map`, ranges, `keys()` /
  `values()`, variadic registration helpers.
- `test_std_interop.cpp` — chrono cells, `PathCell`, hash-backed maps,
  default-cell inference for `std::array` / `std::deque` / `std::list`
  / `std::set`.
- `test_parcel.cpp` — core wire format and round-trip.

## Contributing

Contributions to the library are welcome! If you encounter any issues or have suggestions for
improvements,
please feel free to submit a pull request or open an issue on the project's repository.

## License

This project is licensed under the MIT License. See the [LICENSE](LICENSE) file for details.
