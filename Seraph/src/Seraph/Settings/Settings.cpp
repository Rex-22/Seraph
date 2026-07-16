//
// Settings registry implementation. Descriptors are heap-owned (unique_ptr) so
// their addresses — and the OwnedValue cell / bound closures inside them — stay
// stable as the registry grows. Function-local static storage (static-init-order
// safe, ScriptRegistry / Reflection pattern).
//

#include "Seraph/Settings/Settings.h"

#include "Seraph/Core/Log.h"

#include <memory>
#include <unordered_map>

namespace Seraph
{

struct Settings::Registry
{
    std::vector<std::unique_ptr<SettingDescriptor>> Owned;
    std::unordered_map<std::string, SettingDescriptor*> ByKey;
    std::vector<SettingDescriptor*> AllList;
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
    // Type guard: refuse a value whose type doesn't match the setting's.
    if (d->ValueType && !value.IsEmpty()
        && value.GetTypeId() != d->ValueType->Id)
    {
        SP_CORE_WARN_TAG("Settings", "SetAny: '{}' type mismatch", key);
        return;
    }
    // NOTE: clamp/validate against Min/Max and change-notify land in Settings 4.
    const_cast<SettingDescriptor*>(d)->Write(value);
}

void Settings::Clear()
{
    Registry& r = Store();
    r.ByKey.clear();
    r.AllList.clear();
    r.Owned.clear();
}

} // namespace Seraph
