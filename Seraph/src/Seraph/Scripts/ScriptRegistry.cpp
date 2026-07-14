//
// Created by Ruben on 2026/07/14.
//

#include "ScriptRegistry.h"

#include "Seraph/Core/Log.h"

namespace Seraph
{

std::unordered_map<std::string, ScriptRegistry::FactoryFn>&
ScriptRegistry::Storage()
{
    static std::unordered_map<std::string, FactoryFn> s_Map;
    return s_Map;
}

void ScriptRegistry::Register(const std::string& name, FactoryFn factory)
{
    auto& map = Storage();
    if (map.contains(name))
        SP_CORE_WARN_TAG("Scripting", "Script '{}' registered more than once", name);
    map[name] = std::move(factory);
}

ScriptableEntity* ScriptRegistry::Create(const std::string& name)
{
    const auto& map = Storage();
    const auto it = map.find(name);
    return it != map.end() ? it->second() : nullptr;
}

bool ScriptRegistry::Exists(const std::string& name)
{
    return Storage().contains(name);
}

const std::unordered_map<std::string, ScriptRegistry::FactoryFn>&
ScriptRegistry::GetAll()
{
    return Storage();
}

} // namespace Seraph
