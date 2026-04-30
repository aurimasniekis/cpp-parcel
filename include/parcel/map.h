#pragma once

/**
 * @file map.h
 * @brief `TypedMapCell<T>` and heterogeneous `MapCell` with their descriptors.
 *
 * Two map flavors share this header. `TypedMapCell<T>` carries a
 * compile-time element kind, encoded in its kind id (e.g. `"m:i32"`) and
 * stored as `std::map<std::string, typename T::storage_t>` so values are
 * raw — no per-cell wrapping. `MapCell` holds `std::map<std::string,
 * cell_t>` for heterogeneous values and dispatches each value through the
 * registry. See the README "Lists and maps" section.
 */

#include <parcel/cell.h>
#include <parcel/descriptor.h>
#include <parcel/registry.h>

#include <initializer_list>
#include <map>
#include <memory>
#include <ranges>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace parcel {

// std::map<K, V>::operator== eagerly instantiates V::operator==; suppress
// BaseCell's default compare for map storage so cells whose value payloads
// lack `==` still compile (TypedMapCell overrides compare()).
template <typename K, typename V, typename C, typename A>
struct detail::disables_basecell_compare<std::map<K, V, C, A>> : std::true_type {
};  // namespace detail

class ParcelRegistry;

template <CellLike T>
class TypedMapCell;

/**
 * @brief Descriptor for `TypedMapCell<T>`.
 *
 * Adds an `"element_kind"` field to the JSON descriptor and exposes the
 * element kind through `ISubTypes` so the registry's schema walker reaches
 * the inner type.
 *
 * @tparam T Element cell type.
 */
template <CellLike T>
class TypedMapCellTypeDescriptor final : public BaseCellTypeDescriptor<TypedMapCell<T>>,
                                         public ISubTypes {
public:
    using cell_type = TypedMapCell<T>;
    using base_t = BaseCellTypeDescriptor<TypedMapCell<T>>;

    /**
     * @brief Construct with the given descriptive metadata.
     * @param meta Descriptive metadata.
     */
    explicit TypedMapCellTypeDescriptor(descriptor::MetaInfo meta) : base_t(std::move(meta)) {}

    /** @brief Wire kind id of the map's value type. */
    [[nodiscard]] std::string_view element_kind() const {
        return T::kind_id;
    }

    [[nodiscard]] descriptor::CellCategory category() const override {
        return descriptor::CellCategory::TypedMap;
    }

    [[nodiscard]] std::vector<std::string_view> sub_kinds() const override {
        return {T::kind_id};
    }

    [[nodiscard]] cell_t cell_from_json(json_t const& j, ParcelRegistry const& reg) const override {
        return TypedMapCell<T>::from_json(j, reg);
    }

    [[nodiscard]] json_t to_json() const override {
        auto j = base_t::base_to_json();
        j["element_kind"] = T::kind_id;
        return j;
    }
};

/**
 * @brief Homogeneous string-keyed map of values of type @p T.
 *
 * Wire shape (example for `TypedMapCell<I32Cell>`):
 * @code{.json}
 * {"k": "m:i32", "v": {"a": 1, "b": 2}}
 * @endcode
 * The element kind is part of `"k"` so the registry can dispatch each
 * typed map distinctly. Storage is
 * `std::map<std::string, typename T::storage_t>`, so values are raw and a
 * curated subset of `std::map`'s API is forwarded for ergonomic use.
 *
 * @tparam T Value cell type (must satisfy `CellLike`).
 * @see TypedMapCellTypeDescriptor
 */
