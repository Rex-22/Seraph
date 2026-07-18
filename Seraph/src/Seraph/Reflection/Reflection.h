//
// Reflection — the runtime type registry (static facade, like FileSystem /
// AssetManager). Holds every registered Type, owns their storage so const Type*
// stay stable, and resolves types by TypeId, by name, or by static type. The
// SP_REFLECT_TYPE builder (Reflection 3) feeds Register(); consumers (settings,
// editor, serializer) only ever read. See Todo/plans/reflection-plan.md
// (Public facade).
//
// v1 registers into one global set. Module-scoped registration + ClearModule for
// the hot-reloaded Game module arrives in Reflection 5.
//

#pragma once

#include "Seraph/Core/Assert.h"
#include "Seraph/Reflection/Type.h"
#include "Seraph/Reflection/TypeId.h"

#include <string_view>
#include <vector>

namespace Seraph
{

// Which code module owns a registration. Types from the hot-reloaded Game dylib
// are tagged k_GameModule and dropped (ClearModule) before the dylib unmaps, so
// no Property::Get/Set thunk (a function pointer into that code) dangles. Engine
// types are k_EngineModule and live for the whole process. See example A.
using ModuleTag = u32;
inline constexpr ModuleTag k_EngineModule = 0;
inline constexpr ModuleTag k_GameModule = 1;

class Reflection
{
public:
    Reflection() = delete;

    // Look up a registered type; nullptr if not registered.
    static const Type* Resolve(TypeId id) noexcept;
    static const Type* Resolve(std::string_view name) noexcept;

    // By static type; nullptr if T is not registered.
    template<class T>
    static const Type* TryGet() noexcept
    {
        return Resolve(TypeIdOf<T>());
    }

    // By static type; verifies (loud even in release) if T is not registered —
    // callers that can tolerate absence use TryGet(). Get() returns a reference,
    // so an unregistered T has no valid result: SP_CORE_VERIFY traps deterministically
    // rather than dereferencing null (SP_CORE_ASSERT compiles out in release).
    template<class T>
    static const Type& Get()
    {
        const Type* t = Resolve(TypeIdOf<T>());
        SP_CORE_VERIFY(t, "Reflection::Get<T>(): '{}' is not registered",
                       TypeName<T>());
        return *t;
    }

    // Every registered type, in registration order (editor enumeration).
    static const std::vector<const Type*>& All();

    // Low-level registration. Moves `type` into owned storage and returns a
    // stable pointer, tagging it with the current module (see SetCurrentModule).
    // The SP_REFLECT_TYPE builder is the intended caller.
    //   * Re-registering the same TypeId with the same name is idempotent
    //     (returns the existing type, warns).
    //   * A TypeId clash between two DIFFERENT names is a hash collision and
    //     fails loudly (SP_CORE_ASSERT).
    static const Type* Register(Type&& type);

    // Module scoping for hot reload. The script-module loader brackets the dylib
    // load with SetCurrentModule(k_GameModule) so types self-registering during
    // load are tagged Game; ClearModule(k_GameModule) drops them (and frees their
    // Type storage) before the dylib unmaps. Engine types are untouched.
    //
    // IMPORTANT: after a Game-module reload, any cached `const Type*` /
    // `const Property*` for a Game type is DANGLING — re-resolve by TypeId (stable
    // across reload) or by name. Never cache descriptor pointers for Game types
    // across a reload boundary.
    static ModuleTag CurrentModule() noexcept;
    static void SetCurrentModule(ModuleTag module) noexcept;
    static void ClearModule(ModuleTag module) noexcept;

private:
    struct Storage;
    static Storage& Store();
};

// RAII: set the current registration module for a scope, restore on exit. The
// script loader wraps SDL_LoadObject in one of these.
class ReflectionModuleScope
{
public:
    explicit ReflectionModuleScope(ModuleTag module)
        : m_Previous(Reflection::CurrentModule())
    {
        Reflection::SetCurrentModule(module);
    }
    ~ReflectionModuleScope() { Reflection::SetCurrentModule(m_Previous); }

    ReflectionModuleScope(const ReflectionModuleScope&) = delete;
    ReflectionModuleScope& operator=(const ReflectionModuleScope&) = delete;

private:
    ModuleTag m_Previous;
};

} // namespace Seraph
