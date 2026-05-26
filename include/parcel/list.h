#pragma once

/**
 * @file list.h
 * @brief `TypedListCell<T>` and heterogeneous `ListCell` with their descriptors.
 *
 * Two list flavors share this header. `TypedListCell<T>` carries a
 * compile-time element kind, encoded in its kind id (e.g. `"l:i32"`) and
 * stored as `std::vector<typename T::storage_t>` so element values are raw
 * — no per-cell wrapping. `ListCell` holds `std::vector<cell_t>` for
 * heterogeneous values and dispatches each element through the registry.
 * See the README "Lists and maps" section.
 */

#include <parcel/cell.h>
#include <parcel/descriptor.h>
#include <parcel/registry.h>

#include <initializer_list>
#include <memory>
#include <ranges>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace parcel {

// std::vector<T>::operator== eagerly instantiates T::operator==; suppress
// BaseCell's default compare for vector storage so cells whose element
// payloads lack `==` still compile (TypedListCell overrides compare()).
template <typename T, typename A>
struct detail::disables_basecell_compare<std::vector<T, A>> : std::true_type {
};  // namespace detail

class ParcelRegistry;

template <CellLike T>
class TypedListCell;

/**
 * @brief Descriptor for `TypedListCell<T>`.
 *
 * Adds an `"element_kind"` field to the JSON descriptor and exposes the
 * element kind through `ISubTypes` so the registry's schema walker reaches
 * the inner type.
 *
 * @tparam T Element cell type.
 */
template <CellLike T>
class TypedListCellTypeDescriptor final : public BaseCellTypeDescriptor<TypedListCell<T>>,
                                          public ISubTypes {
public:
    using cell_type = TypedListCell<T>;
    using base_t = BaseCellTypeDescriptor<TypedListCell<T>>;

    /**
     * @brief Construct with the given display info.
     * @param info Display info.
     */
    explicit TypedListCellTypeDescriptor(DisplayInfo info) : base_t(std::move(info)) {}

    /** @brief Wire kind id of the list's element type. */
    [[nodiscard]] std::string_view element_kind() const {
        return T::kind_id;
    }

    [[nodiscard]] descriptor::CellCategory category() const override {
        return descriptor::CellCategory::TypedList;
    }

    [[nodiscard]] std::vector<std::string_view> sub_kinds() const override {
        return {T::kind_id};
    }

    [[nodiscard]] cell_t cell_from_json(json_t const& j, ParcelRegistry const& reg) const override {
        return TypedListCell<T>::from_json(j, reg);
    }

    [[nodiscard]] json_t to_json() const override {
        auto j = base_t::base_to_json();
        j["element_kind"] = T::kind_id;
        return j;
    }
};

/**
 * @brief Homogeneous list of element cells of type @p T.
 *
 * Wire shape (example for `TypedListCell<I32Cell>`):
 * @code{.json}
 * {"k": "l:i32", "v": [1, 2, 3]}
 * @endcode
 * The element kind is part of `"k"` so the registry can dispatch each
 * typed list distinctly. Storage is `std::vector<typename T::storage_t>`,
 * which means raw element values (not wrapper cells) are held — the kind
 * discipline lives on the type tag, and a curated subset of
 * `std::vector`'s API is forwarded for ergonomic use.
 *
 * @tparam T Element cell type (must satisfy `CellLike`).
 * @see TypedListCellTypeDescriptor
 */
template <CellLike T>
class TypedListCell : public BaseCell<TypedListCell<T>, std::vector<typename T::storage_t>> {
    using base_t = BaseCell<TypedListCell, std::vector<typename T::storage_t>>;

public:
    using element_type = T;
    using element_storage_t = typename T::storage_t;

    using base_t::base_t;
    using base_t::operator=;

    /**
     * @brief Construct from a brace-enclosed list of element values.
     * @param elems Initializer list of raw element values.
     */
    TypedListCell(std::initializer_list<element_storage_t> elems)
        : base_t(std::vector<element_storage_t>(elems)) {}

    /**
     * @brief Construct from any input range whose elements convert to `element_storage_t`.
     *
     * Disambiguated by `std::from_range_t` so it does not collide with the
     * inherited `BaseCell(U&&)`.
     */
    template <std::ranges::input_range R>
        requires std::convertible_to<std::ranges::range_value_t<R>, element_storage_t>
    TypedListCell(std::from_range_t /*tag*/, R&& r)
        : base_t(std::vector<element_storage_t>(std::ranges::begin(r), std::ranges::end(r))) {}

