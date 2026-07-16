//
// Registration front-end: the SP_REFLECT_TYPE(...) ... SP_REFLECT_END() macro
// pair and the fluent TypeBuilder it drives. This is the v1 (hand-written) way
// to feed the Reflection registry; SeraphHeaderTool (SHT) generates the same
// calls from annotations in Phase 2. See Todo/plans/reflection-plan.md
// (Registration API).
//
// Member binding uses a NON-TYPE TEMPLATE PARAMETER for the pointer-to-member:
//
//     SP_REFLECT_TYPE(PhysicsSettings)
//         .Property<&PhysicsSettings::Gravity>("Gravity")
//             .Attr(SomeDomain::Tooltip, Any(std::string("world gravity")))
//         .Property<&PhysicsSettings::MaxBodies>("MaxBodies")
//     SP_REFLECT_END();
//
// Passing the member as a template argument (not a runtime argument) is what
// makes the generated Get/Set thunks STATELESS plain function pointers — a
// captured runtime member-pointer would force a heap std::function. Computed
// properties use free-function accessors, also as template args:
//
//         .Property<&GetFov, &SetFov>("Fov")
//
// No trailing ';' is needed before SP_REFLECT_END() (the macro supplies it).
//

#pragma once

#include "Seraph/Core/Assert.h"
#include "Seraph/Reflection/Reflection.h"
#include "Seraph/Reflection/Type.h"
#include "Seraph/Reflection/TypeId.h"

#include <string_view>
#include <type_traits>
#include <utility>

namespace Seraph
{

namespace Detail
{

template<class P>
struct MemberPointerTraits;

template<class C, class M>
struct MemberPointerTraits<M C::*>
{
    using ClassType = C;
    using MemberType = M;
};

} // namespace Detail

// Builds one Type via a fluent chain, then Commit()s it into the registry.
// Constructed by SP_REFLECT_TYPE; finalized by SP_REFLECT_END.
template<class T>
class TypeBuilder
{
public:
    TypeBuilder()
    {
        m_Type.Id = TypeIdOf<T>();
        m_Type.Name = TypeName<T>();
        m_Type.Kind = TypeKind::Struct;
        m_Type.Size = static_cast<u32>(sizeof(T));
        m_Type.Align = static_cast<u32>(alignof(T));
    }

    // Data-member property. Member is a pointer-to-data-member NTTP, e.g.
    // .Property<&Foo::Bar>("Bar"). Works for a member of T or of a base of T.
    template<auto Member>
    TypeBuilder& Property(std::string_view name)
    {
        using Traits = Detail::MemberPointerTraits<decltype(Member)>;
        using M = typename Traits::MemberType;
        using C = typename Traits::ClassType;
        static_assert(std::is_base_of_v<C, T> || std::is_same_v<C, T>,
                      "Property<&C::m>(): m must belong to T or a base of T");

        // ::Seraph::Property (the struct) — qualified because the member
        // function 'Property' shadows the struct name inside this class.
        ::Seraph::Property p;
        p.Name = name;
        p.PropType = Reflection::TryGet<M>();

        if constexpr (std::is_enum_v<M>)
        {
            // Enum members are represented in the Any as their underlying value
            // widened to s64, with PropType pointing at the reflected enum Type.
            // This lets consumers map value <-> label via EnumInfo without any
            // Any enum-extraction (the thunk does the conversion at the boundary).
            p.Get = +[](const void* obj) -> Any
            { return Any(static_cast<s64>(static_cast<const T*>(obj)->*Member)); };
            p.Set = +[](void* obj, const Any& v)
            {
                const s64* i = v.template Cast<s64>();
                SP_CORE_ASSERT(i != nullptr,
                               "Property::Set: enum expects an s64 Any");
                if (i)
                    static_cast<T*>(obj)->*Member = static_cast<M>(*i);
            };
        }
        else
        {
            p.Get = +[](const void* obj) -> Any
            { return Any(static_cast<const T*>(obj)->*Member); };
            p.Set = +[](void* obj, const Any& v)
            {
                const M* m = v.template Cast<M>();
                SP_CORE_ASSERT(m != nullptr,
                               "Property::Set: Any type does not match the field");
                if (m)
                    static_cast<T*>(obj)->*Member = *m;
            };
        }
        AddProperty(std::move(p));
        return *this;
    }

