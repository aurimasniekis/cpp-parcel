#pragma once

/**
 * @file struct.h
 * @brief `StructCell` CRTP base, `FieldsBuilder`, and the per-field descriptors.
 *
 * `StructCell<Derived, Payload>` makes a plain user-defined `Payload`
 * struct addressable through the cell interface: each field is a runtime
 * `IPayloadFieldDescriptor` keyed by JSON name and pointing back at the
 * payload via a member pointer. `FieldsBuilder<Payload>` is the fluent
 * builder used inside `Derived::field_descriptors()` to declare each
 * field. See the README "Structs" section.
 */

#include <parcel/cell.h>
#include <parcel/defaults.h>
#include <parcel/descriptor.h>
#include <parcel/list.h>
#include <parcel/map.h>
#include <parcel/primitive.h>
#include <parcel/registry.h>

#include <algorithm>
#include <map>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

namespace parcel {
class ParcelRegistry;

namespace detail {

template <typename T>
struct member_pointer_traits;

template <typename C, typename F>
struct member_pointer_traits<F C::*> {
    using class_type = C;
    using field_type = F;
};

template <typename T>
struct is_optional : std::false_type {};
template <typename U>
struct is_optional<std::optional<U>> : std::true_type {};

template <typename T>
inline constexpr bool is_optional_v = is_optional<T>::value;

}  // namespace detail

/**
 * @brief Field-descriptor extension that knows how to read/write a specific @p Payload.
 *
 * Refines `IFieldDescriptor` with hooks the `StructCell` machinery uses to
 * project payload members in and out of JSON.
 *
 * @tparam Payload The struct type whose fields are described.
 */
template <typename Payload>
struct IPayloadFieldDescriptor : virtual IFieldDescriptor {
    /**
     * @brief Serialize this field from @p p into the value object @p v_obj.
     * @param v_obj Target JSON object (the `"v"` block of the cell).
     * @param p     Payload instance to read from.
     */
    virtual void to_json_into(json_t& v_obj, Payload const& p) const = 0;

    /**
     * @brief Deserialize this field from @p field_json into @p p.
     * @param field_json JSON value for this field (already a wrapped cell).
     * @param p          Payload instance to write into.
     * @param reg        Registry forwarded to nested cell deserializers.
     */
    virtual void
    from_json_into(json_t const& field_json, Payload& p, ParcelRegistry const& reg) const = 0;

    /**
     * @brief Append `key: value` for this field to the rendered string.
     * @param out String buffer to append to.
     * @param p   Payload instance to read from.
     */
    virtual void to_string_into(std::string& out, Payload const& p) const = 0;

    /**
     * @brief Three-way compare this field across two payload instances.
     *
     * `StructCell::compare` walks fields in declaration order and short-
     * circuits on the first non-equal result; ignores meta. Optional fields
     * follow `std::optional`'s `<=>`: absent < present, both absent is equal.
     */
    [[nodiscard]] virtual std::partial_ordering compare(Payload const& a,
                                                        Payload const& b) const = 0;

    /** @brief Mutable access to this field's meta — used by `FieldsBuilder`. */
    virtual DisplayInfo& mutable_meta() = 0;

    /** @brief Override the field's required flag. */
    virtual void set_required(bool r) = 0;
};

/**
 * @brief Concrete payload field descriptor backed by a member pointer.
 *
 * The `required` flag defaults to `false` for `std::optional<T>` fields
 * and `true` otherwise — matching the natural reading of "optional means
 * may be absent".
 *
 * The member pointer is held at runtime (not as a non-type template
 * parameter) so it can be a `FieldT Base::*` implicitly converted to
 * `FieldT Payload::*`. This keeps `FieldsBuilder<Derived>::field<&Base::x>`
 * usable for redeclaring/overriding inherited fields after `extend<>`.
 *
 * @tparam Payload The struct type being described.
 * @tparam FieldT  The type of the member pointed at by `member_ptr`.
 * @tparam CellT   The cell type used to wrap @p FieldT on the wire.
 */
template <typename Payload, typename FieldT, CellLike CellT>
class MemberFieldDescriptor final : public IPayloadFieldDescriptor<Payload> {
    // clang-format-22 minor versions disagree on Payload::* spacing.
    // clang-format off
    FieldT Payload::*member_ptr_;
    // clang-format on
    std::string key_;
    DisplayInfo meta_;
    bool required_ = !detail::is_optional_v<FieldT>;

public:
    /**
     * @brief Construct with the member pointer, JSON key, and (initial) metadata.
     * @param member_ptr Pointer-to-member into @p Payload.
     * @param key        JSON key under which this field appears.
     * @param meta       Initial descriptive metadata.
     */
    // clang-format off
    MemberFieldDescriptor(FieldT Payload::*member_ptr, std::string key, DisplayInfo meta)
        // clang-format on
        : member_ptr_(member_ptr), key_(std::move(key)), meta_(std::move(meta)) {}

    [[nodiscard]] std::string_view key() const override {
        return key_;
    }
    [[nodiscard]] std::string_view kind() const override {
        return CellT::kind_id;
    }
    [[nodiscard]] DisplayInfo meta() const override {
        return meta_;
    }
    [[nodiscard]] bool is_required() const override {
        return required_;
    }

    [[nodiscard]] json_t to_json() const override {
        return json_t{
            {"key", key_},
            {"kind", CellT::kind_id},
            {"meta", json_t(meta_)},
            {"required", required_},
        };
    }