    /**
     * @brief Construct by copying the contents of @p sp into the list.
     */
    explicit TypedListCell(std::span<const element_storage_t> sp)
        : base_t(std::vector<element_storage_t>(sp.begin(), sp.end())) {}

    // `as_span` is disabled for `bool` because `std::vector<bool>` is the
    // packed bitset specialization — it has no `data()` and cannot be viewed
    // as a contiguous range of `bool`. Reach for `value` (the vector itself)
    // and iterate, or pick a different element cell for span-based access.

    /** @brief Read-only span view over the list's elements. */
    [[nodiscard]] std::span<const element_storage_t> as_span() const noexcept
        requires(!std::same_as<element_storage_t, bool>)
    {
        return std::span<const element_storage_t>(this->value.data(), this->value.size());
    }

    /** @brief Mutable span view over the list's elements. */
    [[nodiscard]] std::span<element_storage_t> as_span() noexcept
        requires(!std::same_as<element_storage_t, bool>)
    {
        return std::span<element_storage_t>(this->value.data(), this->value.size());
    }

    // ---- std::vector pass-through API ------------------------------------
    // Forwards a curated subset of std::vector<element_storage_t>'s API so
    // TypedListCell can be used directly without going through .value /
    // .get(). The list is transparent: elements are raw element_storage_t
    // values, not ICell wrappers. The kind discipline lives on the type tag.

    using vec_t = std::vector<element_storage_t>;
    using value_type = vec_t::value_type;
    using size_type = vec_t::size_type;
    using iterator = vec_t::iterator;
    using const_iterator = vec_t::const_iterator;
    using reference = vec_t::reference;
    using const_reference = vec_t::const_reference;

    [[nodiscard]] size_type size() const noexcept {
        return this->value.size();
    }
    [[nodiscard]] bool empty() const noexcept {
        return this->value.empty();
    }
    void reserve(size_type n) {
        this->value.reserve(n);
    }

