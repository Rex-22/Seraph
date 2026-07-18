//
// The runtime reflection data model a consumer walks: Type, Property, EnumInfo,
// and the open AttributeSet. Domain-agnostic — Reflection defines NO attribute
// keys (settings/editor/serializer each own their own vocabulary; see
// Todo/plans/reflection-plan.md example D). Types are created by the
// registration builder (Reflection 3) and owned by the Reflection registry, so
// every const Type* / const Property* is stable for the type's registered
// lifetime.
//

#pragma once

#include "Seraph/Core/Base.h"
#include "Seraph/Reflection/Any.h"
#include "Seraph/Reflection/TypeId.h"

#include <memory>
#include <optional>
#include <string_view>
#include <vector>

namespace Seraph
{

enum class TypeKind : u8
{
    Primitive,
    Struct,
    Enum,
    Container, // std::vector<E>-like; see ContainerInfo
    Reference, // typed handle to another object (EntityRef / TAssetRef<T>); the
               // value crosses the Any boundary as a UUID, exactly as an enum
               // crosses as an s64. See Reflection/Reference.h.
};

// Open bag of hashed-key -> Any attributes. Keys are constexpr hashes of stable
// strings, minted by each domain via AttributeKey(). Linear scan: attribute sets
// are tiny (a handful of entries per property).
class AttributeSet
{
public:
    void Set(u64 key, Any value)
    {
        for (Entry& e : m_Entries)
        {
            if (e.Key == key)
            {
                e.Value = std::move(value);
                return;
            }
        }
        m_Entries.push_back({key, std::move(value)});
    }

    [[nodiscard]] const Any* Find(u64 key) const noexcept
    {
        for (const Entry& e : m_Entries)
            if (e.Key == key)
                return &e.Value;
        return nullptr;
    }

    [[nodiscard]] bool Has(u64 key) const noexcept
    {
        return Find(key) != nullptr;
    }

    // Typed convenience: value pointer, or nullptr if absent / wrong type.
    template<class T>
    [[nodiscard]] const T* Get(u64 key) const noexcept
    {
        const Any* a = Find(key);
        return a ? a->Cast<T>() : nullptr;
    }

    [[nodiscard]] bool Empty() const noexcept { return m_Entries.empty(); }

private:
    struct Entry
    {
        u64 Key;
        Any Value;
    };
    std::vector<Entry> m_Entries;
};

// Mint a stable attribute key from a domain-owned string, e.g.
//   inline constexpr u64 Section = AttributeKey("settings.section");
constexpr u64 AttributeKey(std::string_view name)
{
    return Fnv1a(name);
}

struct Type;

struct Property
{
    std::string_view Name;
    const Type* PropType = nullptr; // resolved at registration, or back-patched by
                                    // Reflection::Register when PropTypeId's type
                                    // registers later (static-init order safety)
    TypeId PropTypeId = 0;          // identity of the property's type (for back-patch)
    AttributeSet Attrs;

    // Type-erased accessors bound to the owning object. Get copies the field out
    // into an Any; Set writes an Any back into the field. Plain function
    // pointers (no captures) so a Property is trivially copyable data.
    Any (*Get)(const void* obj) = nullptr;
    void (*Set)(void* obj, const Any& value) = nullptr;

    // Address of the member within `obj`, or null for computed properties. Lets
    // consumers operate on a live sub-object in place (e.g. a container's
    // elements via ContainerInfo) without copying it through an Any.
    void* (*GetAddress)(void* obj) = nullptr;
};

// Runtime ops for a reflected container (std::vector<E>-like). The `container`
// pointer is a live element address (from Property::GetAddress). Element values
// cross the boundary as Any of ElementType.
struct ContainerInfo
{
    const Type* ElementType = nullptr;
    TypeId ElementTypeId = 0; // identity of the element type (for back-patch, like
                              // Property::PropTypeId — resolved late if the element
                              // type registers after the container)
    std::size_t (*Size)(const void* container) = nullptr;
    Any (*GetElement)(const void* container, std::size_t index) = nullptr;
    void (*SetElement)(void* container, std::size_t index, const Any& value) = nullptr;
    void (*Resize)(void* container, std::size_t size) = nullptr; // default-fills new
};

// A reflected, invocable method. Args and the return value cross as Any (return
// is empty for void). Invoke asserts on arg-count mismatch. The `obj` pointer is
// a live instance of the owning Type.
struct MethodInfo
{
    std::string_view Name;
    const Type* ReturnType = nullptr; // null for void
    std::vector<const Type*> ParamTypes;
    Any (*Invoke)(void* obj, const Any* args, std::size_t argc) = nullptr;
};

struct EnumInfo
{
    struct Entry
    {
        std::string_view Name;
        s64 Value;
    };
    std::vector<Entry> Entries;