    DisplayInfo& mutable_meta() override {
        return meta_;
    }
    void set_required(const bool r) override {
        required_ = r;
    }

    void to_json_into(json_t& v_obj, Payload const& p) const override {
        if constexpr (detail::is_optional_v<FieldT>) {
            if (!(p.*member_ptr_).has_value()) {
                return;
            }
            CellT wrapper(*(p.*member_ptr_));
            v_obj[key_] = wrapper.to_json();
        } else {
            CellT wrapper(p.*member_ptr_);
            v_obj[key_] = wrapper.to_json();
        }
    }

    void
    from_json_into(json_t const& field_json, Payload& p, ParcelRegistry const& reg) const override {
        const cell_t v = CellT::from_json(field_json, reg);
        auto* typed = dynamic_cast<CellT*>(v.get());
        if (typed == nullptr) {
            throw TypeException(
                "StructCell: field '" + key_ + "' from_json did not return CellT", {}, key_);
        }
        p.*member_ptr_ = std::move(typed->value);
    }

    // Overrides IPayloadFieldDescriptor::compare(a, b); signature is fixed.
    // NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
    [[nodiscard]] std::partial_ordering compare(Payload const& a, Payload const& b) const override {
        if constexpr (detail::is_optional_v<FieldT>) {
            const auto& oa = a.*member_ptr_;
            const auto& ob = b.*member_ptr_;
            if (!oa.has_value() && !ob.has_value()) {
                return std::partial_ordering::equivalent;
            }
            if (!oa.has_value()) {
                return std::partial_ordering::less;
            }
            if (!ob.has_value()) {
                return std::partial_ordering::greater;
            }
            CellT wa(*oa);
            CellT wb(*ob);
            return wa.compare(wb);
        } else {
            CellT wa(a.*member_ptr_);
            CellT wb(b.*member_ptr_);
            return wa.compare(wb);
        }
    }

    void to_string_into(std::string& out, Payload const& p) const override {
        out += key_;
        out += ": ";
        if constexpr (detail::is_optional_v<FieldT>) {
            if (!(p.*member_ptr_).has_value()) {
                out += "<nullopt>";
                return;
            }
            CellT wrapper(*(p.*member_ptr_));
            out += wrapper.to_string();
        } else {
            CellT wrapper(p.*member_ptr_);
            out += wrapper.to_string();
        }
    }
};

/**
 * @brief Adapter that lets a parent struct's payload field descriptor be reused by a derived
 * struct.
 *
 * Holds a `shared_ptr<IPayloadFieldDescriptor<ParentPayload>>` and forwards
 * every call after `static_cast`-ing the derived payload reference to the
 * parent payload reference. The cast is sound because `extend<>` constrains
 * @p DerivedPayload to derive from @p ParentPayload.
 *
 * @tparam DerivedPayload The struct type using the inherited field.
 * @tparam ParentPayload  The struct type that originally declared the field.
 */
template <typename DerivedPayload, typename ParentPayload>
class InheritedFieldDescriptor final : public IPayloadFieldDescriptor<DerivedPayload> {
    static_assert(std::is_base_of_v<ParentPayload, DerivedPayload>,
                  "InheritedFieldDescriptor: DerivedPayload must derive from ParentPayload");

    std::shared_ptr<IPayloadFieldDescriptor<ParentPayload>> inner_;

public:
    /**
     * @brief Wrap a parent payload field descriptor.
     * @param inner Parent descriptor to forward to.
     */
    explicit InheritedFieldDescriptor(std::shared_ptr<IPayloadFieldDescriptor<ParentPayload>> inner)
        : inner_(std::move(inner)) {}

    [[nodiscard]] std::string_view key() const override {
        return inner_->key();
    }
    [[nodiscard]] std::string_view kind() const override {
        return inner_->kind();
    }
    [[nodiscard]] DisplayInfo meta() const override {
        return inner_->meta();
    }
    [[nodiscard]] bool is_required() const override {
        return inner_->is_required();
    }
    [[nodiscard]] json_t to_json() const override {
        return inner_->to_json();
    }

    DisplayInfo& mutable_meta() override {
        return inner_->mutable_meta();
    }
    void set_required(const bool r) override {
        inner_->set_required(r);
    }

    void to_json_into(json_t& v_obj, DerivedPayload const& p) const override {
        inner_->to_json_into(v_obj, static_cast<ParentPayload const&>(p));
    }
    void from_json_into(json_t const& field_json,
                        DerivedPayload& p,
                        ParcelRegistry const& reg) const override {
        inner_->from_json_into(field_json, static_cast<ParentPayload&>(p), reg);
    }
    void to_string_into(std::string& out, DerivedPayload const& p) const override {
        inner_->to_string_into(out, static_cast<ParentPayload const&>(p));
    }
    [[nodiscard]] std::partial_ordering compare(DerivedPayload const& a,
                                                DerivedPayload const& b) const override {
        return inner_->compare(static_cast<ParentPayload const&>(a),
                               static_cast<ParentPayload const&>(b));
    }
};

/**
 * @brief Fluent builder used inside `Derived::field_descriptors()` to declare struct fields.
 *
 * Typical use:
 * @code{.cpp}
 * return FieldsBuilder<Person>{}
 *     .field<&Person::id, I32Cell>("id").description("Primary key")
 *     .field<&Person::name>("name")           // type inferred via default_cell_for
 *     .field<&Person::email>("email").optional()
 *     .build();
 * @endcode
 *
 * Field-list reuse via `extend<ParentCell>()`:
 * @code{.cpp}
 * struct Address     { std::string street; std::string city; };
 * struct HomeAddress : Address { std::string label; };
 *
 * // HomeAddressCell::field_descriptors():
 * return FieldsBuilder<HomeAddress>{}
 *     .extend<AddressCell>()                  // splice street + city
 *     .field<&HomeAddress::label>("label")    // add new field
 *     .build();
 * @endcode
 *
 * Duplicate keys are last-wins: a `field<>()` call with the same key as an
 * already-declared field replaces it in place. `remove_field(key)` drops a
 * previously declared (or inherited) field.
 *
 * @tparam Payload The struct type whose fields are being declared.
 * @see default_cell_for — drives the inference overload.
 * @see InheritedFieldDescriptor — backs `extend<>`.
 */
template <typename Payload>
class FieldsBuilder {
public:
    using payload_field_t = std::shared_ptr<IPayloadFieldDescriptor<Payload>>;
    using payload_field_descriptors_t = std::vector<payload_field_t>;

