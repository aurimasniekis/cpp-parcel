#pragma once

/**
 * @file unordered_map.h
 * @brief Hash-backed map cells — `TypedHashMapCell<T>` and `HashMapCell`.
 *
 * A faster sibling of `TypedMapCell` / `MapCell` for hot paths. The wire
 * shape is the same (a JSON object), but the storage is
 * `std::unordered_map<std::string, …>` so iteration order is implementation-
 * defined. Key order is sorted *on the wire* so two equal logical maps
 * produce identical JSON regardless of insertion order.
 */

#include <parcel/cell.h>
#include <parcel/descriptor.h>
#include <parcel/registry.h>

#include <algorithm>
#include <initializer_list>
#include <map>
#include <memory>
#include <ranges>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

namespace parcel {

template <typename K, typename V, typename H, typename E, typename A>
struct detail::disables_basecell_compare<std::unordered_map<K, V, H, E, A>> : std::true_type {
};  // namespace detail

template <CellLike T>
class TypedHashMapCell;

template <CellLike T>
class TypedHashMapCellTypeDescriptor final : public BaseCellTypeDescriptor<TypedHashMapCell<T>>,
                                             public ISubTypes {
public:
    using cell_type = TypedHashMapCell<T>;
    using base_t = BaseCellTypeDescriptor<TypedHashMapCell<T>>;

    explicit TypedHashMapCellTypeDescriptor(DisplayInfo meta) : base_t(std::move(meta)) {}

    [[nodiscard]] descriptor::CellCategory category() const override {
        return descriptor::CellCategory::TypedMap;
    }

    [[nodiscard]] std::vector<std::string_view> sub_kinds() const override {
        return {T::kind_id};
    }

    [[nodiscard]] cell_t cell_from_json(json_t const& j, ParcelRegistry const& reg) const override {
        return TypedHashMapCell<T>::from_json(j, reg);
    }

    [[nodiscard]] json_t to_json() const override {
        auto j = base_t::base_to_json();
        j["element_kind"] = T::kind_id;
        return j;
    }
};

/**
 * @brief Homogeneous string-keyed `unordered_map` of values of type @p T.
 *
 * Wire kind: `"hm:" + T::kind_id`. Iteration order in storage is
 * unspecified; serialization sorts keys so the wire form is canonical.
 */
template <CellLike T>
class TypedHashMapCell
    : public BaseCell<TypedHashMapCell<T>, std::unordered_map<std::string, typename T::storage_t>> {
    using base_t =
        BaseCell<TypedHashMapCell<T>, std::unordered_map<std::string, typename T::storage_t>>;

public:
    using element_type = T;
    using element_storage_t = typename T::storage_t;
    using map_t = std::unordered_map<std::string, element_storage_t>;

    using base_t::base_t;
    using base_t::operator=;

    TypedHashMapCell(std::initializer_list<std::pair<const std::string, element_storage_t>> elems)
        : base_t(map_t(elems)) {}

    [[nodiscard]] std::size_t size() const noexcept {
        return this->value.size();
    }
    [[nodiscard]] bool empty() const noexcept {
        return this->value.empty();
    }

    element_storage_t& operator[](std::string const& k) {
        return this->value[k];
    }
    [[nodiscard]] element_storage_t const& at(std::string const& k) const {
        return this->value.at(k);
    }
    [[nodiscard]] bool contains(std::string const& k) const {
        return this->value.contains(k);
    }
    auto begin() noexcept {
        return this->value.begin();
    }
    auto end() noexcept {
        return this->value.end();
    }
    [[nodiscard]] auto begin() const noexcept {
        return this->value.begin();
    }
    [[nodiscard]] auto end() const noexcept {
        return this->value.end();
    }

    static constexpr std::string_view kind_id = prefixed_id_v<"hm:", T::kind_id>;

    [[nodiscard]] std::string to_string() const override {
        std::string out = "{";
        bool first = true;
        for (auto const& [k, vp] : sorted_view()) {
            if (!first) {
                out += ", ";
            }
            out += k;
            out += ": ";
            T wrapper(*vp);
            out += wrapper.to_string();
            first = false;
        }
        out += "}";
        return out;
    }

    [[nodiscard]] json_t to_json() const override {
        json_t obj = json_t::object();
        for (auto const& [k, vp] : sorted_view()) {
            T wrapper(*vp);
            obj[std::string(k)] = wrapper.to_json().at(ICell::KEY_VALUE);
        }
        json_t j{
            {ICell::KEY_KIND, kind_id},
            {ICell::KEY_VALUE, std::move(obj)},
        };
        this->inject_meta(j);
        return j;
    }

    [[nodiscard]] std::partial_ordering compare(ICell const& other) const override {
        if (const auto k_cmp = this->kind() <=> other.kind(); k_cmp != 0) {
            return k_cmp;
        }
        const auto* o = dynamic_cast<TypedHashMapCell const*>(&other);
        if (o == nullptr) {
            return std::partial_ordering::unordered;
        }
        auto a = sorted_view();
        auto b = o->sorted_view();
        const std::size_t n = std::min(a.size(), b.size());
        for (std::size_t i = 0; i < n; ++i) {
            if (const auto kc = a[i].first <=> b[i].first; kc != 0) {
                return kc;
            }
            T wa(*a[i].second);
            T wb(*b[i].second);
            if (const auto vc = wa.compare(wb); vc != 0) {
                return vc;
            }
        }
        return a.size() <=> b.size();
    }

    static cell_t from_json(json_t const& j, ParcelRegistry const& reg) {
        if (!j.is_object()) {
            throw InvalidJsonException("Expected JSON object for TypedHashMapCell",
                                       std::string(kind_id));
        }
        const auto it_k = j.find(ICell::KEY_KIND);
        if (it_k == j.end() || !it_k->is_string()) {
            throw InvalidJsonException("TypedHashMapCell: missing/invalid 'k' (expected '" +
                                           std::string(kind_id) + "')",
                                       std::string(kind_id));
        }
        if (it_k->get<std::string_view>() != kind_id) {
            throw KindMismatchException("TypedHashMapCell: missing/invalid 'k' (expected '" +
                                            std::string(kind_id) + "')",
                                        std::string(kind_id));
        }
        const auto it_v = j.find(ICell::KEY_VALUE);
        if (it_v == j.end() || !it_v->is_object()) {
            throw InvalidJsonException("TypedHashMapCell: missing/invalid 'v' (expected object)",
                                       std::string(kind_id));
        }

        map_t elems;
        for (auto const& [key, raw] : it_v->items()) {
            json_t wrapped{{ICell::KEY_KIND, T::kind_id}, {ICell::KEY_VALUE, raw}};
            cell_t v = T::from_json(wrapped, reg);
            auto* typed = dynamic_cast<T*>(v.get());
            if (typed == nullptr) {
                throw TypeException("TypedHashMapCell: element from_json mismatch",
                                    std::string(kind_id));
            }
            elems.emplace(key, std::move(typed->value));
        }
        auto cell = std::make_shared<TypedHashMapCell<T>>(std::move(elems));
        base_t::absorb_meta(j, cell);
        return cell;
    }

    static cell_type_descriptor_t descriptor() {
        static const auto d = std::make_shared<TypedHashMapCellTypeDescriptor<T>>(DisplayInfo{
            .name = "HashMap",
            .description = std::string("Hash-backed map of ") + std::string(T::kind_id),
        });
        return d;
    }

private:
    [[nodiscard]] std::vector<std::pair<std::string_view, element_storage_t const*>>
    sorted_view() const {
        std::vector<std::pair<std::string_view, element_storage_t const*>> out;
        out.reserve(this->value.size());
        for (auto const& [k, v] : this->value) {
            out.emplace_back(std::string_view(k), &v);
        }
        std::ranges::sort(out, {}, &std::pair<std::string_view, element_storage_t const*>::first);
        return out;
    }
};

/**
 * @brief Heterogeneous string-keyed `unordered_map` of `cell_t`.
 *
 * Wire kind: `"hm"`. Same canonicalization rule as `TypedHashMapCell` —
 * sorted on the wire, unsorted in memory.
 */
class HashMapCell : public BaseCell<HashMapCell, std::unordered_map<std::string, cell_t>> {
    using base_t = BaseCell<HashMapCell, std::unordered_map<std::string, cell_t>>;

public:
    using base_t::base_t;
    using base_t::operator=;