    [[nodiscard]] const Entry* FindByName(std::string_view name) const noexcept
    {
        for (const Entry& e : Entries)
            if (e.Name == name)
                return &e;
        return nullptr;
    }

    [[nodiscard]] const Entry* FindByValue(s64 value) const noexcept
    {
        for (const Entry& e : Entries)
            if (e.Value == value)
                return &e;
        return nullptr;
    }
};

struct Type
{
    TypeId Id = 0;
    std::string_view Name;
    TypeKind Kind = TypeKind::Struct;
    u32 Size = 0;
    u32 Align = 0;

    const Type* Base = nullptr;          // single-inheritance chain (v1)
    std::vector<Property> Properties;    // own + inherited, resolved at registration
    std::unique_ptr<EnumInfo> Enum;      // owned; non-null iff Kind == Enum
    std::unique_ptr<ContainerInfo> Container; // owned; non-null iff Kind == Container
    std::vector<MethodInfo> Methods;     // reflected invocable methods
    AttributeSet Attrs;

    // Given a pointer to an instance held as this (base) type, return its most-
    // derived reflected Type. Set only for types using the intrusive SP_REFLECT()
    // macro (Reflection 4); nullptr means "no dynamic resolution — static type".
    const Type* (*DynamicType)(const void* obj) = nullptr;

    // Default-construct a fresh value into an Any. Auto-populated for reflected
    // types that are default- and copy-constructible; null otherwise. For
    // deserialization / "new instance" flows.
    Any (*DefaultConstruct)() = nullptr;

    // Heap-construct a fresh, default-constructed instance and return its address
    // (the caller owns it — `delete` through the appropriate type). Auto-populated
    // for default-constructible, non-abstract reflected types; null otherwise.
    // Unlike DefaultConstruct (a value inside an Any), this yields an owned raw
    // instance for POLYMORPHIC use — e.g. the script system news a ScriptableEntity
    // subclass purely from its reflected type, with no compile-time knowledge of it.
    // NOTE: the returned pointer is the MOST-DERIVED object address; converting it
    // to a base pointer is only valid for single inheritance with the base at
    // offset 0 (consistent with the v1 single-Base model above).
    void* (*HeapConstruct)() = nullptr;

    [[nodiscard]] const Property* FindProperty(std::string_view name) const noexcept
    {
        for (const Property& p : Properties)
            if (p.Name == name)
                return &p;
        return nullptr;
    }

    [[nodiscard]] const MethodInfo* FindMethod(std::string_view name) const noexcept
    {
        for (const MethodInfo& m : Methods)
            if (m.Name == name)
                return &m;
        return nullptr;
    }
};

// Enum <-> string over a reflected enum Type. Return nullopt if `t` is not an
// enum or the name/value is unknown.
[[nodiscard]] inline std::optional<std::string_view> EnumToString(
    const Type& t, s64 value) noexcept
{
    if (!t.Enum)
        return std::nullopt;
    if (const EnumInfo::Entry* e = t.Enum->FindByValue(value))
        return e->Name;
    return std::nullopt;
}

[[nodiscard]] inline std::optional<s64> EnumFromString(
    const Type& t, std::string_view name) noexcept
{
    if (!t.Enum)
        return std::nullopt;
    if (const EnumInfo::Entry* e = t.Enum->FindByName(name))
        return e->Value;
    return std::nullopt;
}

} // namespace Seraph