    /**
     * @brief Declare a field with an explicit cell wrapper @p CellT.
     * @tparam MemberPtr Pointer-to-member of @p Payload.
     * @tparam CellT     Cell type used to wrap the field on the wire.
     * @param  key       JSON key for this field.
     * @return `*this` for chaining.
     */
    template <auto MemberPtr, CellLike CellT>
    FieldsBuilder& field(std::string key) {
        using mp_t = decltype(MemberPtr);
        using traits = detail::member_pointer_traits<mp_t>;
        static_assert(std::is_base_of_v<typename traits::class_type, Payload>,
                      "FieldsBuilder: member pointer's class must be Payload "
                      "or a base class of Payload");

        using FieldT = typename traits::field_type;
        // Implicit base-to-derived member-pointer conversion happens here at
        // runtime — the conversion isn't constexpr, so the pointer is held
        // by the descriptor instead of a non-type template parameter.
        // clang-format off
        FieldT Payload::*mp = MemberPtr;
        // clang-format on
        auto fd = std::make_shared<MemberFieldDescriptor<Payload, FieldT, CellT>>(
            mp, std::move(key), DisplayInfo{});
        replace_or_append(std::move(fd));
        return *this;
    }

    /**
     * @brief Declare a field with cell wrapper inferred via `default_cell_for`.
     *
     * Available for any field type that has a `default_cell_for`
     * specialization (every `PrimitiveStorage` type out of the box, plus
     * `std::vector`, `std::map`, `std::variant`, and `std::optional` of
     * those).
     *
     * @tparam MemberPtr Pointer-to-member of @p Payload.
     * @param  key       JSON key for this field.
     * @return `*this` for chaining.
     */
    template <auto MemberPtr>
    FieldsBuilder& field(std::string key)
        requires HasDefaultCellWrapper<
            typename detail::member_pointer_traits<decltype(MemberPtr)>::field_type>
    {
        using FieldT = typename detail::member_pointer_traits<decltype(MemberPtr)>::field_type;
        return field<MemberPtr, default_cell_for_t<FieldT>>(std::move(key));
    }

    /**
     * @brief Set the `name` meta of the most recently declared field.
     * @param v Display name.
     * @return `*this` for chaining.
     * @throws std::runtime_error if called before any `field<>()`.
     */
    FieldsBuilder& name(std::string v) {
        require_last("name")->mutable_meta().name = std::move(v);
        return *this;
    }

    /**
     * @brief Set the `description` meta of the most recently declared field.
     * @param v Description text.
     * @return `*this` for chaining.
     * @throws std::runtime_error if called before any `field<>()`.
     */
    FieldsBuilder& description(std::string v) {
        require_last("description")->mutable_meta().description = std::move(v);
        return *this;
    }

    /**
     * @brief Set the `icon` meta of the most recently declared field to the
     *        typed @p icon.
     * @param icon Icon value.
     * @return `*this` for chaining.
     * @throws std::runtime_error if called before any `field<>()`.
     */
    FieldsBuilder& icon(comms::Icon icon) {
        require_last("icon")->mutable_meta().icon = icon;
        return *this;
    }

    /**
     * @brief Set the `icon` meta of the most recently declared field, parsing
     *        an Iconify `set:name` string (e.g. `"mdi:account"`).
     * @param v Iconify `set:name` identifier.
     * @return `*this` for chaining.
     * @throws std::runtime_error if called before any `field<>()`.
     * @throws std::invalid_argument if @p v is not a valid `set:name` icon.
     */
    FieldsBuilder& icon(std::string const& v) {
        return icon(comms::Icon::from(v));
    }

    /**
     * @brief Set the `color` meta of the most recently declared field to the
     *        typed @p color.
     * @param color Color value.
     * @return `*this` for chaining.
     * @throws std::runtime_error if called before any `field<>()`.
     */
    FieldsBuilder& color(comms::Color color) {
        require_last("color")->mutable_meta().color = color;
        return *this;
    }

    /**
     * @brief Set the `color` meta of the most recently declared field, parsing
     *        a color string (hex, CSS-functional, or a CSS color name).
     * @param v Color string accepted by `comms::Color::parse`.
     * @return `*this` for chaining.
     * @throws std::runtime_error if called before any `field<>()`.
     * @throws InvalidJsonException if @p v does not parse to a color.
     */
    FieldsBuilder& color(std::string const& v) {
        const auto parsed = comms::Color::parse(v);
        if (!parsed) {
            throw InvalidJsonException("FieldsBuilder::color: '" + v + "' is not a valid color");
        }
        return color(*parsed);
    }