    HashMapCell(const std::initializer_list<std::pair<const std::string, cell_t>> elems)
        : base_t(std::unordered_map<std::string, cell_t>(elems)) {}

    [[nodiscard]] std::size_t size() const noexcept {
        return this->value.size();
    }
    [[nodiscard]] bool empty() const noexcept {
        return this->value.empty();
    }

    cell_t& operator[](std::string const& k) {
        return this->value[k];
    }
    [[nodiscard]] cell_t const& at(std::string const& k) const {
        return this->value.at(k);
    }
    [[nodiscard]] bool contains(std::string const& k) const {
        return this->value.contains(k);
    }

    static constexpr std::string_view kind_id = "hm";

    [[nodiscard]] std::string to_string() const override {
        std::vector<std::string_view> keys;
        keys.reserve(this->value.size());
        for (const auto& k : this->value | std::views::keys) {
            keys.emplace_back(k);
        }
        std::ranges::sort(keys);
        std::string out = "{";
        bool first = true;
        for (auto k : keys) {
            if (!first) {
                out += ", ";
            }
            out += k;
            out += ": ";
            const auto& v = this->value.at(std::string(k));
            out += v ? v->to_string() : "<null>";
            first = false;
        }
        out += "}";
        return out;
    }

    [[nodiscard]] json_t to_json() const override {
        std::vector<std::string_view> keys;
        keys.reserve(this->value.size());
        for (const auto& k : this->value | std::views::keys) {
            keys.emplace_back(k);
        }
        std::ranges::sort(keys);
        json_t obj = json_t::object();
        for (auto k : keys) {
            const auto& v = this->value.at(std::string(k));
            obj[std::string(k)] = v ? v->to_json() : json_t();
        }
        json_t j{
            {ICell::KEY_KIND, kind_id},
            {ICell::KEY_VALUE, std::move(obj)},
        };
        this->inject_meta(j);
        return j;
    }

