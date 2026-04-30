# parcel — examples

Each example below is built as part of `make build`. Run a single one with:

```sh
make build && ./build/examples/<target>
```

| target                           | file                          | demonstrates                                                                                                        | headers / APIs touched                                                                                               |
|----------------------------------|-------------------------------|---------------------------------------------------------------------------------------------------------------------|----------------------------------------------------------------------------------------------------------------------|
| `parcel_primitive_demo`          | `primitive_demo.cpp`          | Build primitive cells; `to_json` / `from_json`; descriptors; `clone()`.                                             | `PrimitiveCell<T>`, `I32Cell`, `BoolCell`, `StringCell`, `descriptor()`                                              |
| `parcel_list_demo`               | `list_demo.cpp`               | `TypedListCell<T>` (raw scalars on the wire) vs `ListCell` (full `{k,v}` objects).                                  | `TypedListCell`, `ListCell`, `std::vector` pass-through API                                                          |
| `parcel_map_demo`                | `map_demo.cpp`                | `TypedMapCell<T>` vs `MapCell`; `contains`/`at`/ordered iteration.                                                  | `TypedMapCell`, `MapCell`, `std::map` pass-through API                                                               |
| `parcel_struct_demo`             | `struct_demo.cpp`             | Define a payload + `StructCell`; required + optional + inferred-vector fields; round-trip.                          | `StructCell`, `FieldsBuilder`, `default_cell_for`, `IHasFields`                                                      |
| `parcel_struct_extras_demo`      | `struct_extras_demo.cpp`      | `allow_extra_fields = true` capturing unknown keys into `extras`; clone/re-emit preservation.                       | `StructCell::extras`, `allow_extra_fields`                                                                           |
| `parcel_union_demo`              | `union_demo.cpp`              | `UnionCell<Ts...>`; `index()`, `active_kind()`, `get<I>()`, `get<S>()`; round-trip.                                 | `UnionCell`                                                                                                          |
| `parcel_registry_demo`           | `registry_demo.cpp`           | Register kinds; polymorphic dispatch via `cell_from_json`; `find_by_*`; `define()` graph.                           | `ParcelRegistry`, `Definition`                                                                                       |
| `parcel_meta_demo`               | `meta_demo.cpp`               | Immutable `with_*` builders; `MetaInfo` round-trip via the `"d"` wire block.                                        | `ICell::with_meta` / `with_name` / `with_description` / `with_icon` / `with_color`                                   |
| `parcel_error_handling_demo`     | `error_handling_demo.cpp`     | Every documented `runtime_error` path: missing/wrong `k`                                                            | strictness contract across every cell category                                                                       |
| `parcel_defaults_demo`           | `defaults_demo.cpp`           | `default_cell_for<T>` field-type inference for primitives, optional, vector, map, variant.                          | `default_cell_for`, `default_cell_for_t`, inferred `FieldsBuilder::field<>`                                          |
| `parcel_custom_primitive_demo`   | `custom_primitive_demo.cpp`   | Recipe: define a brand-new primitive-style cell (`UuidCell`).                                                       | `BaseCell<Derived,Storage>`, `SimpleCellTypeDescriptor`, `default_cell_for`                                          |
| `parcel_custom_struct_demo`      | `custom_struct_demo.cpp`      | Recipe: define a brand-new struct-style cell that consumes the custom primitive.                                    | `StructCell<Derived,Payload>`, `FieldsBuilder`, transitive `Definition`                                              |
| `parcel_struct_inheritance_demo` | `struct_inheritance_demo.cpp` | DRY field-list reuse across `StructCell`s via `extend<ParentCell>()`, override-by-key, and `remove_field`.          | `FieldsBuilder::extend`, `FieldsBuilder::remove_field`, `InheritedFieldDescriptor`                                   |
| `parcel_nested_struct_demo`      | `nested_struct_demo.cpp`      | Nest one struct cell inside another; let `FieldsBuilder` infer the wrapper via `PARCEL_DEFAULT_CELL`.               | `StructCell`, `default_cell_for`, `PARCEL_DEFAULT_CELL`                                                              |
| `parcel_intrusive_struct_demo`   | `intrusive_struct_demo.cpp`   | `SelfStructCell<Derived>` + a CRTP `BaseEvent` that carves out its own `"s:event:…"` wire namespace.                | `SelfStructCell`, `id_join_lit_v`, `prefixed_id_v`, `fixed_string`                                                   |
| `parcel_cell_handle_demo`        | `cell_handle_demo.cpp`        | Working with a `cell_t` you already have: cast helpers, free `to_json` / `to_string`, hashing, tree walks.          | `cell_cast`, `cell_cast_or`, `try_cell_cast`, `as`, `value_or`, `walk_to_vector`, `std::hash<cell_t>`                |
| `parcel_format_io_demo`          | `format_io_demo.cpp`          | Every printing/serialization path: `std::format` specs, `operator<<`, stream/byte JSON I/O, `try_*` errors.         | `std::formatter<ICell>`, `parcel::print`, `cell_to_stream`, `cell_from_stream`, `cell_from_bytes`, `try_cell_from_*` |
| `parcel_ergonomics_demo`         | `ergonomics_demo.cpp`         | `parcel::cell` / `make_list` / `make_map`, range/span construction, `keys`/`values` views, variadic registry.       | `cell`, `make_list`, `make_map`, `from_range_t`, `as_span`, `keys`, `values`, `register_kinds`, `register_cells`     |
| `parcel_union_visit_demo`        | `union_visit_demo.cpp`        | Visiting and inspecting union alternatives by index or storage type, including the null-pointer-safe path.          | `UnionCell::visit`, `Overload`, `get_if`, `holds_alternative`, free `parcel::visit` / `get` / `get_if`               |
| `parcel_std_interop_demo`        | `std_interop_demo.cpp`        | Adapter cells for `std::chrono`, `std::filesystem::path`, hash-backed maps; struct field via `PARCEL_DEFAULT_CELL`. | `SystemTimePointCell`, `UnixMillisCell`, `DurationMsCell`, `YmdCell`, `PathCell`, `TypedHashMapCell`, `HashMapCell`  |