    /**
     * @brief Override the required flag of the most recently declared field.
     * @param v Whether the field must be present on deserialization.
     * @return `*this` for chaining.
     * @throws std::runtime_error if called before any `field<>()`.
     */
    FieldsBuilder& is_required(bool v = true) {
        require_last("is_required")->set_required(v);
        return *this;
    }

    /**
     * @brief Shorthand for `is_required(false)`.
     * @return `*this` for chaining.
     * @throws std::runtime_error if called before any `field<>()`.
     */
    FieldsBuilder& optional() {
        return is_required(false);
    }

    /**
     * @brief Splice in every field declared by another struct cell.
     *
     * Each parent field is wrapped in `InheritedFieldDescriptor` and
     * appended in declaration order. A subsequent `field<>(key)` with the
     * same key replaces the inherited entry in place (last-wins). The
     * constraint requires @p ParentCell's payload to be a base of (or the
     * same as) @p Payload — extending from an unrelated struct cell will
     * not compile.
     *
     * After this call, `last_` points at the most recently spliced field,
     * so a chained `.name(...)` / `.description(...)` / `.optional()`
     * mutates that inherited field's per-call descriptor copy without
     * affecting the parent cell.
     *
     * @tparam ParentCell A `CellLike` exposing `payload_type` whose payload
     *                    is a base of (or same as) @p Payload.
     * @return `*this` for chaining.
     */
    template <CellLike ParentCell>
        requires std::is_base_of_v<typename ParentCell::payload_type, Payload>
    FieldsBuilder& extend() {
        using ParentPayload = typename ParentCell::payload_type;
        for (auto parent_fields = ParentCell::field_descriptors();
             auto& parent_field : parent_fields) {
            auto adapter = std::make_shared<InheritedFieldDescriptor<Payload, ParentPayload>>(
                std::move(parent_field));
            replace_or_append(std::move(adapter));
        }
        return *this;
    }

    /**
     * @brief Drop a previously declared (or inherited) field by key.
     *
     * No-op if no field with @p key exists. Resets the "last declared
     * field" pointer so a stray `.name(...)` / `.optional()` after
     * `remove_field` throws instead of silently mutating an unrelated
     * field.
     *
     * @param  key JSON key of the field to remove.
     * @return `*this` for chaining.
     */
    FieldsBuilder& remove_field(const std::string_view key) {
        std::erase_if(fields_, [&](payload_field_t const& f) { return f->key() == key; });
        last_ = nullptr;
        return *this;
    }

    /**
     * @brief Move out the accumulated field descriptors.
     * @return Vector of payload field descriptors.
     */
    payload_field_descriptors_t build() {
        last_ = nullptr;
        return std::move(fields_);
    }

private:
    payload_field_descriptors_t fields_;
    IPayloadFieldDescriptor<Payload>* last_ = nullptr;

    [[nodiscard]] IPayloadFieldDescriptor<Payload>*
    require_last(const std::string_view modifier) const {
        if (last_ == nullptr) {
            throw std::runtime_error("FieldsBuilder::" + std::string(modifier) +
                                     " called before any .field<>()");
        }
        return last_;
    }

    void replace_or_append(payload_field_t fd) {
        const auto key = fd->key();
        for (auto& existing : fields_) {
            if (existing->key() == key) {
                existing = fd;
                last_ = fd.get();
                return;
            }
        }
        last_ = fd.get();
        fields_.push_back(std::move(fd));
    }
};

/**
 * @brief Descriptor for `StructCell<Derived, Payload>`.
 *
 * Implements `IHasFields` (returns the field descriptors keyed by JSON
 * key) and `ISubTypes` (returns the cell kinds of every field).
 *
 * @tparam Derived The concrete struct cell type.
 * @tparam Payload The plain payload struct.
 */
template <typename Derived, typename Payload>
class StructCellTypeDescriptor final : public BaseCellTypeDescriptor<Derived>,
                                       public IHasFields,
                                       public ISubTypes {
    using base_t = BaseCellTypeDescriptor<Derived>;

public:
    using payload_field_descriptors_t =
        typename FieldsBuilder<Payload>::payload_field_descriptors_t;

    /**
     * @brief Construct with descriptive metadata and the field descriptor list.
     * @param meta   Cell-level descriptive metadata.
     * @param fields Field descriptor list (typically built via `FieldsBuilder`).
     */
    StructCellTypeDescriptor(DisplayInfo meta, payload_field_descriptors_t fields)
        : base_t(std::move(meta)), fields_(std::move(fields)) {}

    [[nodiscard]] descriptor::CellCategory category() const override {
        return descriptor::CellCategory::Struct;
    }

    [[nodiscard]] cell_t cell_from_json(json_t const& j, ParcelRegistry const& reg) const override {
        return Derived::from_json(j, reg);
    }

    [[nodiscard]] json_t to_json() const override {
        auto j = base_t::base_to_json();
        json_t arr = json_t::array();
        for (auto const& f : fields_) {
            arr.push_back(f->to_json());
        }
        j["fields"] = std::move(arr);
        return j;
    }

    [[nodiscard]] field_descriptors_t fields() const override {
        field_descriptors_t out;
        for (auto const& f : fields_) {
            out.emplace(std::string(f->key()), std::static_pointer_cast<IFieldDescriptor>(f));
        }
        return out;
    }

    [[nodiscard]] std::vector<std::string_view> sub_kinds() const override {
        std::vector<std::string_view> out;
        out.reserve(fields_.size());
        for (auto const& f : fields_) {
            out.push_back(f->kind());
        }
        return out;
    }

    /** @brief Read-only access to the typed payload-field descriptors. */
    [[nodiscard]] payload_field_descriptors_t const& payload_fields() const {
        return fields_;
    }

private:
    payload_field_descriptors_t fields_;
};

/**
 * @brief CRTP base for user-defined struct cells.
 *
 * Concrete `Derived` declares its bare struct id as the third template
 * argument; `StructCell` synthesizes the wire `kind_id` as
 * `"s:" + StructId` so user-defined struct kinds always live in the
 * `"s:"` namespace and never collide with primitive (`i32`) or container
 * (`l:`, `m:`, `u:`) kinds.
 *
 * @code{.cpp}
 * class PersonCell : public parcel::StructCell<PersonCell, Person, "person"> {
 *     using StructCell::StructCell;
 *     // PersonCell::struct_id == "person"
 *     // PersonCell::kind_id   == "s:person"
 * };
 * @endcode
 *
 * The concrete `Derived` must additionally provide:
 *   - `static auto field_descriptors()`             — built via `FieldsBuilder<Payload>`.
 *   - (optional) `static DisplayInfo meta_info()` — cell-level meta;
 *     defaults to an empty `DisplayInfo{}` when not declared.
 *   - (optional) `static constexpr bool allow_extra_fields` — defaults to false.
 *
 * Wire shape:
 * @code{.json}
 * {"k": "s:<name>", "v": {"<field>": <wrapped cell>, …}}
 * @endcode
 * Each field is itself a fully wrapped cell. When `allow_extra_fields` is
 * true, unknown keys are kept in `extras` as registry-dispatched cells;
 * otherwise unknown keys cause `from_json` to throw.
 *
 * @tparam Derived  The concrete struct cell.
 * @tparam Payload  The plain payload struct.
 * @tparam StructId Bare struct id (no `"s:"` prefix); the prefix is added
 *                  by the framework.
 * @see FieldsBuilder
 * @see StructCellTypeDescriptor
 */
template <typename Derived, typename Payload, FixedString StructId>
class StructCell : public BaseCell<Derived, Payload> {
    using base_t = BaseCell<Derived, Payload>;

public:
    using payload_type = Payload;

