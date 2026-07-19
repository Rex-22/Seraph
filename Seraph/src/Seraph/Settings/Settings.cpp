//
// Settings registry implementation. Descriptors are heap-owned (unique_ptr) so
// their addresses — and the OwnedValue cell / bound closures inside them — stay
// stable as the registry grows. Function-local static storage (static-init-order
// safe, ScriptRegistry / Reflection pattern).
//

#include "Seraph/Settings/Settings.h"

#include "Seraph/Core/CommandLine.h"
#include "Seraph/Core/Log.h"
#include "Seraph/Settings/ISettingsStore.h"
#include "Seraph/Settings/YamlSettingsStore.h"

#include <algorithm>
#include <array>
#include <charconv>
#include <glm/common.hpp>
#include <memory>
#include <unordered_map>
#include <unordered_set>

namespace Seraph
{

struct Settings::Registry
{
    struct Sub
    {
        SubscriptionToken Token;
        std::string Key; // empty = global
        SettingChangeFn Fn;
    };

    std::vector<std::unique_ptr<SettingDescriptor>> Owned;
    std::unordered_map<std::string, SettingDescriptor*> ByKey;
    std::vector<SettingDescriptor*> AllList;
    std::array<bool, 3> DirtyScope{false, false, false}; // indexed by SettingScope