    // Computed property from free-function accessors (both NTTP):
    //   Getter: M(*)(const T&)      Setter: void(*)(T&, const M&)  (M by value ok)
    template<auto Getter, auto Setter>
    TypeBuilder& Property(std::string_view name)
    {
        using M = std::decay_t<std::invoke_result_t<decltype(Getter), const T&>>;

        ::Seraph::Property p;
        p.Name = name;
        p.PropType = Reflection::TryGet<M>();
        p.Get = +[](const void* obj) -> Any
        { return Any(Getter(*static_cast<const T*>(obj))); };
        p.Set = +[](void* obj, const Any& v)
        {
            const M* m = v.template Cast<M>();
            SP_CORE_ASSERT(m != nullptr,
                           "Property::Set: Any type does not match the field");
            if (m)
                Setter(*static_cast<T*>(obj), *m);
        };
        AddProperty(std::move(p));
        return *this;
    }

    // Attach an attribute to the last-added property, or to the type itself if
    // no property has been added yet.
    TypeBuilder& Attr(u64 key, Any value)
    {
        if (m_LastProperty != k_NoProperty)
            m_Type.Properties[m_LastProperty].Attrs.Set(key, std::move(value));
        else
            m_Type.Attrs.Set(key, std::move(value));
        return *this;
    }

    // Declare T's (single) base. The base MUST already be registered — its
    // flattened property list is spliced in at Commit(). Loud assert otherwise.
    template<class TBase>
    TypeBuilder& Base()
    {
        static_assert(std::is_base_of_v<TBase, T>,
                      "Base<TBase>(): T must derive from TBase");
        const Type* base = Reflection::Resolve(TypeIdOf<TBase>());
        SP_CORE_ASSERT(base,
                       "Base '{}' must be registered before derived '{}'",
                       TypeName<TBase>(), TypeName<T>());
        m_Base = base;
        return *this;
    }

    // Enable dynamic type resolution: given a pointer to a T (possibly held as a
    // base pointer), Type::DynamicType recovers the most-derived reflected Type
    // via the virtual GetType(). Requires T to declare a public virtual
    // GetType() (via SP_REFLECT(), or hand-declared on a root like
    // ScriptableEntity).
    TypeBuilder& Dynamic()
    {
        m_Type.DynamicType = +[](const void* obj) -> const Type*
        { return &static_cast<const T*>(obj)->GetType(); };
        return *this;
    }

    // Run T's intrusive member hook (declared by SP_REFLECT(), defined by
    // SP_REFLECT_IMPL). TypeBuilder<T> is a friend of T, so it may call the
    // private static hook; the hook, being a member of T, can form pointers to
    // T's PRIVATE members. Used by SP_REFLECT_IMPL_END — not called directly.
    TypeBuilder& IntrusiveMembers()
    {
        T::SpReflectMembers(*this);
        return *this;
    }

    // Finalize: splice inherited properties (base is already flattened), then
    // hand the Type to the registry. Called by SP_REFLECT_END().
    void Commit()
    {
        SP_CORE_ASSERT(!m_Committed, "TypeBuilder::Commit() called twice");
        m_Committed = true;
        m_Type.Base = m_Base;

        if (m_Base)
        {
            std::vector<::Seraph::Property> flattened;
            flattened.reserve(m_Base->Properties.size()
                              + m_Type.Properties.size());
            for (const ::Seraph::Property& bp : m_Base->Properties)
                flattened.push_back(bp);
            for (::Seraph::Property& op : m_Type.Properties)
                flattened.push_back(std::move(op));
            m_Type.Properties = std::move(flattened);
        }

        Reflection::Register(std::move(m_Type));
    }

private:
    static constexpr std::size_t k_NoProperty = static_cast<std::size_t>(-1);

    void AddProperty(::Seraph::Property&& p)
    {
        m_Type.Properties.push_back(std::move(p));
        m_LastProperty = m_Type.Properties.size() - 1; // index: realloc-safe
    }

    Type m_Type;
    const Type* m_Base = nullptr;
    std::size_t m_LastProperty = k_NoProperty;
    bool m_Committed = false;
};

// Builds a reflected enum Type: name<->value entries + labels. Driven by the
// SP_REFLECT_ENUM(...) ... SP_REFLECT_ENUM_END() macro pair.
template<class E>
class EnumBuilder
{
public:
    EnumBuilder()
    {
        m_Type.Id = TypeIdOf<E>();
        m_Type.Name = TypeName<E>();
        m_Type.Kind = TypeKind::Enum;
        m_Type.Size = static_cast<u32>(sizeof(E));
        m_Type.Align = static_cast<u32>(alignof(E));
        m_Type.Enum = std::make_unique<EnumInfo>();
    }

    EnumBuilder& Value(std::string_view name, E value)
    {
        m_Type.Enum->Entries.push_back({name, static_cast<s64>(value)});
        return *this;
    }

    void Commit()
    {
        SP_CORE_ASSERT(!m_Committed, "EnumBuilder::Commit() called twice");
        m_Committed = true;
        Reflection::Register(std::move(m_Type));
    }

private:
    Type m_Type;
    bool m_Committed = false;
};

} // namespace Seraph

