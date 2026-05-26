#pragma once

/**
 * @file descriptor.h
 * @brief Runtime cell-type descriptors and the schema-graph mix-ins (`IHasFields`, `ISubTypes`).
 *
 * Descriptors are the runtime, type-erased face of the static cell types.
 * Each cell exposes its descriptor via the `CellLike::descriptor()` static,
 * and the `ParcelRegistry` keeps them keyed by kind id so polymorphic
 * dispatch (e.g. `MapCell` deserialization) can route to the right
 * `from_json`. See the README "Descriptors and the registry" section.
 */

#include <parcel/cell.h>
#include <parcel/common.h>

#include <map>
#include <memory>
#include <string>
#include <string_view>
#include <typeindex>
#include <typeinfo>
#include <utility>
#include <vector>

namespace parcel::descriptor {

/**
 * @brief Coarse runtime classification of a cell type.
 *
 * Mirrors the eight wire categories: `Primitive`, the typed and
 * heterogeneous list/map variants, structs, unions, and a `Custom` escape
 * hatch for user-supplied descriptors.
 */
enum class CellCategory { Primitive, TypedList, List, TypedMap, Map, Struct, Union, Custom };

NLOHMANN_JSON_SERIALIZE_ENUM(CellCategory,
                             {
                                 {CellCategory::Primitive, "primitive"},
                                 {CellCategory::TypedList, "typed_list"},
                                 {CellCategory::List, "list"},
                                 {CellCategory::TypedMap, "typed_map"},
                                 {CellCategory::Map, "map"},
                                 {CellCategory::Struct, "struct"},
                                 {CellCategory::Union, "union"},
                                 {CellCategory::Custom, "custom"},
                             })

}  // namespace parcel::descriptor

namespace parcel {
class ParcelRegistry;

/**
 * @brief Runtime descriptor for a cell type — type-erased face of a `CellLike`.
 *
 * Held by the registry and used to dispatch deserialization into the right
 * concrete cell when only a kind id is known at runtime.
 *
 * @see BaseCellTypeDescriptor — CRTP base used by every concrete descriptor.
 */
struct ICellTypeDescriptor {
    virtual ~ICellTypeDescriptor() = default;

    /** @brief Wire-stable kind identifier. */
    [[nodiscard]] virtual std::string_view kind() const = 0;

    /** @brief Display info for this cell type. */
    [[nodiscard]] virtual DisplayInfo display_info() const = 0;

    /** @brief Coarse classification (primitive, list, struct, …). */
    [[nodiscard]] virtual descriptor::CellCategory category() const = 0;

    /** @brief `std::type_index` of the cell's storage type. */
    [[nodiscard]] virtual std::type_index storage_type() const = 0;

    /**
     * @brief Construct a cell from JSON, dispatching to the concrete `from_json`.
     * @param j   JSON object to deserialize.
     * @param reg Registry used to resolve nested polymorphic cells.
     * @return Newly built cell handle.
     * @throws std::runtime_error on shape or kind mismatch.
     */
    [[nodiscard]] virtual cell_t cell_from_json(const json_t& j,
                                                ParcelRegistry const& reg) const = 0;

    /**
     * @brief Serialize the descriptor itself (kind + display info + category + extras).
     * @return JSON object describing this cell type.
     */
    [[nodiscard]] virtual json_t to_json() const = 0;
};

/**
 * @brief Runtime descriptor for a single struct field.
 *
 * @see StructCell — host that aggregates field descriptors.
 */
struct IFieldDescriptor {
    virtual ~IFieldDescriptor() = default;

    /** @brief JSON key under which this field appears in `"v"`. */
    [[nodiscard]] virtual std::string_view key() const = 0;

    /** @brief Cell kind id for the field's value type. */
    [[nodiscard]] virtual std::string_view kind() const = 0;

    /** @brief Display info for this field. */
    [[nodiscard]] virtual DisplayInfo display_info() const = 0;

    /** @brief Whether the field must be present on deserialization. */
    [[nodiscard]] virtual bool is_required() const = 0;