    using base_t::base_t;
    using base_t::operator=;

    /**
     * @brief Whether unknown JSON keys are tolerated.
     *
     * Concrete `Derived` types may shadow this with `true` to opt into
     * lenient deserialization; the default is strict (`false`).
     */
    static constexpr bool allow_extra_fields = false;

    /** @brief Bare struct id, lifted from the @p StructId template arg. */
    static constexpr std::string_view struct_id = StructId.view();

    /**
     * @brief Wire kind id of this struct cell (`"s:" + struct_id`).
     *
     * Synthesized at compile time from the @p StructId template argument.
     * The `"s:"` prefix is framework-enforced — concrete struct cells
     * never declare `kind_id` directly.
     */
    static constexpr std::string_view kind_id = id_join_lit_v<"s:", StructId>;

    /**
     * @brief Default cell-level meta (empty).
     *
     * Concrete `Derived` types may shadow this with their own
     * `static DisplayInfo meta_info()` (e.g. to set a display
     * name); when omitted, an empty `DisplayInfo{}` is used.
     */
    static DisplayInfo meta_info() {
        return {};
    }

    /** @brief Unknown keys retained when `Derived::allow_extra_fields` is true. */
    std::map<std::string, cell_t> extras;

    [[nodiscard]] std::string to_string() const override {
        std::string out = "{";
        bool first = true;
        for (auto const& f : Derived::field_descriptors()) {
            if (!first) {
                out += ", ";
            }
            f->to_string_into(out, this->value);
            first = false;
        }
        if constexpr (Derived::allow_extra_fields) {
            for (auto const& [key, ev] : extras) {
                if (!first) {
                    out += ", ";
                }
                out += key;
                out += ": ";
                out += ev ? ev->to_string() : "<null>";
                first = false;
            }
        }
        out += "}";
        return out;
    }

    [[nodiscard]] json_t to_json() const override {
        json_t v_obj = json_t::object();
        for (auto const& f : Derived::field_descriptors()) {
            f->to_json_into(v_obj, this->value);
        }
        if constexpr (Derived::allow_extra_fields) {
            for (auto const& [key, ev] : extras) {
                v_obj[key] = ev ? ev->to_json() : json_t();
            }
        }
        json_t j{
            {ICell::KEY_KIND, Derived::kind_id},
            {ICell::KEY_VALUE, std::move(v_obj)},
        };
        this->inject_meta(j);
        return j;
    }

    [[nodiscard]] cell_t clone() const override {
        auto copy = std::make_shared<Derived>(static_cast<Derived const&>(*this));
        if constexpr (Derived::allow_extra_fields) {
            copy->extras.clear();
            for (auto const& [key, ev] : extras) {
                copy->extras.emplace(key, ev ? ev->clone() : nullptr);
            }
        }
        return copy;
    }