template <CellLike T>
class TypedMapCell
    : public BaseCell<TypedMapCell<T>, std::map<std::string, typename T::storage_t>> {
    using base_t = BaseCell<TypedMapCell<T>, std::map<std::string, typename T::storage_t>>;

public:
    using element_type = T;
    using element_storage_t = typename T::storage_t;

    using base_t::base_t;
    using base_t::operator=;

    /**
     * @brief Construct from a brace-enclosed list of `{key, value}` pairs.
     * @param elems Initializer list of key/value pairs.
     */
    TypedMapCell(std::initializer_list<std::pair<const std::string, element_storage_t>> elems)
        : base_t(std::map<std::string, element_storage_t>(elems)) {}

    /**
     * @brief Construct from any input range whose elements are `{key, value}`-like.
     */
    template <std::ranges::input_range R>
        requires requires(std::ranges::range_value_t<R> p) {
            { p.first } -> std::convertible_to<std::string>;
            { p.second } -> std::convertible_to<element_storage_t>;
        }
    TypedMapCell(std::from_range_t /*tag*/, R&& r)
        : base_t(std::map<std::string, element_storage_t>(std::ranges::begin(r),
                                                          std::ranges::end(r))) {}

    /**
     * @brief Lazy view over the keys of the map.
     *
     * Equivalent to `value | std::views::keys`, but slightly shorter at the
     * call site and self-documenting on `TypedMapCell`.
     */
    [[nodiscard]] auto keys() const noexcept {
        return this->value | std::views::keys;
    }

    /** @brief Lazy view over the values of the map. */
    [[nodiscard]] auto values() const noexcept {
        return this->value | std::views::values;
    }

    // ---- std::map pass-through API ---------------------------------------
    // Transparent map: values are raw element_storage_t, not ICell wrappers.

    using map_t = std::map<std::string, element_storage_t>;
    using key_type = map_t::key_type;
    using mapped_type = map_t::mapped_type;
    using value_type = map_t::value_type;
    using size_type = map_t::size_type;
    using iterator = map_t::iterator;
    using const_iterator = map_t::const_iterator;

    [[nodiscard]] size_type size() const noexcept {
        return this->value.size();
    }
    [[nodiscard]] bool empty() const noexcept {
        return this->value.empty();
    }

    mapped_type& operator[](key_type const& k) {
        return this->value[k];
    }
    mapped_type& operator[](key_type&& k) {
        return this->value[std::move(k)];
    }
    mapped_type& at(key_type const& k) {
        return this->value.at(k);
    }
    [[nodiscard]] mapped_type const& at(key_type const& k) const {
        return this->value.at(k);
    }

    iterator find(key_type const& k) {
        return this->value.find(k);
    }
    [[nodiscard]] const_iterator find(key_type const& k) const {
        return this->value.find(k);
    }
    [[nodiscard]] bool contains(key_type const& k) const {
        return this->value.contains(k);
    }
    [[nodiscard]] size_type count(key_type const& k) const {
        return this->value.count(k);
    }

    iterator begin() noexcept {
        return this->value.begin();
    }
    [[nodiscard]] const_iterator begin() const noexcept {
        return this->value.begin();
    }
    [[nodiscard]] const_iterator cbegin() const noexcept {
        return this->value.cbegin();
    }
    iterator end() noexcept {
        return this->value.end();
    }
    [[nodiscard]] const_iterator end() const noexcept {
        return this->value.end();
    }
    [[nodiscard]] const_iterator cend() const noexcept {
        return this->value.cend();
    }

    std::pair<iterator, bool> insert(value_type const& v) {
        return this->value.insert(v);
    }
    std::pair<iterator, bool> insert(value_type&& v) {
        return this->value.insert(std::move(v));
    }
    template <typename... Args>
    std::pair<iterator, bool> emplace(Args&&... args) {
        return this->value.emplace(std::forward<Args>(args)...);
    }
    size_type erase(key_type const& k) {
        return this->value.erase(k);
    }
    iterator erase(const_iterator pos) {
        return this->value.erase(pos);
    }
    void clear() noexcept {
        this->value.clear();
    }
    // ----------------------------------------------------------------------

    /**
     * @brief Wire kind id of this typed map (`"m:" + T::kind_id`).
     *
     * Encoded in the type so each instantiation registers under a distinct
     * kind:
     * @code{.cpp}
     * TypedMapCell<I32Cell>::kind_id    == "m:i32";
     * TypedMapCell<StringCell>::kind_id == "m:string";
     * @endcode
     */
    static constexpr std::string_view kind_id = prefixed_id_v<"m:", T::kind_id>;

    /**
     * @brief Render the map as `{k1: v1, k2: v2}` using each value's `to_string`.
     * @return Brace-enclosed comma-separated form.
     */
    [[nodiscard]] std::string to_string() const override {
        std::string out = "{";
        bool first = true;
        for (auto const& [key, el] : this->value) {
            if (!first) {
                out += ", ";
            }
            out += key;
            out += ": ";
            T wrapper(el);
            out += wrapper.to_string();
            first = false;
        }
        out += "}";
        return out;
    }

    [[nodiscard]] json_t to_json() const override {
        json_t obj = json_t::object();
        for (auto const& [key, el] : this->value) {
            T wrapper(el);
            obj[key] = wrapper.to_json().at(ICell::KEY_VALUE);
        }
        json_t j{
            {ICell::KEY_KIND, kind_id},
            {ICell::KEY_VALUE, std::move(obj)},
        };
        this->inject_meta(j);
        return j;
    }

    /**
     * @brief Lexicographic three-way comparison over (key, value) pairs;
     *        ignores meta.
     *
     * Each value pair is wrapped in `T` and routed through `T::compare` so
     * element types whose raw storage lacks `==` still compare correctly.
     */
    [[nodiscard]] std::partial_ordering compare(ICell const& other) const override {
        if (const auto k_cmp = this->kind() <=> other.kind(); k_cmp != 0) {
            return k_cmp;
        }
        const auto* o = dynamic_cast<TypedMapCell const*>(&other);
        if (o == nullptr) {
            return std::partial_ordering::unordered;
        }
        auto it_a = this->value.begin();
        auto it_b = o->value.begin();
        const auto end_a = this->value.end();
        const auto end_b = o->value.end();
        while (it_a != end_a && it_b != end_b) {
            if (const auto kc = it_a->first <=> it_b->first; kc != 0) {
                return kc;
            }
            T wa(it_a->second);
            T wb(it_b->second);
            if (const auto vc = wa.compare(wb); vc != 0) {
                return vc;
            }
            ++it_a;
            ++it_b;
        }
        return this->value.size() <=> o->value.size();
    }

    /**
     * @brief Deserialize a `TypedMapCell<T>` from JSON.
     * @param j   Input JSON object — must match `{"k": kind_id, "v": {…}}`.
     * @param reg Registry (passed through to per-value `from_json`).
     * @return Newly built cell handle.
     * @throws std::runtime_error on shape or kind mismatch, or if any
     *         value fails to deserialize as @p T.
     */
    static cell_t from_json(json_t const& j, ParcelRegistry const& reg) {
        if (!j.is_object()) {
            throw InvalidJsonException("Expected JSON object for TypedMapCell",
                                       std::string(kind_id));
        }

        const auto it_k = j.find(ICell::KEY_KIND);
        if (it_k == j.end() || !it_k->is_string()) {
            throw InvalidJsonException("TypedMapCell: missing/invalid 'k' (expected '" +
                                           std::string(kind_id) + "')",
                                       std::string(kind_id));
        }
        if (it_k->get<std::string_view>() != kind_id) {
            throw KindMismatchException("TypedMapCell: missing/invalid 'k' (expected '" +
                                            std::string(kind_id) + "')",
                                        std::string(kind_id));
        }

        const auto it_v = j.find(ICell::KEY_VALUE);
        if (it_v == j.end() || !it_v->is_object()) {
            throw InvalidJsonException("TypedMapCell: missing/invalid 'v' (expected object)",
                                       std::string(kind_id));
        }

        std::map<std::string, element_storage_t> elems;
        for (auto const& [key, raw] : it_v->items()) {
            json_t wrapped{{ICell::KEY_KIND, T::kind_id}, {ICell::KEY_VALUE, raw}};
            cell_t v = T::from_json(wrapped, reg);
            auto* typed = dynamic_cast<T*>(v.get());
            if (typed == nullptr) {
                throw TypeException(std::string("TypedMapCell: element from_json did not return ") +
                                        std::string(T::kind_id),
                                    std::string(kind_id));
            }
            elems.emplace(key, std::move(typed->value));
        }
        auto cell = std::make_shared<TypedMapCell<T>>(std::move(elems));
        base_t::absorb_meta(j, cell);
        return cell;
    }

    /**
     * @brief Cached descriptor for this typed map.
     * @return Shared `TypedMapCellTypeDescriptor<T>` instance.
     */
    static cell_type_descriptor_t descriptor() {
        static const auto d = std::make_shared<TypedMapCellTypeDescriptor<T>>(descriptor::MetaInfo{
            .name = "Map",
            .description = std::string("Homogeneous map of ") + std::string(T::kind_id),
        });
        return d;
    }
};

