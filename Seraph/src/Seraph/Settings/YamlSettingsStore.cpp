//
// YamlSettingsStore — Any <-> YAML conversion + per-scope file I/O.
//

#include "Seraph/Settings/YamlSettingsStore.h"

#include "Seraph/Core/Buffer.h"
#include "Seraph/Core/FileSystem.h"
#include "Seraph/Core/Log.h"
#include "Seraph/Reflection/Reflection.h"
#include "Seraph/Settings/Settings.h"

#include <glm/glm.hpp>
#include <yaml-cpp/yaml.h>

#include <string>

namespace Seraph
{

namespace
{

const char* PlatformName()
{
#if defined(_WIN32)
    return "Windows";
#elif defined(__APPLE__)
    return "Mac";
#elif defined(__linux__)
    return "Linux";
#else
    return "Unknown";
#endif
}

Root RootForScope(SettingScope scope)
{
    switch (scope)
    {
        case SettingScope::Engine:  return Root::Engine;
        case SettingScope::Project: return Root::Project;
        case SettingScope::User:    return Root::User;
    }
    return Root::User;
}

const char* ScopeStem(SettingScope scope)
{
    switch (scope)
    {
        case SettingScope::Engine:  return "EngineSettings";
        case SettingScope::Project: return "ProjectSettings";
        case SettingScope::User:    return "UserSettings";
    }
    return "UserSettings";
}

std::string FileName(SettingScope scope, bool platformOverlay)
{
    std::string name = ScopeStem(scope);
    if (platformOverlay)
    {
        name += '.';
        name += PlatformName();
    }
    return name + ".yaml";
}

// --- Any <-> YAML ---

template<class T>
bool Is(const Type* t)
{
    return t && t->Id == TypeIdOf<T>();
}

YAML::Node AnyToNode(const Any& v, const Type* t)
{
    YAML::Node n;
    if (!t)
        return n;

    // NOTE: enum-valued settings aren't supported by the YAML store yet — reading
    // an enum's underlying integer generically from a type-erased Any needs a raw
    // accessor Any doesn't expose. None of the v1 consumers use enum settings;
    // deferred (see SettingsBoard).
    if (t->Kind == TypeKind::Enum)
    {
        SP_CORE_WARN_TAG("Settings", "YAML: enum setting '{}' not yet persisted",
                         t->Name);
        return n;
    }

    if (Is<bool>(t)) n = *v.Cast<bool>();
    else if (Is<s32>(t)) n = *v.Cast<s32>();
    else if (Is<u32>(t)) n = *v.Cast<u32>();
    else if (Is<s64>(t)) n = *v.Cast<s64>();
    else if (Is<u64>(t)) n = *v.Cast<u64>();
    else if (Is<f32>(t)) n = *v.Cast<f32>();
    else if (Is<f64>(t)) n = *v.Cast<f64>();
    else if (Is<std::string>(t)) n = *v.Cast<std::string>();
    else if (Is<glm::vec2>(t)) { auto* p = v.Cast<glm::vec2>(); n.push_back(p->x); n.push_back(p->y); }
    else if (Is<glm::vec3>(t)) { auto* p = v.Cast<glm::vec3>(); n.push_back(p->x); n.push_back(p->y); n.push_back(p->z); }
    else if (Is<glm::vec4>(t)) { auto* p = v.Cast<glm::vec4>(); n.push_back(p->x); n.push_back(p->y); n.push_back(p->z); n.push_back(p->w); }
    else
        SP_CORE_WARN_TAG("Settings", "YAML: unsupported type '{}'", t->Name);
    return n;
}

Any NodeToAny(const YAML::Node& n, const Type* t)
{
    if (!t || !n)
        return {};
    try
    {
        if (Is<bool>(t)) return Any(n.as<bool>());
        if (Is<s32>(t)) return Any(n.as<s32>());
        if (Is<u32>(t)) return Any(n.as<u32>());
        if (Is<s64>(t)) return Any(n.as<s64>());
        if (Is<u64>(t)) return Any(n.as<u64>());
        if (Is<f32>(t)) return Any(n.as<f32>());
        if (Is<f64>(t)) return Any(n.as<f64>());
        if (Is<std::string>(t)) return Any(n.as<std::string>());
        if (Is<glm::vec2>(t) && n.size() == 2)
            return Any(glm::vec2{n[0].as<f32>(), n[1].as<f32>()});
        if (Is<glm::vec3>(t) && n.size() == 3)
            return Any(glm::vec3{n[0].as<f32>(), n[1].as<f32>(), n[2].as<f32>()});
        if (Is<glm::vec4>(t) && n.size() == 4)
            return Any(glm::vec4{n[0].as<f32>(), n[1].as<f32>(), n[2].as<f32>(), n[3].as<f32>()});
    }
    catch (const std::exception& e)
    {
        SP_CORE_WARN_TAG("Settings", "YAML: bad value for '{}': {}", t->Name,
                         e.what());
    }
    return {};
}

} // namespace

bool YamlSettingsStore::Load(SettingScope scope, bool platformOverlay)
{
    const Root root = RootForScope(scope);
    const std::string file = FileName(scope, platformOverlay);

    if (!FileSystem::Exists(root, file))
        return true; // missing is fine (first run / no overlay)

    Buffer bytes;
    if (!FileSystem::Read(root, file, bytes))
        return true;

    YAML::Node doc;
    try
    {
        doc = YAML::Load(std::string(bytes.As<char>(), bytes.Size()));
    }
    catch (const std::exception& e)
    {
        SP_CORE_ERROR_TAG("Settings", "Corrupt settings file '{}': {}", file,
                          e.what());
        return false;
    }
    if (!doc.IsMap())
        return true;

    int applied = 0;
    for (const auto& kv : doc)
    {
        const std::string key = kv.first.as<std::string>();
        const SettingDescriptor* d = Settings::Find(key);
        if (!d)
            continue; // registry is the schema; unknown keys ignored
        Any v = NodeToAny(kv.second, d->ValueType);
        if (!v.IsEmpty())
        {
            Settings::ApplyLoaded(key, v);
            ++applied;
        }
    }
    SP_CORE_INFO_TAG("Settings", "Loaded {} ({} values)", file, applied);
    return true;
}

bool YamlSettingsStore::Save(SettingScope scope)
{
    YAML::Node doc;
    int count = 0;
    for (const SettingDescriptor* d : Settings::All())
    {
        if (d->Scope != scope || d->CliOverridden)
            continue;
        if (d->HasFlag(SettingFlag_Transient))
            continue; // runtime-only debug CVars are never persisted
        YAML::Node n = AnyToNode(d->Read(), d->ValueType);
        if (n.IsDefined())
        {
            doc[d->Key] = n;
            ++count;
        }
    }

    YAML::Emitter out;
    out << doc;
    const std::string file = FileName(scope, /*platformOverlay*/ false);
    const Root root = RootForScope(scope);
    if (!FileSystem::Write(root, file, Buffer::Copy(out.c_str(), out.size())))
    {
        SP_CORE_ERROR_TAG("Settings", "Failed to write '{}'", file);
        return false;
    }
    SP_CORE_INFO_TAG("Settings", "Saved {} ({} values)", file, count);
    return true;
}

} // namespace Seraph
