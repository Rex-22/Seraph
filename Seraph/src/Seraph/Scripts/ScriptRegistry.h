//
// Created by Ruben on 2026/07/14.
//
#pragma once

#include <functional>
#include <string>
#include <unordered_map>

namespace Seraph
{
class ScriptableEntity;

// Global name -> factory registry. Native scripts self-register their class
// under a stable string name (via SP_REGISTER_SCRIPT); scenes serialize and
// instantiate scripts by that name. The string indirection is what lets a
// scene reference a script without a compile-time type, and what a future VM
// backend would slot into.
class ScriptRegistry
{
public:
    using FactoryFn = std::function<ScriptableEntity*()>;

    // Bind a name to a factory. Warns (and overwrites) on a duplicate name.
    static void Register(const std::string& name, FactoryFn factory);

    // Heap-allocate a fresh instance for `name`, or nullptr if unknown. The
    // caller (ScriptEngine) owns the returned pointer.
    static ScriptableEntity* Create(const std::string& name);

    static bool Exists(const std::string& name);

    // For editor enumeration (the inspector class dropdown).
    static const std::unordered_map<std::string, FactoryFn>& GetAll();

    // Drop all registrations. Called before unloading a script module (dylib) so
    // no factory lambda outlives the code it points into. Only safe when no
    // ScriptEngine/instances are live (i.e. not playing).
    static void Clear();

private:
    // Function-local static: constructed on first use, so registration from any
    // translation unit's static-init is safe regardless of init order.
    static std::unordered_map<std::string, FactoryFn>& Storage();
};

} // namespace Seraph

// Register a ScriptableEntity subclass under a name. Place at file scope in the
// script's .cpp:  SP_REGISTER_SCRIPT(PlayerController, "PlayerController")
//
// NOTE: the enclosing translation unit must be linked directly into the
// executable (e.g. via a CMake OBJECT library, as the Game module is) — a
// self-registering .cpp buried in a *static archive* is dead-stripped because
// nothing references this symbol. See docs/scripting-overview.md.
#define SP_REGISTER_SCRIPT(Type, Name)                                         \
    namespace                                                                  \
    {                                                                          \
    const bool k_##Type##_Registered = []                                      \
    {                                                                          \
        ::Seraph::ScriptRegistry::Register(                                    \
            Name,                                                              \
            [] { return static_cast<::Seraph::ScriptableEntity*>(new Type()); }); \
        return true;                                                           \
    }();                                                                       \
    }