The `consumers/fetch_content/` directory is a **standalone downstream
project**. It is not a sub-target of this build; instead, it shows how to pull
parcel into another CMake project via `FetchContent`. See its own README for
how to run it locally without a published release.

## Reading order

If you are new to parcel, work through the examples in this order — each one
builds on the concepts of the previous:

1. `parcel_primitive_demo` — what a cell *is*.
2. `parcel_list_demo` — homogeneous vs heterogeneous containers.
3. `parcel_map_demo` — same idea, for maps.
4. `parcel_struct_demo` — modelling your own data.
5. `parcel_union_demo` — closed-set polymorphism.
6. `parcel_registry_demo` — how the wire becomes a cell.
7. `parcel_meta_demo` — annotating cells without mutating them.
8. `parcel_error_handling_demo` — what failure looks like.
9. `parcel_defaults_demo` — letting `FieldsBuilder` infer cell types.
10. `parcel_custom_primitive_demo` — extending parcel with your own primitive.
11. `parcel_custom_struct_demo` — composing custom primitives into structs.
12. `parcel_struct_inheritance_demo` — DRY field-list reuse via `extend<>` / `remove_field`.
13. `parcel_nested_struct_demo` — nesting struct cells via inferred wrappers.
14. `parcel_intrusive_struct_demo` — `SelfStructCell` and library-style CRTP bases.
15. `parcel_cell_handle_demo` — working with `cell_t` you already have.
16. `parcel_format_io_demo` — printing, formatting, stream / byte JSON I/O.
17. `parcel_ergonomics_demo` — heterogeneous builders, range views, variadic registry sugar.
18. `parcel_union_visit_demo` — visiting / inspecting union alternatives.
19. `parcel_std_interop_demo` — `std::chrono`, `std::filesystem`, hash maps.
20. `consumers/fetch_content/` — how a downstream project depends on parcel.