    reference operator[](size_type i) {
        return this->value[i];
    }
    const_reference operator[](size_type i) const {
        return this->value[i];
    }
    reference at(size_type i) {
        return this->value.at(i);
    }
    const_reference at(size_type i) const {
        return this->value.at(i);
    }
    reference front() {
        return this->value.front();
    }
    [[nodiscard]] const_reference front() const {
        return this->value.front();
    }
    reference back() {
        return this->value.back();
    }
    [[nodiscard]] const_reference back() const {
        return this->value.back();
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

    void push_back(element_storage_t const& v) {
        this->value.push_back(v);
    }
    void push_back(element_storage_t&& v) {
        this->value.push_back(std::move(v));
    }
    template <typename... Args>
    reference emplace_back(Args&&... args) {
        return this->value.emplace_back(std::forward<Args>(args)...);
    }
    void pop_back() {
        this->value.pop_back();
    }
    void clear() noexcept {
        this->value.clear();
    }
    iterator erase(const_iterator pos) {
        return this->value.erase(pos);
    }
    iterator erase(const_iterator first, const_iterator last) {
        return this->value.erase(first, last);
    }
    iterator insert(const_iterator pos, element_storage_t const& v) {
        return this->value.insert(pos, v);
    }
    iterator insert(const_iterator pos, element_storage_t&& v) {
        return this->value.insert(pos, std::move(v));
    }
    // ----------------------------------------------------------------------

    /**
     * @brief Wire kind id of this typed list (`"l:" + T::kind_id`).
     *
     * Encoded in the type so each instantiation registers under a distinct
     * kind:
     * @code{.cpp}
     * TypedListCell<I32Cell>::kind_id    == "l:i32";
     * TypedListCell<StringCell>::kind_id == "l:string";
     * @endcode
     */
    static constexpr std::string_view kind_id = prefixed_id_v<"l:", T::kind_id>;

    /**
     * @brief Render the list as `[a, b, c]` using each element's `to_string`.
     * @return Bracketed comma-separated form.
     */
    [[nodiscard]] std::string to_string() const override {
        std::string out = "[";
        bool first = true;
        for (auto const& el : this->value) {
            if (!first) {
                out += ", ";
            }
            T wrapper(el);
            out += wrapper.to_string();
            first = false;
        }
        out += "]";
        return out;
    }

    [[nodiscard]] json_t to_json() const override {
        json_t arr = json_t::array();
        for (auto const& el : this->value) {
            T wrapper(el);
            arr.push_back(wrapper.to_json().at(ICell::KEY_VALUE));
        }
        json_t j{
            {ICell::KEY_KIND, kind_id},
            {ICell::KEY_VALUE, std::move(arr)},
        };
        this->inject_display_info(j);
        return j;
    }

    /**
     * @brief Element-wise three-way comparison; ignores display info.
     *
     * Each element pair is wrapped in `T` and routed through `T::compare`
     * so element types whose raw storage lacks `==` (e.g. struct payloads)
     * still compare correctly via the cell wrapper.
     */
    [[nodiscard]] std::partial_ordering compare(ICell const& other) const override {
        if (const auto k_cmp = this->kind() <=> other.kind(); k_cmp != 0) {
            return k_cmp;
        }
        const auto* o = dynamic_cast<TypedListCell const*>(&other);
        if (o == nullptr) {
            return std::partial_ordering::unordered;
        }
        const auto& a = this->value;
        const auto& b = o->value;
        const std::size_t n = std::min(a.size(), b.size());
        for (std::size_t i = 0; i < n; ++i) {
            T wa(a[i]);
            T wb(b[i]);
            if (const auto c = wa.compare(wb); c != 0) {
                return c;
            }
        }
        return a.size() <=> b.size();
    }

    /**
     * @brief Deserialize a `TypedListCell<T>` from JSON.
     * @param j   Input JSON object — must match `{"k": kind_id, "v": [...] }`.
     * @param reg Registry (passed through to per-element `from_json`).
     * @return Newly built cell handle.
     * @throws std::runtime_error on shape or kind mismatch, or if any
     *         element fails to deserialize as @p T.
     */
    static cell_t from_json(json_t const& j, ParcelRegistry const& reg) {
        if (!j.is_object()) {
            throw InvalidJsonException("Expected JSON object for TypedListCell",
                                       std::string(kind_id));
        }

        const auto it_k = j.find(ICell::KEY_KIND);
        if (it_k == j.end() || !it_k->is_string()) {
            throw InvalidJsonException("TypedListCell: missing/invalid 'k' (expected '" +
                                           std::string(kind_id) + "')",
                                       std::string(kind_id));
        }
        if (it_k->get<std::string_view>() != kind_id) {
            throw KindMismatchException("TypedListCell: missing/invalid 'k' (expected '" +
                                            std::string(kind_id) + "')",
                                        std::string(kind_id));
        }

        const auto it_v = j.find(ICell::KEY_VALUE);
        if (it_v == j.end() || !it_v->is_array()) {
            throw InvalidJsonException("TypedListCell: missing/invalid 'v' (expected array)",
                                       std::string(kind_id));
        }

        std::vector<element_storage_t> elems;
        elems.reserve(it_v->size());
        for (auto const& raw : *it_v) {
            json_t wrapped{{ICell::KEY_KIND, T::kind_id}, {ICell::KEY_VALUE, raw}};
            cell_t v = T::from_json(wrapped, reg);
            auto* typed = dynamic_cast<T*>(v.get());
            if (typed == nullptr) {
                throw TypeException(
                    std::string("TypedListCell: element from_json did not return ") +
                        std::string(T::kind_id),
                    std::string(kind_id));
            }
            elems.push_back(std::move(typed->value));
        }
        auto cell = std::make_shared<TypedListCell>(std::move(elems));
        base_t::absorb_display_info(j, cell);
        return cell;
    }

    /**
     * @brief Cached descriptor for this typed list.
     * @return Shared `TypedListCellTypeDescriptor<T>` instance.
     */
    static cell_type_descriptor_t descriptor() {
        static const auto d = std::make_shared<TypedListCellTypeDescriptor<T>>(DisplayInfo{
            .name = "List",
            .description = std::string("Homogeneous list of ") + std::string(T::kind_id),
        });
        return d;
    }
};

/**
 * @brief Heterogeneous list of `cell_t` — wire kind `"l"`.
 *
 * Wire shape:
 * @code{.json}
 * {"k": "l", "v": [{"k":"i32","v":1}, {"k":"string","v":"hi"}]}
 * @endcode
 * Elements are full nested cells whose kind is recovered from each
 * element's own `"k"`. Use `TypedListCell<T>` instead when every element
 * has the same wire kind — the typed variant skips the per-element kind
 * tag and is more compact on the wire.
 *
 * @see TypedListCell — homogeneous variant with element-tagged kind id.
 */
class ListCell : public BaseCell<ListCell, std::vector<cell_t>> {
    using base_t = BaseCell;

public:
    using base_t::base_t;
    using base_t::operator=;

