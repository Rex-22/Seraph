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

#include <array>
#include <charconv>
#include <memory>
#include <unordered_map>

namespace Seraph
{

struct Settings::Registry
{
    std::vector<std::unique_ptr<SettingDescriptor>> Owned;
    std::unordered_map<std::string, SettingDescriptor*> ByKey;
    std::vector<SettingDescriptor*> AllList;
    std::array<bool, 3> DirtyScope{false, false, false}; // indexed by SettingScope
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

// Shared type-checked write. Returns the mutable descriptor on success, else null.
SettingDescriptor* CheckedWrite(const SettingDescriptor* d, std::string_view key,
                                const Any& value, const char* who)
{
    if (d->ValueType && !value.IsEmpty()
        && value.GetTypeId() != d->ValueType->Id)
    {
        SP_CORE_WARN_TAG("Settings", "{}: '{}' type mismatch", who, key);
        return nullptr;
    }
    auto* m = const_cast<SettingDescriptor*>(d);
    m->Write(value); // NOTE: clamp/validate + change-notify land in Settings 4
    return m;
}
} // namespace

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

    if (CheckedWrite(d, key, value, "SetAny"))
        MarkScopeDirty(d->Scope); // user edit -> its scope needs saving
}

void Settings::ApplyLoaded(std::string_view key, const Any& value)
{
    const SettingDescriptor* d = Find(key);
    if (!d)
        return; // unknown key in a file -> ignored (registry is the schema)
    if (d->CliOverridden)
        return; // a --set override outranks a loaded value
    CheckedWrite(d, key, value, "ApplyLoaded"); // loaded, not edited: no dirty
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

void Settings::Clear()
{
    Registry& r = Store();
    r.ByKey.clear();
    r.AllList.clear();
    r.Owned.clear();
    r.DirtyScope = {false, false, false};
}

} // namespace Seraph