    /**
     * @brief Serialize the field descriptor itself.
     * @return JSON object with `key`, `kind`, `display_info`, `required`.
     */
    [[nodiscard]] virtual json_t to_json() const = 0;
};

/** @brief Shared handle to a field descriptor. */
using field_descriptor_t = std::shared_ptr<IFieldDescriptor>;

/** @brief Ordered map of field key → descriptor. */
using field_descriptors_t = std::map<std::string, field_descriptor_t>;

/**
 * @brief Mix-in declaring a descriptor exposes a static set of named fields.
 *
 * Implemented by `StructCellTypeDescriptor`. Used by tooling that walks the
 * schema graph.
 */
struct IHasFields {
    virtual ~IHasFields() = default;

    /** @brief Field descriptors keyed by JSON key. */
    [[nodiscard]] virtual field_descriptors_t fields() const = 0;
};

/**
 * @brief Mix-in declaring a descriptor refers to other registered cell kinds.
 *
 * Implemented by container descriptors (`TypedListCellTypeDescriptor`,
 * `TypedMapCellTypeDescriptor`, `UnionCellTypeDescriptor`,
 * `StructCellTypeDescriptor`). Drives `ParcelRegistry::define`'s
 * referenced-kind walk.
 */
struct ISubTypes {
    virtual ~ISubTypes() = default;

    /** @brief Sub-kind ids reachable from this descriptor. */
    [[nodiscard]] virtual std::vector<std::string_view> sub_kinds() const = 0;
};

/**
 * @brief CRTP base that fills in the boilerplate of every cell-type descriptor.
 *
 * Concrete descriptors inherit `BaseCellTypeDescriptor<T>` and provide the
 * `category()` plus, when relevant, `cell_from_json` and `to_json`
 * overrides.
 *
 * @tparam T A `CellLike` cell type whose descriptor this is.
 */
template <CellLike T>
class BaseCellTypeDescriptor : public virtual ICellTypeDescriptor {
public:
    using cell_type = T;

    /** @brief Wire-stable kind id, lifted from `T::kind_id`. */
    static constexpr std::string_view kind_id = T::kind_id;

    /**
     * @brief Construct with the given display info.
     * @param info Display info.
     */
    explicit BaseCellTypeDescriptor(DisplayInfo info) : display_info_(std::move(info)) {}

    [[nodiscard]] std::string_view kind() const override {
        return kind_id;
    }

    [[nodiscard]] DisplayInfo display_info() const override {
        return display_info_;
    }

    [[nodiscard]] std::type_index storage_type() const override {
        return std::type_index(typeid(typename T::storage_t));
    }

    [[nodiscard]] json_t to_json() const override {
        return base_to_json();
    }

protected:
    /** @brief Stored display info. */
    DisplayInfo display_info_;

    /**
     * @brief Common JSON skeleton (`kind`, `display_info`, `category`) reused by overrides.
     * @return Base JSON object that derived `to_json` overrides extend.
     */
    [[nodiscard]] json_t base_to_json() const {
        return json_t{
            {"kind", kind_id},
            {"display_info", json_t(display_info_)},
            {"category", this->category()},
        };
    }
};

/**
 * @brief Plain descriptor for cells in the `Primitive` category.
 *
 * Used by `PrimitiveCell<T>`, `ListCell`, and `MapCell` — anywhere there is
 * nothing more to expose beyond the base shape.
 *
 * @tparam T A `CellLike` cell type.
 */
template <CellLike T>
class SimpleCellTypeDescriptor final : public BaseCellTypeDescriptor<T> {
public:
    using cell_type = T;

    /**
     * @brief Construct with the given display info.
     * @param info Display info.
     */
    explicit SimpleCellTypeDescriptor(DisplayInfo info)
        : BaseCellTypeDescriptor<T>(std::move(info)) {}

    [[nodiscard]] descriptor::CellCategory category() const override {
        return descriptor::CellCategory::Primitive;
    }

    [[nodiscard]] cell_t cell_from_json(const json_t& j, const ParcelRegistry& reg) const override {
        return T::from_json(j, reg);
    }
};

}  // namespace parcel