    /**
     * @brief Three-way comparison over field values (and `extras` when
     *        `allow_extra_fields` is true); ignores meta.
     *
     * Walks declared fields in the order returned by `field_descriptors()`,
     * delegating each comparison to the field descriptor's typed
     * `compare(Payload const&, Payload const&)` hook (so e.g. nested struct
     * cells go through their own field-by-field compare instead of a JSON
     * projection). Short-circuits on the first non-equal result.
     *
     * Optional fields follow `std::optional`'s `<=>` (absent < present;
     * both absent is equivalent). When `allow_extra_fields` is true, after
     * declared fields agree the `extras` maps are compared lexicographically
     * by key, with ICell `<=>` resolving each value pair (a null cell ranks
     * before a non-null one).
     */
    [[nodiscard]] std::partial_ordering compare(ICell const& other) const override {
        if (const auto k_cmp = this->kind() <=> other.kind(); k_cmp != 0) {
            return k_cmp;
        }
        const auto* o = dynamic_cast<Derived const*>(&other);
        if (o == nullptr) {
            return std::partial_ordering::unordered;
        }
        for (auto const& f : Derived::field_descriptors()) {
            if (const auto c = f->compare(this->value, o->value); c != 0) {
                return c;
            }
        }
        if constexpr (Derived::allow_extra_fields) {
            // std::map already iterates by key; lexicographic compare on
            // (key, value-cell) pairs, with null-cell < non-null-cell.
            auto ai = extras.begin();
            auto bi = o->extras.begin();
            while (ai != extras.end() && bi != o->extras.end()) {
                if (const auto kc = ai->first <=> bi->first; kc != 0) {
                    return kc;
                }
                const bool an = ai->second == nullptr;
                if (const bool bn = bi->second == nullptr; an != bn) {
                    return an ? std::partial_ordering::less : std::partial_ordering::greater;
                }
                if (!an) {
                    if (const auto vc = *ai->second <=> *bi->second; vc != 0) {
                        return vc;
                    }
                }
                ++ai;
                ++bi;
            }
            return extras.size() <=> o->extras.size();
        }
        return std::partial_ordering::equivalent;
    }

    /**
     * @brief Deserialize the struct cell from JSON.
     *
     * Each declared field is read in order; missing required fields throw.
     * Unknown keys throw unless `Derived::allow_extra_fields` is true, in
     * which case they are routed through the registry into `extras`.
     *
     * @param j   Input JSON object — must match `{"k": kind_id, "v": {…}}`.
     * @param reg Registry used to dispatch nested cells.
     * @return Newly built cell handle.
     * @throws std::runtime_error on shape or kind mismatch, missing
     *         required field, or unknown key when extras are not allowed.
     */
    static cell_t from_json(json_t const& j, ParcelRegistry const& reg) {
        if (!j.is_object()) {
            throw InvalidJsonException(std::string("Expected JSON object for StructCell '") +
                                           std::string(Derived::kind_id) + "'",
                                       std::string(Derived::kind_id));
        }

        const auto it_k = j.find(ICell::KEY_KIND);
        if (it_k == j.end() || !it_k->is_string()) {
            throw InvalidJsonException(std::string("StructCell '") + std::string(Derived::kind_id) +
                                           "': missing/invalid 'k'",
                                       std::string(Derived::kind_id));
        }
        if (it_k->get<std::string_view>() != Derived::kind_id) {
            throw KindMismatchException(std::string("StructCell '") +
                                            std::string(Derived::kind_id) +
                                            "': missing/invalid 'k'",
                                        std::string(Derived::kind_id));
        }

        const auto it_v = j.find(ICell::KEY_VALUE);
        if (it_v == j.end() || !it_v->is_object()) {
            throw InvalidJsonException(std::string("StructCell '") + std::string(Derived::kind_id) +
                                           "': missing/invalid 'v' (expected object)",
                                       std::string(Derived::kind_id));
        }

        auto out = std::make_shared<Derived>();
        auto field_descs = Derived::field_descriptors();

        std::map<std::string_view, IPayloadFieldDescriptor<Payload> const*> by_key;
        for (auto const& f : field_descs) {
            by_key.emplace(f->key(), f.get());
        }

        for (auto const& f : field_descs) {
            const auto it = it_v->find(f->key());
            if (it == it_v->end()) {
                if (f->is_required()) {
                    throw MissingFieldException(
                        std::string("StructCell '") + std::string(Derived::kind_id) +
                            "': missing required field '" + std::string(f->key()) + "'",
                        std::string(Derived::kind_id),
                        std::string(f->key()));
                }
                continue;
            }
            f->from_json_into(*it, out->value, reg);
        }

        for (auto const& [key, raw] : it_v->items()) {
            if (by_key.contains(key)) {
                continue;
            }
            if constexpr (Derived::allow_extra_fields) {
                out->extras.emplace(key, reg.cell_from_json(raw));
            } else {
                throw TypeException(std::string("StructCell '") + std::string(Derived::kind_id) +
                                        "': unknown field '" + key + "'",
                                    std::string(Derived::kind_id),
                                    key);
            }
        }

        base_t::absorb_meta(j, out);
        return out;
    }

