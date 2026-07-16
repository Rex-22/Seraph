//
// A setting = a reflected value (Any + reflected Type) plus settings policy: a
// stable string key, a scope, and a metadata attribute bag. Built on the
// reflection layer (Any / Type / AttributeSet). See Todo/plans/settings-plan.md.
//
// Two bindings (the hybrid model, forced by the Game-module hot-reload rule):
//   * BOUND  — accessors close over a live engine field pointer (zero-overhead
//              hot reads happen on the field directly, never through here). The
//              owning struct need NOT be reflected.
//   * OWNED  — the value lives in an Any cell this descriptor owns, in engine
//              memory (for script / no-C++-home settings; safe across reload).
//

#pragma once

#include "Seraph/Core/Base.h"
#include "Seraph/Reflection/Any.h"
#include "Seraph/Reflection/Type.h"

#include <functional>
#include <string>

namespace Seraph
{

// Which tier owns/persists a setting and where a UI edit writes back. Platform is
// an orthogonal overlay (Settings 3), not a scope.
enum class SettingScope : u8
{
    Engine,  // engine defaults (read-only tier)
    Project, // per-project
    User,    // per-user
};

// UI/behaviour flags, stored as the Setting::Attr::Flags attribute value.
enum SettingFlags : u32
{
    SettingFlag_None = 0,
    SettingFlag_ReadOnly = BIT(0),
    SettingFlag_Hidden = BIT(1),
    SettingFlag_Advanced = BIT(2),
    SettingFlag_RequiresRestart = BIT(3),
};

// Settings-domain attribute keys (stored in SettingDescriptor::Attrs, which is a
// reflection AttributeSet). Reflection ships no keys; each domain owns its own.
namespace Setting::Attr
{
inline constexpr u64 Section = AttributeKey("settings.section");
inline constexpr u64 DisplayName = AttributeKey("settings.display");
inline constexpr u64 Tooltip = AttributeKey("settings.tooltip");
inline constexpr u64 Min = AttributeKey("settings.min");
inline constexpr u64 Max = AttributeKey("settings.max");
inline constexpr u64 Step = AttributeKey("settings.step");
inline constexpr u64 EnumLabels = AttributeKey("settings.enumlabels");
inline constexpr u64 Flags = AttributeKey("settings.flags");
} // namespace Setting::Attr

struct SettingDescriptor
{
    std::string Key;
    SettingScope Scope = SettingScope::User;
    const Type* ValueType = nullptr; // reflected type of the value (for the UI)
    AttributeSet Attrs;              // settings metadata (Section, Tooltip, ...)
    Any DefaultValue;                // for reset-to-default (Settings 4)

    // BOUND: closures over a live field pointer. Empty for owned settings.
    std::function<Any()> BoundGet;
    std::function<void(const Any&)> BoundSet;
    // OWNED: the value cell (used iff !IsBound). Stable because descriptors are
    // heap-owned (unique_ptr) by the registry.
    Any OwnedValue;
    bool IsBound = false;

    // Read/write the current value regardless of binding kind.
    [[nodiscard]] Any Read() const { return IsBound ? BoundGet() : OwnedValue; }
    void Write(const Any& v)
    {
        if (IsBound)
            BoundSet(v);
        else
            OwnedValue = v;
    }

    [[nodiscard]] u32 FlagBits() const
    {
        const u32* f = Attrs.Get<u32>(Setting::Attr::Flags);
        return f ? *f : SettingFlag_None;
    }
    [[nodiscard]] bool HasFlag(SettingFlags f) const
    {
        return (FlagBits() & f) != 0;
    }
};

} // namespace Seraph
