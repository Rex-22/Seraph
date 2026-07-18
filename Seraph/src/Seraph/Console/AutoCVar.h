//
// AutoCVar — a console variable declared at file scope, anywhere in the codebase,
// that reads at zero overhead. It owns its value cell and binds a Settings entry
// to it (so console `set`/`get`, clamping, change hooks, and persistence all come
// from the Settings registry — a CVar IS a setting), while callers read the live
// value directly through Get() with no registry lookup.
//
//   static Seraph::AutoCVar<bool> CVarBloom{
//       "r.bloom", true, Seraph::CVarFlag_None, "Enable the bloom pass"};
//   ... if (CVarBloom.Get()) { ... }
//   // or, equivalently, the SP_CVAR sugar:
//   SP_CVAR(CVarBloom, bool, "r.bloom", true, Seraph::CVarFlag_None, "Enable bloom");
//
// Registration is DEFERRED: an AutoCVar constructed during static init (before
// Settings::Init / before the reflection registry is populated) enqueues itself
// and is flushed by Console::Init. One constructed later (e.g. from a hot-reloaded
// game module, after the flush) registers immediately.
//
// Persistence: console CVars are TRANSIENT by default (never written to disk).
// Pass CVarFlag_Archive to persist the value to the User scope — the opt-in
// mirror of Unreal's ECVF_Archive.
//

#pragma once

#include "Seraph/Core/Base.h"
#include "Seraph/Settings/SettingDescriptor.h"
#include "Seraph/Settings/Settings.h"

#include <functional>
#include <string>
#include <utility>
#include <vector>

namespace Seraph
{

// CVar-facing flags for SP_CVAR / AutoCVar. Translated to SettingFlags + scope at
// registration; Archive is a registration directive, not stored on the descriptor.
enum CVarFlags : u32
{
    CVarFlag_None = 0,
    CVarFlag_Cheat = BIT(0),    // settable only when cheats are enabled
    CVarFlag_ReadOnly = BIT(1), // exposes engine state; the console can't set it
    CVarFlag_Archive = BIT(2),  // persist to the User scope (else transient)
};

namespace Detail
{
// Deferred-registration plumbing (implemented in AutoCVar.cpp). EnqueueOrRun runs
// `fn` now if the queue was already flushed, else records it for the flush.
void EnqueueOrRunCVarRegistration(std::function<void()> fn);
} // namespace Detail

// Flush every pending AutoCVar registration into the Settings registry. Called
// once by Console::Init after Settings::Init. Idempotent.
void FlushPendingCVarRegistrations();

template<class T>
class AutoCVar
{
public:
    AutoCVar(std::string key, T defaultValue, u32 flags = CVarFlag_None,
             std::string help = {})
        : m_Key(std::move(key)), m_Value(defaultValue),
          m_Default(defaultValue), m_Flags(flags), m_Help(std::move(help))
    {
        Detail::EnqueueOrRunCVarRegistration([this] { Registrate(); });
    }

    AutoCVar(const AutoCVar&) = delete;
    AutoCVar& operator=(const AutoCVar&) = delete;

    // Zero-overhead read of the live value (no registry lookup).
    [[nodiscard]] const T& Get() const { return m_Value; }
    [[nodiscard]] operator const T&() const { return m_Value; }

    // Set through the registry so clamping / change notification / dirty-tracking
    // all run (a no-op-with-warning if called before the registration flush).
    void Set(const T& v) { Settings::Set<T>(m_Key, v); }

    [[nodiscard]] const std::string& Key() const { return m_Key; }

    // Run `fn(newValue)` whenever the CVar changes. Deferred like registration, so
    // it is safe to call from a constructor before the flush.
    void OnChanged(std::function<void(const T&)> fn)
    {
        Detail::EnqueueOrRunCVarRegistration(
            [this, fn = std::move(fn)]() mutable { Subscribe(std::move(fn)); });
    }

private:
    void Registrate()
    {
        u32 settingFlags = 0;
        if (m_Flags & CVarFlag_Cheat)
            settingFlags |= SettingFlag_Cheat;
        if (m_Flags & CVarFlag_ReadOnly)
            settingFlags |= SettingFlag_ReadOnly;
        const bool archive = (m_Flags & CVarFlag_Archive) != 0;
        if (!archive)
            settingFlags |= SettingFlag_Transient; // runtime-only unless archived

        SettingBuilder b = Settings::Register(m_Key)
                               .Bind(&m_Value)
                               .Default(m_Default)
                               .Scope(SettingScope::User)
                               .Flags(settingFlags);
        if (!m_Help.empty())
            b.Tooltip(m_Help);
    }

    void Subscribe(std::function<void(const T&)> fn)
    {
        Settings::Subscribe(m_Key,
                            [fn = std::move(fn)](const SettingDescriptor&,
                                                 const Any&, const Any& nv)
                            {
                                if (const T* v = nv.template Cast<T>())
                                    fn(*v);
                            });
    }

    std::string m_Key;
    T m_Value;
    T m_Default;
    u32 m_Flags;
    std::string m_Help;
};

} // namespace Seraph

// Declare a named AutoCVar at file scope. `var` is the C++ handle you read via
// var.Get(); the remaining arguments mirror the AutoCVar constructor.
#define SP_CVAR(var, Type, key, defaultValue, flags, help)                     \
    static ::Seraph::AutoCVar<Type> var{key, defaultValue, flags, help}