    /**
     * @brief Cached descriptor for this struct cell.
     * @return Shared `StructCellTypeDescriptor<Derived, Payload>` instance.
     */
    static cell_type_descriptor_t descriptor() {
        static const auto d = std::make_shared<StructCellTypeDescriptor<Derived, Payload>>(
            Derived::meta_info(), Derived::field_descriptors());
        return d;
    }
};

/**
 * @brief CRTP base for struct cells that *are* their own payload.
 *
 * Where `StructCell<Derived, Payload, StructId>` composes a separate
 * payload struct held in `BaseCell::value`, `SelfStructCell<Derived>`
 * inherits `ICell` directly and treats `Derived` itself as the payload.
 * Field descriptors are `IPayloadFieldDescriptor<Derived>` and address
 * members on the cell instance itself — there is no nested `value`.
 *
 * Library authors typically introduce a CRTP intermediate (e.g.
 * `BaseEvent<Self, EventId>`) on top of `SelfStructCell` to carry common
 * fields and to choose their own wire-namespace prefix:
 *
 * @code{.cpp}
 * template <typename Self, parcel::FixedString EventId>
 * class BaseEvent : public parcel::SelfStructCell<Self> {
 * public:
 *     static constexpr std::string_view kind_id =
 *         parcel::id_join_lit_v<"s:event:", EventId>;
 *
 * protected:
 *     std::int64_t timestamp_{};
 * public:
 *     [[nodiscard]] std::int64_t timestamp() const { return timestamp_; }
 *
 *     static auto field_descriptors() {
 *         auto builder = parcel::FieldsBuilder<Self>{};
 *         return Self::event_field_descriptors(builder)
 *             .template field<&BaseEvent::timestamp_>("timestamp")
 *             .build();
 *     }
 * };
 * @endcode
 *
 * Unlike `StructCell`, `SelfStructCell` has no `StructId` template
 * parameter and does not synthesize a `kind_id`; the deriving class (or a
 * CRTP intermediate) declares `static constexpr std::string_view kind_id`
 * directly, which gives library authors freedom to carve out their own
 * sub-namespace (e.g. `"s:event:something"`).
 *
 * `Derived` must additionally provide:
 *   - `static constexpr std::string_view kind_id` — wire kind id.
 *   - `static auto field_descriptors()`           — built via `FieldsBuilder<Derived>`.
 *   - (optional) `static DisplayInfo meta_info()` — cell-level meta;
 *     defaults to an empty `DisplayInfo{}` when not declared.
 *   - (optional) `static constexpr bool allow_extra_fields` — defaults to false.
 *
 * `Derived` must also be default-constructible *with access from
 * parcel-internal code* — `from_json` calls `std::make_shared<Derived>()`.
 * Keeping the default constructor `public` while making data members
 * `protected`/`private` is the standard pattern.
 *
 * @tparam Derived The concrete struct cell, which is also its own payload.
 */
template <typename Derived>
class SelfStructCell : public ICell {
public:
    using payload_type = Derived;

    /**
     * @brief Storage-type alias used by descriptors that probe `T::storage_t`.
     *
     * `SelfStructCell` does not own a separate payload — the cell *is* its
     * own payload — so `storage_t` resolves to `Derived` itself. Lets the
     * existing `BaseCellTypeDescriptor<Derived>::storage_type()` keep working
     * unchanged.
     */
    using storage_t = Derived;

    /**
     * @brief Whether unknown JSON keys are tolerated.
     *
     * Concrete `Derived` types may shadow this with `true` to opt into
     * lenient deserialization; the default is strict (`false`).
     */
    static constexpr bool allow_extra_fields = false;

    [[nodiscard]] std::string_view kind() const override {
        return Derived::kind_id;
    }

    [[nodiscard]] std::string to_string() const override {
        std::string out = "{";
        bool first = true;
        auto const& self = static_cast<Derived const&>(*this);
        for (auto const& f : Derived::field_descriptors()) {
            if (!first) {
                out += ", ";
            }
            f->to_string_into(out, self);
            first = false;
        }
        if constexpr (Derived::allow_extra_fields) {
            for (auto const& [key, ev] : cell_extras_) {
                if (!first) {
                    out += ", ";
                }
                out += key;
                out += ": ";
                out += ev ? ev->to_string() : "<null>";
                first = false;
            }
        }
        out += "}";
        return out;
    }

    [[nodiscard]] json_t to_json() const override {
        json_t v_obj = json_t::object();
        auto const& self = static_cast<Derived const&>(*this);
        for (auto const& f : Derived::field_descriptors()) {
            f->to_json_into(v_obj, self);
        }
        if constexpr (Derived::allow_extra_fields) {
            for (auto const& [key, ev] : cell_extras_) {
                v_obj[key] = ev ? ev->to_json() : json_t();
            }
        }
        json_t j{
            {ICell::KEY_KIND, Derived::kind_id},
            {ICell::KEY_VALUE, std::move(v_obj)},
        };
        this->inject_meta(j);
        return j;
    }

    [[nodiscard]] cell_t clone() const override {
        auto copy = std::make_shared<Derived>(static_cast<Derived const&>(*this));
        if constexpr (Derived::allow_extra_fields) {
            copy->cell_extras_.clear();
            for (auto const& [key, ev] : cell_extras_) {
                copy->cell_extras_.emplace(key, ev ? ev->clone() : nullptr);
            }
        }
        return copy;
    }

    /**
     * @brief Three-way comparison over field values (and `cell_extras_` when
     *        `allow_extra_fields` is true); ignores meta.
     *
     * Walks declared fields in the order returned by `field_descriptors()`
     * and short-circuits on the first non-equal result. Optional fields
     * follow `std::optional`'s `<=>`. When `allow_extra_fields` is true,
     * after declared fields agree the `cell_extras_` maps are compared
     * lexicographically by key, with ICell `<=>` resolving each value pair.
     */
    [[nodiscard]] std::partial_ordering compare(ICell const& other) const override {
        if (const auto k_cmp = this->kind() <=> other.kind(); k_cmp != 0) {
            return k_cmp;
        }
        const auto* o = dynamic_cast<Derived const*>(&other);
        if (o == nullptr) {
            return std::partial_ordering::unordered;
        }
        auto const& self = static_cast<Derived const&>(*this);
        for (auto const& f : Derived::field_descriptors()) {
            if (const auto c = f->compare(self, *o); c != 0) {
                return c;
            }
        }
        if constexpr (Derived::allow_extra_fields) {
            auto ai = cell_extras_.begin();
            auto bi = o->cell_extras_.begin();
            while (ai != cell_extras_.end() && bi != o->cell_extras_.end()) {
                if (const auto kc = ai->first <=> bi->first; kc != 0) {
                    return kc;
                }
                const bool an = ai->second == nullptr;
                if (const bool bn = bi->second == nullptr; an != bn) {
                    return an ? std::partial_ordering::less : std::partial_ordering::greater;
                }
                if (!an) {
                    if (const auto vc = *ai->second <=> *bi->second; vc != 0) {
                        return vc;
                    }
                }
                ++ai;
                ++bi;
            }
            return cell_extras_.size() <=> o->cell_extras_.size();
        }
        return std::partial_ordering::equivalent;
    }