    /**
     * @brief Construct from a brace-enclosed list of cells.
     * @param elems Initializer list of cell handles.
     */
    ListCell(const std::initializer_list<cell_t> elems) : base_t(std::vector(elems)) {}

    // ---- std::vector pass-through API ------------------------------------

    using vec_t = std::vector<cell_t>;
    using value_type = vec_t::value_type;
    using size_type = vec_t::size_type;
    using iterator = vec_t::iterator;
    using const_iterator = vec_t::const_iterator;
    using reference = vec_t::reference;
    using const_reference = vec_t::const_reference;

    [[nodiscard]] size_type size() const noexcept {
        return this->value.size();
    }
    [[nodiscard]] bool empty() const noexcept {
        return this->value.empty();
    }
    void reserve(const size_type n) {
        this->value.reserve(n);
    }

    reference operator[](const size_type i) {
        return this->value[i];
    }
    const_reference operator[](const size_type i) const {
        return this->value[i];
    }
    reference at(const size_type i) {
        return this->value.at(i);
    }
    [[nodiscard]] const_reference at(const size_type i) const {
        return this->value.at(i);
    }
    reference front() {
        return this->value.front();
    }
    [[nodiscard]] const_reference front() const {
        return this->value.front();
    }
    reference back() {
        return this->value.back();
    }
    [[nodiscard]] const_reference back() const {
        return this->value.back();
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

    void push_back(cell_t const& v) {
        this->value.push_back(v);
    }
    void push_back(cell_t&& v) {
        this->value.push_back(std::move(v));
    }
    template <typename... Args>
    reference emplace_back(Args&&... args) {
        return this->value.emplace_back(std::forward<Args>(args)...);
    }
    void pop_back() {
        this->value.pop_back();
    }
    void clear() noexcept {
        this->value.clear();
    }
    iterator erase(const const_iterator pos) {
        return this->value.erase(pos);
    }
    iterator erase(const const_iterator first, const const_iterator last) {
        return this->value.erase(first, last);
    }
    iterator insert(const const_iterator pos, cell_t const& v) {
        return this->value.insert(pos, v);
    }
    iterator insert(const const_iterator pos, cell_t&& v) {
        return this->value.insert(pos, std::move(v));
    }
    // ----------------------------------------------------------------------

    /** @brief Wire kind id (`"l"`). */
    static constexpr std::string_view kind_id = "l";

    /**
     * @brief Render the list as `[a, b, c]`; null elements appear as `<null>`.
     * @return Bracketed comma-separated form.
     */
    [[nodiscard]] std::string to_string() const override {
        std::string out = "[";
        bool first = true;
        for (auto const& el : this->value) {
            if (!first) {
                out += ", ";
            }
            out += el ? el->to_string() : "<null>";
            first = false;
        }
        out += "]";
        return out;
    }

    [[nodiscard]] json_t to_json() const override {
        json_t arr = json_t::array();
        for (auto const& el : this->value) {
            arr.push_back(el ? el->to_json() : json_t());
        }
        json_t j{
            {KEY_KIND, kind_id},
            {KEY_VALUE, std::move(arr)},
        };
        this->inject_display_info(j);
        return j;
    }

    [[nodiscard]] cell_t clone() const override {
        std::vector<cell_t> copied;
        copied.reserve(this->value.size());
        for (auto const& el : this->value) {
            copied.push_back(el ? el->clone() : nullptr);
        }
        auto out = std::make_shared<ListCell>(std::move(copied));
        out->display_info_ = this->display_info_;
        return out;
    }

    [[nodiscard]] std::size_t hash_value() const noexcept override {
        std::size_t h = std::hash<std::string_view>{}(kind());
        for (auto const& el : this->value) {
            const std::size_t hv = el ? el->hash_value() : std::size_t{0};
            h ^= hv + 0x9e3779b97f4a7c15ULL + (h << 6) + (h >> 2);
        }
        return h;
    }

    /**
     * @brief Lexicographic three-way comparison over elements; ignores display info.
     *
     * Null elements compare less than non-null. Different kinds short-circuit
     * via `kind() <=> kind()`.
     */
    [[nodiscard]] std::partial_ordering compare(ICell const& other) const override {
        if (const auto k_cmp = this->kind() <=> other.kind(); k_cmp != 0) {
            return k_cmp;
        }
        const auto* o = dynamic_cast<ListCell const*>(&other);
        if (o == nullptr) {
            return std::partial_ordering::unordered;
        }
        const auto& a = this->value;
        const auto& b = o->value;
        const std::size_t n = std::min(a.size(), b.size());
        for (std::size_t i = 0; i < n; ++i) {
            if (!a[i] && !b[i]) {
                continue;
            }
            if (!a[i]) {
                return std::partial_ordering::less;
            }
            if (!b[i]) {
                return std::partial_ordering::greater;
            }
            if (const auto c = a[i]->compare(*b[i]); c != 0) {
                return c;
            }
        }
        return a.size() <=> b.size();
    }

    /**
     * @brief Deserialize a `ListCell` from JSON.
     * @param j   Input JSON object — must match `{"k": "l", "v": [...] }`.
     * @param reg Registry used to dispatch each element by its own kind.
     * @return Newly built cell handle.
     * @throws std::runtime_error on shape mismatch or unknown element kind.
     */
    static cell_t from_json(json_t const& j, ParcelRegistry const& reg) {
        if (!j.is_object()) {
            throw InvalidJsonException("Expected JSON object for ListCell", std::string(kind_id));
        }

        const auto it_k = j.find(KEY_KIND);
        if (it_k == j.end() || !it_k->is_string()) {
            throw InvalidJsonException("ListCell: missing/invalid 'k' (expected 'l')",
                                       std::string(kind_id));
        }
        if (it_k->get<std::string_view>() != kind_id) {
            throw KindMismatchException("ListCell: missing/invalid 'k' (expected 'l')",
                                        std::string(kind_id));
        }

        const auto it_v = j.find(KEY_VALUE);
        if (it_v == j.end() || !it_v->is_array()) {
            throw InvalidJsonException("ListCell: missing/invalid 'v' (expected array)",
                                       std::string(kind_id));
        }

        std::vector<cell_t> elems;
        elems.reserve(it_v->size());
        for (auto const& raw : *it_v) {
            // Symmetric with to_json: a null element on the wire round-trips
            // to a null cell_t handle, not a deserialization error.
            elems.push_back(raw.is_null() ? cell_t{} : reg.cell_from_json(raw));
        }
        auto cell = std::make_shared<ListCell>(std::move(elems));
        absorb_display_info(j, cell);
        return cell;
    }

    /**
     * @brief Cached descriptor for `ListCell`.
     * @return Shared descriptor instance.
     */
    static cell_type_descriptor_t descriptor();
};

/**
 * @brief Descriptor for the heterogeneous `ListCell`.
 *
 * Reports `CellCategory::List` so registry consumers can distinguish the
 * heterogeneous list from typed lists and primitives.
 */
class ListCellTypeDescriptor final : public BaseCellTypeDescriptor<ListCell> {
public:
    using cell_type = ListCell;
    using base_t = BaseCellTypeDescriptor;

    explicit ListCellTypeDescriptor(DisplayInfo info) : base_t(std::move(info)) {}

    [[nodiscard]] descriptor::CellCategory category() const override {
        return descriptor::CellCategory::List;
    }

    [[nodiscard]] cell_t cell_from_json(const json_t& j, const ParcelRegistry& reg) const override {
        return ListCell::from_json(j, reg);
    }
};

inline cell_type_descriptor_t ListCell::descriptor() {
    static const auto d = std::make_shared<ListCellTypeDescriptor>(
        DisplayInfo{.name = "List", .description = "Heterogeneous list of cells"});
    return d;
}

}  // namespace parcel

// `make_list` lives in <parcel/defaults.h> because it needs `parcel::cell()`
// which is declared there. The function is ADL-discoverable because it lives
// in `parcel::`.