/**
 * @brief Heterogeneous string-keyed map of `cell_t` — wire kind `"m"`.
 *
 * Wire shape:
 * @code{.json}
 * {"k": "m", "v": {"a": {"k":"i32","v":1}, "b": {"k":"string","v":"hi"}}}
 * @endcode
 * Values are full nested cells whose kind is recovered from each value's
 * own `"k"`. Use `TypedMapCell<T>` instead when every value has the same
 * wire kind — the typed variant skips the per-value kind tag.
 *
 * @see TypedMapCell — homogeneous variant with element-tagged kind id.
 */
class MapCell : public BaseCell<MapCell, std::map<std::string, cell_t>> {
    using base_t = BaseCell<MapCell, std::map<std::string, cell_t>>;

public:
    using base_t::base_t;
    using base_t::operator=;

    /**
     * @brief Construct from a brace-enclosed list of `{key, cell}` pairs.
     * @param elems Initializer list of key/cell pairs.
     */
    MapCell(const std::initializer_list<std::pair<const std::string, cell_t>> elems)
        : base_t(std::map<std::string, cell_t>(elems)) {}

    /** @brief Lazy view over the keys of the map. */
    [[nodiscard]] auto keys() const noexcept {
        return this->value | std::views::keys;
    }

    /** @brief Lazy view over the cell values of the map. */
    [[nodiscard]] auto values() const noexcept {
        return this->value | std::views::values;
    }