    [[nodiscard]] const std::map<std::string, cell_t>& cell_extras() const {
        return cell_extras_;
    }

    /**
     * @brief Deserialize the self-payload struct cell from JSON.
     *
     * Mirrors `StructCell::from_json`, but field descriptors target
     * @p Derived itself rather than a separate payload type.
     *
     * @param j   Input JSON object — must match `{"k": kind_id, "v": {…}}`.
     * @param reg Registry used to dispatch nested cells.
     * @return Newly built cell handle.
     * @throws std::runtime_error on shape or kind mismatch, missing
     *         required field, or unknown key when extras are not allowed.
     */
    static cell_t from_json(json_t const& j, ParcelRegistry const& reg) {
        if (!j.is_object()) {
            throw InvalidJsonException(std::string("Expected JSON object for SelfStructCell '") +
                                           std::string(Derived::kind_id) + "'",
                                       std::string(Derived::kind_id));
        }

        const auto it_k = j.find(ICell::KEY_KIND);
        if (it_k == j.end() || !it_k->is_string()) {
            throw InvalidJsonException(std::string("SelfStructCell '") +
                                           std::string(Derived::kind_id) + "': missing/invalid 'k'",
                                       std::string(Derived::kind_id));
        }
        if (it_k->get<std::string_view>() != Derived::kind_id) {
            throw KindMismatchException(std::string("SelfStructCell '") +
                                            std::string(Derived::kind_id) +
                                            "': missing/invalid 'k'",
                                        std::string(Derived::kind_id));
        }

        const auto it_v = j.find(ICell::KEY_VALUE);
        if (it_v == j.end() || !it_v->is_object()) {
            throw InvalidJsonException(std::string("SelfStructCell '") +
                                           std::string(Derived::kind_id) +
                                           "': missing/invalid 'v' (expected object)",
                                       std::string(Derived::kind_id));
        }

        auto out = std::make_shared<Derived>();
        auto field_descs = Derived::field_descriptors();

        std::map<std::string_view, IPayloadFieldDescriptor<Derived> const*> by_key;
        for (auto const& f : field_descs) {
            by_key.emplace(f->key(), f.get());
        }

        for (auto const& f : field_descs) {
            const auto it = it_v->find(f->key());
            if (it == it_v->end()) {
                if (f->is_required()) {
                    throw MissingFieldException(
                        std::string("SelfStructCell '") + std::string(Derived::kind_id) +
                            "': missing required field '" + std::string(f->key()) + "'",
                        std::string(Derived::kind_id),
                        std::string(f->key()));
                }
                continue;
            }
            f->from_json_into(*it, *out, reg);
        }

        for (auto const& [key, raw] : it_v->items()) {
            if (by_key.contains(key)) {
                continue;
            }
            if constexpr (Derived::allow_extra_fields) {
                out->cell_extras_.emplace(key, reg.cell_from_json(raw));
            } else {
                throw TypeException(std::string("SelfStructCell '") +
                                        std::string(Derived::kind_id) + "': unknown field '" + key +
                                        "'",
                                    std::string(Derived::kind_id),
                                    key);
            }
        }

        if (const auto it_d = j.find(ICell::KEY_DESCRIPTION); it_d != j.end()) {
            out->meta_ = it_d->get<DisplayInfo>();
        }
        return out;
    }

    /**
     * @brief Cached descriptor for this self-payload struct cell.
     * @return Shared `StructCellTypeDescriptor<Derived, Derived>` instance.
     */
    static cell_type_descriptor_t descriptor() {
        static const auto d = std::make_shared<StructCellTypeDescriptor<Derived, Derived>>(
            Derived::meta_info(), Derived::field_descriptors());
        return d;
    }

    [[nodiscard]] std::optional<DisplayInfo> const& meta() const override {
        return meta_;
    }

protected:
    /**
     * @brief Copy this cell's meta (if any) into the JSON object under `"d"`.
     *
     * Mirrors `BaseCell::inject_meta` — re-declared here because
     * `SelfStructCell` inherits `ICell` directly, not `BaseCell`.
     *
     * @param j JSON object to mutate.
     */
    void inject_meta(json_t& j) const {
        if (meta_) {
            j[ICell::KEY_DESCRIPTION] = *meta_;
        }
    }

    void set_meta(std::optional<DisplayInfo> m) override {
        meta_ = std::move(m);
    }

    /** @brief Optional descriptive metadata; omitted from JSON when empty. */
    std::optional<DisplayInfo> meta_;

    /** @brief Unknown keys retained when `Derived::allow_extra_fields` is true. */
    std::map<std::string, cell_t> cell_extras_;
};

}  // namespace parcel