    [[nodiscard]] cell_t clone() const override {
        std::unordered_map<std::string, cell_t> copied;
        for (auto const& [k, v] : this->value) {
            copied.emplace(k, v ? v->clone() : nullptr);
        }
        auto out = std::make_shared<HashMapCell>(std::move(copied));
        out->meta_ = this->meta_;
        return out;
    }

    [[nodiscard]] std::size_t hash_value() const noexcept override {
        std::size_t h = std::hash<std::string_view>{}(kind());
        // Sort by key first so the hash is stable across insertion orders.
        std::vector<std::string_view> keys;
        keys.reserve(this->value.size());
        for (const auto& k : this->value | std::views::keys) {
            keys.emplace_back(k);
        }
        std::ranges::sort(keys);
        for (const auto k : keys) {
            const auto& el = this->value.at(std::string(k));
            const std::size_t hk = std::hash<std::string_view>{}(k);
            const std::size_t hv = el ? el->hash_value() : std::size_t{0};
            h ^= hk + 0x9e3779b97f4a7c15ULL + (h << 6) + (h >> 2);
            h ^= hv + 0x9e3779b97f4a7c15ULL + (h << 6) + (h >> 2);
        }
        return h;
    }

    [[nodiscard]] std::partial_ordering compare(ICell const& other) const override {
        if (const auto k_cmp = this->kind() <=> other.kind(); k_cmp != 0) {
            return k_cmp;
        }
        const auto* o = dynamic_cast<HashMapCell const*>(&other);
        if (o == nullptr) {
            return std::partial_ordering::unordered;
        }
        // Use a sorted projection so unordered storage compares deterministically.
        std::map<std::string, cell_t> a(this->value.begin(), this->value.end());
        std::map<std::string, cell_t> b(o->value.begin(), o->value.end());
        auto it_a = a.begin();
        auto it_b = b.begin();
        while (it_a != a.end() && it_b != b.end()) {
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
        return a.size() <=> b.size();
    }

    static cell_t from_json(json_t const& j, ParcelRegistry const& reg) {
        if (!j.is_object()) {
            throw InvalidJsonException("Expected JSON object for HashMapCell",
                                       std::string(kind_id));
        }
        const auto it_k = j.find(ICell::KEY_KIND);
        if (it_k == j.end() || !it_k->is_string()) {
            throw InvalidJsonException("HashMapCell: missing/invalid 'k' (expected 'hm')",
                                       std::string(kind_id));
        }
        if (it_k->get<std::string_view>() != kind_id) {
            throw KindMismatchException("HashMapCell: missing/invalid 'k' (expected 'hm')",
                                        std::string(kind_id));
        }
        const auto it_v = j.find(ICell::KEY_VALUE);
        if (it_v == j.end() || !it_v->is_object()) {
            throw InvalidJsonException("HashMapCell: missing/invalid 'v' (expected object)",
                                       std::string(kind_id));
        }
        std::unordered_map<std::string, cell_t> elems;
        for (auto const& [k, raw] : it_v->items()) {
            elems.emplace(k, raw.is_null() ? cell_t{} : reg.cell_from_json(raw));
        }
        auto cell = std::make_shared<HashMapCell>(std::move(elems));
        base_t::absorb_meta(j, cell);
        return cell;
    }

    static cell_type_descriptor_t descriptor();
};

/**
 * @brief Descriptor for the heterogeneous `HashMapCell`.
 *
 * Reports `CellCategory::Map` (the wire shape is the same as `MapCell`,
 * just hash-backed in memory).
 */
class HashMapCellTypeDescriptor final : public BaseCellTypeDescriptor<HashMapCell> {
public:
    using cell_type = HashMapCell;
    using base_t = BaseCellTypeDescriptor<HashMapCell>;

    explicit HashMapCellTypeDescriptor(DisplayInfo meta) : base_t(std::move(meta)) {}

    [[nodiscard]] descriptor::CellCategory category() const override {
        return descriptor::CellCategory::Map;
    }

    [[nodiscard]] cell_t cell_from_json(const json_t& j, const ParcelRegistry& reg) const override {
        return HashMapCell::from_json(j, reg);
    }
};

inline cell_type_descriptor_t HashMapCell::descriptor() {
    static const auto d = std::make_shared<HashMapCellTypeDescriptor>(
        DisplayInfo{.name = "HashMap", .description = "Hash-backed heterogeneous map of cells"});
    return d;
}

}  // namespace parcel