#define SP_REFLECT_CONCAT_(a, b) a##b
#define SP_REFLECT_CONCAT(a, b) SP_REFLECT_CONCAT_(a, b)

// Open a registration block for type T. Follow with a fluent .Property/.Attr/
// .Base chain (no leading or trailing ';'), then SP_REFLECT_END().
#define SP_REFLECT_TYPE(T)                                                     \
    namespace                                                                  \
    {                                                                          \
    const bool SP_REFLECT_CONCAT(k_spReflected_, __COUNTER__) = []             \
    {                                                                          \
        ::Seraph::TypeBuilder<T> _sp_builder;                                  \
        _sp_builder

// Close a registration block: commit the built Type to the registry.
#define SP_REFLECT_END()                                                       \
    ;                                                                          \
    _sp_builder.Commit();                                                      \
    return true;                                                               \
    }                                                                          \
    ();                                                                        \
    }

// ---------------------------------------------------------------------------
// Intrusive reflection: reach PRIVATE members + polymorphic dynamic resolution.
//
// A private member pointer (&T::m_Private) can only be formed where access is
// granted — inside a member of T. So SP_REFLECT(T) declares a private member
// hook (and befriends TypeBuilder<T> to invoke it) plus a virtual GetType(); the
// hook's body (the .Property<...> chain) is written out-of-line via
// SP_REFLECT_IMPL, where it is a member of T and therefore reaches privates.
// See example B.
//
// Placement: put SP_REFLECT(T) at the TOP of the class body. It leaves the class
// in `private:` access — add your own access specifier afterwards.
//
//   class Enemy : public ScriptableEntity {
//       SP_REFLECT(Enemy);
//       float m_Health = 100.f;   // private
//   };
//   // in the .cpp:
//   SP_REFLECT_IMPL(Enemy)
//       .Base<ScriptableEntity>()
//       .Property<&Enemy::m_Health>("Health")
//   SP_REFLECT_IMPL_END(Enemy)
// ---------------------------------------------------------------------------

#define SP_REFLECT(T)                                                          \
public:                                                                        \
    virtual const ::Seraph::Type& GetType() const;                             \
                                                                               \
private:                                                                       \
    friend class ::Seraph::TypeBuilder<T>;                                     \
    static void SpReflectMembers(::Seraph::TypeBuilder<T>& _sp_builder)

// Open the out-of-line hook definition for an intrusive type. Defines T's
// GetType() and begins T's member hook; follow with the fluent chain. The
// builder here is the concrete TypeBuilder<T>, so .Property/.Base need no
// '.template' disambiguation.
#define SP_REFLECT_IMPL(T)                                                      \
    const ::Seraph::Type& T::GetType() const                                   \
    {                                                                          \
        return ::Seraph::Reflection::Get<T>();                                 \
    }                                                                          \
    void T::SpReflectMembers(::Seraph::TypeBuilder<T>& _sp_builder)            \
    {                                                                          \
        _sp_builder

// Close the hook and emit the self-registering trigger (runs the hook, marks
// dynamic resolution, commits).
#define SP_REFLECT_IMPL_END(T)                                                  \
    ;                                                                          \
    }                                                                          \
    namespace                                                                  \
    {                                                                          \
    const bool SP_REFLECT_CONCAT(k_spReflIntrusive_, __COUNTER__) = []         \
    {                                                                          \
        ::Seraph::TypeBuilder<T> _sp_builder;                                  \
        _sp_builder.IntrusiveMembers().Dynamic().Commit();                     \
        return true;                                                           \
    }();                                                                       \
    }

// ---------------------------------------------------------------------------
// Enum reflection. Registers name<->value entries so EnumToString /
// EnumFromString (Type.h) work, and Any round-trips the enum by its own type.
//
//   SP_REFLECT_ENUM(MaterialParameterType)
//       .Value("Bool",  MaterialParameterType::Bool)
//       .Value("Float", MaterialParameterType::Float)
//   SP_REFLECT_ENUM_END();
// ---------------------------------------------------------------------------

#define SP_REFLECT_ENUM(E)                                                     \
    namespace                                                                  \
    {                                                                          \
    const bool SP_REFLECT_CONCAT(k_spReflEnum_, __COUNTER__) = []              \
    {                                                                          \
        ::Seraph::EnumBuilder<E> _sp_enum;                                     \
        _sp_enum

#define SP_REFLECT_ENUM_END()                                                  \
    ;                                                                          \
    _sp_enum.Commit();                                                         \
    return true;                                                               \
    }                                                                          \
    ();                                                                        \
    }