    // ---- std::map pass-through API ---------------------------------------

    using map_t = std::map<std::string, cell_t>;
    using key_type = map_t::key_type;
    using mapped_type = map_t::mapped_type;
    using value_type = map_t::value_type;
    using size_type = map_t::size_type;
    using iterator = map_t::iterator;
    using const_iterator = map_t::const_iterator;

    [[nodiscard]] size_type size() const noexcept {
        return this->value.size();
    }
    [[nodiscard]] bool empty() const noexcept {
        return this->value.empty();
    }

    mapped_type& operator[](key_type const& k) {
        return this->value[k];
    }
    mapped_type& operator[](key_type&& k) {
        return this->value[std::move(k)];
    }
    mapped_type& at(key_type const& k) {
        return this->value.at(k);
    }
    [[nodiscard]] mapped_type const& at(key_type const& k) const {
        return this->value.at(k);
    }

    iterator find(key_type const& k) {
        return this->value.find(k);
    }
    [[nodiscard]] const_iterator find(key_type const& k) const {
        return this->value.find(k);
    }
    [[nodiscard]] bool contains(key_type const& k) const {
        return this->value.contains(k);
    }
    [[nodiscard]] size_type count(key_type const& k) const {
        return this->value.count(k);
    }

    iterator begin() noexcept {
        return this->value.begin();
    }
    [[nodiscard]] const_iterator begin() const noexcept {
        return this->value.begin();
    }
    [[nodiscard]] const_iterator cbegin() const noexcept {
        return this->value.cbegin();
    }
    iterator end() noexcept {
        return this->value.end();
    }
    [[nodiscard]] const_iterator end() const noexcept {
        return this->value.end();
    }
    [[nodiscard]] const_iterator cend() const noexcept {
        return this->value.cend();
    }

    std::pair<iterator, bool> insert(value_type const& v) {
        return this->value.insert(v);
    }
    std::pair<iterator, bool> insert(value_type&& v) {
        return this->value.insert(std::move(v));
    }
    template <typename... Args>
    std::pair<iterator, bool> emplace(Args&&... args) {
        return this->value.emplace(std::forward<Args>(args)...);
    }
    size_type erase(key_type const& k) {
        return this->value.erase(k);
    }
    iterator erase(const const_iterator pos) {
        return this->value.erase(pos);
    }
    void clear() noexcept {
        this->value.clear();
    }
    // ----------------------------------------------------------------------

    /** @brief Wire kind id (`"m"`). */
    static constexpr std::string_view kind_id = "m";

    /**
     * @brief Render the map as `{k1: v1, …}`; null values appear as `<null>`.
     * @return Brace-enclosed comma-separated form.
     */
    [[nodiscard]] std::string to_string() const override {
        std::string out = "{";
        bool first = true;
        for (auto const& [key, el] : this->value) {
            if (!first) {
                out += ", ";
            }
            out += key;
            out += ": ";
            out += el ? el->to_string() : "<null>";
            first = false;
        }
        out += "}";
        return out;
    }

    [[nodiscard]] json_t to_json() const override {
        json_t obj = json_t::object();
        for (auto const& [key, el] : this->value) {
            obj[key] = el ? el->to_json() : json_t();
        }
        json_t j{
            {ICell::KEY_KIND, kind_id},
            {ICell::KEY_VALUE, std::move(obj)},
        };
        this->inject_meta(j);
        return j;
    }

    [[nodiscard]] cell_t clone() const override {
        std::map<std::string, cell_t> copied;
        for (auto const& [key, el] : this->value) {
            copied.emplace(key, el ? el->clone() : nullptr);
        }
        auto out = std::make_shared<MapCell>(std::move(copied));
        out->meta_ = this->meta_;
        return out;
    }