    std::vector<Sub> Subs;
    SubscriptionToken NextToken = 1;
    std::unordered_set<std::string> Notifying; // re-entrancy guard (per key)
    bool RestartPending = false;
    Ref<ISettingsStore> Backend;
};

Settings::Registry& Settings::Store()
{
    static Registry s;
    return s;
}

namespace
{
// Gentle convention nudge: keys are namespaced engine.* / editor.* / game.*.
void WarnOnBadNamespace(const std::string& key)
{
    if (key.rfind("engine.", 0) == 0 || key.rfind("editor.", 0) == 0
        || key.rfind("game.", 0) == 0)
        return;
    SP_CORE_WARN_TAG("Settings",
                     "Setting key '{}' has no engine./editor./game. namespace",
                     key);
}
} // namespace

SettingBuilder Settings::Register(std::string key)
{
    Registry& r = Store();

    if (auto it = r.ByKey.find(key); it != r.ByKey.end())
    {
        SP_CORE_WARN_TAG("Settings", "Setting '{}' registered more than once",
                         key);
        return SettingBuilder(it->second);
    }

    WarnOnBadNamespace(key);

    auto desc = std::make_unique<SettingDescriptor>();
    desc->Key = key;
    SettingDescriptor* p = desc.get();
    r.Owned.push_back(std::move(desc));
    r.ByKey.emplace(std::move(key), p);
    r.AllList.push_back(p);
    return SettingBuilder(p);
}

const SettingDescriptor* Settings::Find(std::string_view key)
{
    Registry& r = Store();
    auto it = r.ByKey.find(std::string(key));
    return it == r.ByKey.end() ? nullptr : it->second;
}

const std::vector<SettingDescriptor*>& Settings::All()
{
    return Store().AllList;
}

Any Settings::GetAny(std::string_view key)
{
    const SettingDescriptor* d = Find(key);
    if (!d)
    {
        SP_CORE_WARN_TAG("Settings", "GetAny: unknown setting '{}'", key);
        return {};
    }
    return d->Read();
}

namespace
{
// Parse a flat string into an Any of the setting's scalar type (for --set).
// Structural types (vec/matrix/etc.) aren't supported on the command line; those
// go through the YAML store (Settings 3). Returns an empty Any on failure.
template<class T>
bool FromChars(const std::string& s, T& out)
{
    auto [p, ec] = std::from_chars(s.data(), s.data() + s.size(), out);
    return ec == std::errc() && p == s.data() + s.size();
}

Any ParseScalar(const Type* type, const std::string& s)
{
    if (!type)
        return {};
    const TypeId id = type->Id;
    if (id == TypeIdOf<bool>())
        return Any(s == "true" || s == "1");
    if (id == TypeIdOf<std::string>())
        return Any(s);
    if (id == TypeIdOf<s32>())
    { s32 v; if (FromChars(s, v)) return Any(v); }
    else if (id == TypeIdOf<u32>())
    { u32 v; if (FromChars(s, v)) return Any(v); }
    else if (id == TypeIdOf<s64>())
    { s64 v; if (FromChars(s, v)) return Any(v); }
    else if (id == TypeIdOf<u64>())
    { u64 v; if (FromChars(s, v)) return Any(v); }
    else if (id == TypeIdOf<f32>())
    { f32 v; if (FromChars(s, v)) return Any(v); }
    else if (id == TypeIdOf<f64>())
    { f64 v; if (FromChars(s, v)) return Any(v); }
    return {};
}

template<class T>
Any ClampScalar(const Any& v, const Any* mn, const Any* mx)
{
    T val = *v.Cast<T>();
    if (mn)
        if (const T* m = mn->Cast<T>())
            val = std::max(val, *m);
    if (mx)
        if (const T* m = mx->Cast<T>())
            val = std::min(val, *m);
    return Any(val);
}

template<class V>
Any ClampVec(const Any& v, const Any* mn, const Any* mx)
{
    V val = *v.Cast<V>();
    if (mn)
        if (const V* m = mn->Cast<V>())
            val = glm::max(val, *m);
    if (mx)
        if (const V* m = mx->Cast<V>())
            val = glm::min(val, *m);
    return Any(val);
}

// Clamp a value against the setting's Min/Max attributes (if present + matching
// type). Supported: numeric scalars + glm vec2/3/4 (component-wise). Others pass
// through unchanged.
Any Clamp(const Any& v, const SettingDescriptor* d)
{
    const Any* mn = d->Attrs.Find(Setting::Attr::Min);
    const Any* mx = d->Attrs.Find(Setting::Attr::Max);
    if ((!mn && !mx) || !d->ValueType || v.IsEmpty())
        return v;
    const TypeId id = d->ValueType->Id;
    if (id == TypeIdOf<s32>()) return ClampScalar<s32>(v, mn, mx);
    if (id == TypeIdOf<u32>()) return ClampScalar<u32>(v, mn, mx);
    if (id == TypeIdOf<s64>()) return ClampScalar<s64>(v, mn, mx);
    if (id == TypeIdOf<u64>()) return ClampScalar<u64>(v, mn, mx);
    if (id == TypeIdOf<f32>()) return ClampScalar<f32>(v, mn, mx);
    if (id == TypeIdOf<f64>()) return ClampScalar<f64>(v, mn, mx);
    if (id == TypeIdOf<glm::vec2>()) return ClampVec<glm::vec2>(v, mn, mx);
    if (id == TypeIdOf<glm::vec3>()) return ClampVec<glm::vec3>(v, mn, mx);
    if (id == TypeIdOf<glm::vec4>()) return ClampVec<glm::vec4>(v, mn, mx);
    return v;
}

// Rough Any equality for change detection (avoids notifying on no-op writes).
// Compares supported scalar/vec types by value; unknown types report "changed".
bool AnyEquals(const Any& a, const Any& b)
{
    if (a.GetTypeId() != b.GetTypeId())
        return false;
    const TypeId id = a.GetTypeId();
    if (id == TypeIdOf<bool>()) return *a.Cast<bool>() == *b.Cast<bool>();
    if (id == TypeIdOf<s32>()) return *a.Cast<s32>() == *b.Cast<s32>();
    if (id == TypeIdOf<u32>()) return *a.Cast<u32>() == *b.Cast<u32>();
    if (id == TypeIdOf<s64>()) return *a.Cast<s64>() == *b.Cast<s64>();
    if (id == TypeIdOf<u64>()) return *a.Cast<u64>() == *b.Cast<u64>();
    if (id == TypeIdOf<f32>()) return *a.Cast<f32>() == *b.Cast<f32>();
    if (id == TypeIdOf<f64>()) return *a.Cast<f64>() == *b.Cast<f64>();
    if (id == TypeIdOf<std::string>())
        return *a.Cast<std::string>() == *b.Cast<std::string>();
    if (id == TypeIdOf<glm::vec2>()) return *a.Cast<glm::vec2>() == *b.Cast<glm::vec2>();
    if (id == TypeIdOf<glm::vec3>()) return *a.Cast<glm::vec3>() == *b.Cast<glm::vec3>();
    if (id == TypeIdOf<glm::vec4>()) return *a.Cast<glm::vec4>() == *b.Cast<glm::vec4>();
    return false; // unknown type -> treat as changed
}
} // namespace

void Settings::ApplyChange(const SettingDescriptor* d, const Any& value,
                           bool markDirty, bool isRuntimeEdit, SettingSetBy source)
{
    Registry& r = Store();
    if (r.Notifying.count(d->Key))
    {
        SP_CORE_WARN_TAG("Settings", "re-entrant Set on '{}' ignored", d->Key);
        return;
    }

    // SetBy precedence: a lower-priority source can't overwrite a higher one — so a
    // scope reload (Engine/Project/User) leaves a live Console/CLI override intact.
    if (source < d->Source)
        return;

    Any clamped = Clamp(value, d);
    if (d->ValueType && !clamped.IsEmpty()
        && clamped.GetTypeId() != d->ValueType->Id)
    {
        // Enum settings carry their value as s64 (the reflection enum convention)
        // while ValueType is the reflected enum Type.
        const bool enumAsS64 = d->ValueType->Kind == TypeKind::Enum
                               && clamped.GetTypeId() == TypeIdOf<s64>();
        if (!enumAsS64)
        {
            SP_CORE_WARN_TAG("Settings", "'{}' type mismatch", d->Key);
            return;
        }
    }

    // Record the winning source now — even if the value is unchanged — so a
    // console set to the current value still protects it from later reloads.
    const_cast<SettingDescriptor*>(d)->Source = source;

    Any old = d->Read();
    if (AnyEquals(old, clamped))
        return; // no-op: don't dirty, don't notify

    auto* m = const_cast<SettingDescriptor*>(d);
    m->Write(clamped);
    if (markDirty && !d->CliOverridden)
        MarkScopeDirty(d->Scope);

    // RequiresRestart runtime edits persist (above) but take effect next launch.
    if (isRuntimeEdit && d->HasFlag(SettingFlag_RequiresRestart))
    {
        r.RestartPending = true;
        return; // no live notification
    }

    r.Notifying.insert(d->Key);
    FireNotify(*d, old, clamped);
    r.Notifying.erase(d->Key);
}

void Settings::FireNotify(const SettingDescriptor& d, const Any& oldValue,
                          const Any& newValue)
{
    // Copy matching subscribers first so (un)subscribing inside a callback is safe.
    std::vector<SettingChangeFn> fns;
    for (const Registry::Sub& s : Store().Subs)
        if (s.Key.empty() || s.Key == d.Key)
            fns.push_back(s.Fn);
    for (const SettingChangeFn& fn : fns)
        fn(d, oldValue, newValue);
}

void Settings::SetAny(std::string_view key, const Any& value)
{
    const SettingDescriptor* d = Find(key);
    if (!d)
    {
        SP_CORE_WARN_TAG("Settings", "SetAny: unknown setting '{}'", key);
        return;
    }
    if (d->HasFlag(SettingFlag_ReadOnly))
    {
        SP_CORE_WARN_TAG("Settings", "SetAny: '{}' is read-only", key);
        return;
    }
    if (d->CliOverridden)
        return; // CLI override is authoritative; a user edit can't dislodge it

    ApplyChange(d, value, /*markDirty*/ true, /*isRuntimeEdit*/ true,
                SettingSetBy::Console);
}

namespace
{
SettingSetBy SetByForScope(SettingScope scope)
{
    switch (scope)
    {
        case SettingScope::Engine:  return SettingSetBy::Engine;
        case SettingScope::Project: return SettingSetBy::Project;
        case SettingScope::User:    return SettingSetBy::User;
    }
    return SettingSetBy::Engine;
}
} // namespace

void Settings::ApplyLoaded(std::string_view key, const Any& value,
                           SettingScope loadedFrom)
{
    const SettingDescriptor* d = Find(key);
    if (!d)
        return; // unknown key in a file -> ignored (registry is the schema)
    if (d->CliOverridden)
        return; // a --set override outranks a loaded value
    ApplyChange(d, value, /*markDirty*/ false, /*isRuntimeEdit*/ false,
                SetByForScope(loadedFrom));
}

void Settings::ApplyCommandLineOverrides()
{
    const std::vector<std::string>& args = CommandLine::Args();
    for (std::size_t i = 0; i < args.size(); ++i)
    {
        if (args[i] != "--set" || i + 1 >= args.size())
            continue;
        const std::string& kv = args[++i];
        auto eq = kv.find('=');
        if (eq == std::string::npos)
        {
            SP_CORE_WARN_TAG("Settings", "--set expects key=value, got '{}'", kv);
            continue;
        }
        std::string key = kv.substr(0, eq);
        std::string valStr = kv.substr(eq + 1);
        const SettingDescriptor* d = Find(key);
        if (!d)
        {
            SP_CORE_WARN_TAG("Settings", "--set: unknown setting '{}'", key);
            continue;
        }
        if (d->CliOverridden)
            continue; // already applied on an earlier pass (Engine/User vs Project)
        Any parsed = ParseScalar(d->ValueType, valStr);
        if (parsed.IsEmpty())
        {
            SP_CORE_WARN_TAG("Settings", "--set: cannot parse '{}' for '{}'",
                             valStr, key);
            continue;
        }
        auto* m = const_cast<SettingDescriptor*>(d);
        m->Write(parsed);
        m->CliOverridden = true; // highest precedence, never persisted
        m->Source = SettingSetBy::CommandLine;
        SP_CORE_INFO_TAG("Settings", "--set {}={}", key, valStr);
    }
}

bool Settings::IsCommandLineOverridden(std::string_view key)
{
    const SettingDescriptor* d = Find(key);
    return d && d->CliOverridden;
}

bool Settings::IsScopeDirty(SettingScope scope)
{
    return Store().DirtyScope[static_cast<std::size_t>(scope)];
}

void Settings::MarkScopeDirty(SettingScope scope)
{
    Store().DirtyScope[static_cast<std::size_t>(scope)] = true;
}

void Settings::ClearScopeDirty(SettingScope scope)
{
    Store().DirtyScope[static_cast<std::size_t>(scope)] = false;
}

void Settings::Init()
{
    Registry& r = Store();
    if (!r.Backend)
        r.Backend = Ref<YamlSettingsStore>::Create();
    SP_CORE_INFO_TAG("Settings", "initialized");
}

void Settings::Shutdown()
{
    SaveDirty();
    Clear();
}

void Settings::InstallStore(Ref<ISettingsStore> store)
{
    Store().Backend = std::move(store);
}

void Settings::LoadEngineUser()
{
    Registry& r = Store();
    if (!r.Backend)
        return;
    r.Backend->Load(SettingScope::Engine, false);
    r.Backend->Load(SettingScope::Engine, true);
    r.Backend->Load(SettingScope::User, false);
    r.Backend->Load(SettingScope::User, true);
    ApplyCommandLineOverrides();
}

void Settings::LoadProject()
{
    Registry& r = Store();
    if (!r.Backend)
        return;
    r.Backend->Load(SettingScope::Project, false);
    r.Backend->Load(SettingScope::Project, true);
    ApplyCommandLineOverrides(); // catches --set for project/game keys registered late
}

void Settings::SaveDirty()
{
    Registry& r = Store();
    if (r.Backend)
        SaveDirty(*r.Backend);
}

void Settings::PurgeByPrefix(std::string_view prefix)
{
    Registry& r = Store();
    int removed = 0;
    for (auto it = r.AllList.begin(); it != r.AllList.end();)
    {
        if ((*it)->Key.rfind(prefix, 0) == 0)
        {
            const std::string key = (*it)->Key;
            it = r.AllList.erase(it);
            r.ByKey.erase(key);
            std::erase_if(r.Owned,
                          [&key](const std::unique_ptr<SettingDescriptor>& d)
                          { return d->Key == key; });
            std::erase_if(r.Subs, [&key](const Registry::Sub& s)
                          { return s.Key == key; });
            ++removed;
        }
        else
            ++it;
    }
    if (removed)
        SP_CORE_INFO_TAG("Settings", "Purged {} setting(s) with prefix '{}'",
                         removed, prefix);
}

void Settings::LoadAll(ISettingsStore& store)
{
    for (SettingScope s :
         {SettingScope::Engine, SettingScope::Project, SettingScope::User})
    {
        store.Load(s, /*platformOverlay*/ false);
        store.Load(s, /*platformOverlay*/ true);
    }
    ApplyCommandLineOverrides(); // highest precedence, last
}

void Settings::SaveDirty(ISettingsStore& store)
{
    if (!store.SupportsWrite())
        return;
    for (SettingScope s :
         {SettingScope::Engine, SettingScope::Project, SettingScope::User})
    {
        if (IsScopeDirty(s) && store.Save(s))
            ClearScopeDirty(s);
    }
}

Settings::SubscriptionToken Settings::Subscribe(std::string_view key,
                                                SettingChangeFn fn)
{
    Registry& r = Store();
    SubscriptionToken t = r.NextToken++;
    r.Subs.push_back({t, std::string(key), std::move(fn)});
    return t;
}

Settings::SubscriptionToken Settings::SubscribeAll(SettingChangeFn fn)
{
    Registry& r = Store();
    SubscriptionToken t = r.NextToken++;
    r.Subs.push_back({t, std::string(), std::move(fn)});
    return t;
}

void Settings::Unsubscribe(SubscriptionToken token)
{
    Registry& r = Store();
    std::erase_if(r.Subs,
                  [token](const Registry::Sub& s) { return s.Token == token; });
}

bool Settings::IsRestartPending()
{
    return Store().RestartPending;
}

void Settings::ResetToDefault(std::string_view key)
{
    const SettingDescriptor* d = Find(key);
    if (!d)
    {
        SP_CORE_WARN_TAG("Settings", "ResetToDefault: unknown '{}'", key);
        return;
    }
    if (d->DefaultValue.IsEmpty())
    {
        SP_CORE_WARN_TAG("Settings", "ResetToDefault: '{}' has no default", key);
        return;
    }
    SetAny(key, d->DefaultValue); // validated + notified + dirtied like any edit
}

void Settings::Clear()
{
    Registry& r = Store();
    r.ByKey.clear();
    r.AllList.clear();
    r.Owned.clear();
    r.DirtyScope = {false, false, false};
    r.Subs.clear();
    r.Notifying.clear();
    r.RestartPending = false;
    r.NextToken = 1;
}

} // namespace Seraph
