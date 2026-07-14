//
// Created by Ruben on 2026/07/14.
//

#pragma once

#include <string>
#include <utility>

namespace Seraph
{
class ScriptableEntity;

struct ScriptComponent
{
    std::string ScriptClass;
    ScriptableEntity* Instance = nullptr;

    ScriptComponent() = default;
    explicit ScriptComponent(std::string scriptClass): ScriptClass(std::move(scriptClass)) {}
};

}