    [[nodiscard]] std::size_t hash_value() const noexcept override {
        std::size_t h = std::hash<std::string_view>{}(kind());
        // std::map iterates in sorted key order, so the digest is canonical.
        for (auto const& [key, el] : this->value) {
            const std::size_t hk = std::hash<std::string>{}(key);
            const std::size_t hv = el ? el->hash_value() : std::size_t{0};
            h ^= hk + 0x9e3779b97f4a7c15ULL + (h << 6) + (h >> 2);
            h ^= hv + 0x9e3779b97f4a7c15ULL + (h << 6) + (h >> 2);
        }
        return h;
    }

    /**
     * @brief Lexicographic three-way comparison over keys then values; ignores meta.
     *
     * Walks the (sorted) key sequences in lockstep. Null values compare less
     * than non-null at the same key.
     */
    [[nodiscard]] std::partial_ordering compare(ICell const& other) const override {
        if (const auto k_cmp = this->kind() <=> other.kind(); k_cmp != 0) {
            return k_cmp;
        }
        const auto* o = dynamic_cast<MapCell const*>(&other);
        if (o == nullptr) {
            return std::partial_ordering::unordered;
        }
        auto it_a = this->value.begin();
        auto it_b = o->value.begin();
        const auto end_a = this->value.end();
        const auto end_b = o->value.end();
        while (it_a != end_a && it_b != end_b) {
            if (const auto kc = it_a->first <=> it_b->first; kc != 0) {
                return kc;
            }
            if (it_a->second && it_b->second) {
                if (const auto vc = it_a->second->compare(*it_b->second); vc != 0) {
                    return vc;
                }
            } else if (!it_a->second && it_b->second) {
                return std::partial_ordering::less;
            } else if (it_a->second && !it_b->second) {
                return std::partial_ordering::greater;
            }
            ++it_a;
            ++it_b;
        }
        return this->value.size() <=> o->value.size();
    }

    /**
     * @brief Deserialize a `MapCell` from JSON.
     * @param j   Input JSON object — must match `{"k": "m", "v": {…}}`.
     * @param reg Registry used to dispatch each value by its own kind.
     * @return Newly built cell handle.
     * @throws std::runtime_error on shape mismatch or unknown value kind.
     */
    static cell_t from_json(json_t const& j, ParcelRegistry const& reg) {
        if (!j.is_object()) {
            throw InvalidJsonException("Expected JSON object for MapCell", std::string(kind_id));
        }

        const auto it_k = j.find(ICell::KEY_KIND);
        if (it_k == j.end() || !it_k->is_string()) {
            throw InvalidJsonException("MapCell: missing/invalid 'k' (expected 'm')",
                                       std::string(kind_id));
        }
        if (it_k->get<std::string_view>() != kind_id) {
            throw KindMismatchException("MapCell: missing/invalid 'k' (expected 'm')",
                                        std::string(kind_id));
        }

        const auto it_v = j.find(ICell::KEY_VALUE);
        if (it_v == j.end() || !it_v->is_object()) {
            throw InvalidJsonException("MapCell: missing/invalid 'v' (expected object)",
                                       std::string(kind_id));
        }

        std::map<std::string, cell_t> elems;
        for (auto const& [key, raw] : it_v->items()) {
            elems.emplace(key, raw.is_null() ? cell_t{} : reg.cell_from_json(raw));
        }
        auto cell = std::make_shared<MapCell>(std::move(elems));
        base_t::absorb_meta(j, cell);
        return cell;
    }

    /**
     * @brief Cached descriptor for `MapCell`.
     * @return Shared descriptor instance.
     */
    static cell_type_descriptor_t descriptor();
};

/**
 * @brief Descriptor for the heterogeneous `MapCell`.
 *
 * Reports `CellCategory::Map` so registry consumers can distinguish the
 * heterogeneous map from typed maps and primitives.
 */
class MapCellTypeDescriptor final : public BaseCellTypeDescriptor<MapCell> {
public:
    using cell_type = MapCell;
    using base_t = BaseCellTypeDescriptor<MapCell>;

    explicit MapCellTypeDescriptor(descriptor::MetaInfo meta) : base_t(std::move(meta)) {}

    [[nodiscard]] descriptor::CellCategory category() const override {
        return descriptor::CellCategory::Map;
    }

    [[nodiscard]] cell_t cell_from_json(const json_t& j, const ParcelRegistry& reg) const override {
        return MapCell::from_json(j, reg);
    }
};

inline cell_type_descriptor_t MapCell::descriptor() {
    static const auto d = std::make_shared<MapCellTypeDescriptor>(
        descriptor::MetaInfo{.name = "Map", .description = "Heterogeneous map of cells"});
    return d;
}

}  // namespace parcel
