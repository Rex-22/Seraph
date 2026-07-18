//
// Created by Ruben on 2026/07/14.
//

#pragma once

#include "Seraph/Reflection/Annotations.h"
#include "Seraph/Reflection/Any.h"

#include <string>
#include <unordered_map>
#include <utility>

namespace Seraph
{
class ScriptableEntity;

struct SCLASS() ScriptComponent
{
    SPROPERTY()
    std::string ScriptClass;

    // Authored per-entity values for the script's reflected fields (reflected
    // field name -> value). Edited in the inspector, persisted in the scene, and
    // applied to the instance right before OnCreate (ScriptEngine). Empty means
    // "use the script's own defaults". Managed bespoke rather than reflected: the
    // field SET is dynamic — it depends on which class ScriptClass names — so it
    // can't be a fixed reflected property. Holds plain values (TypeId + bytes), so
    // it stays valid across a script hot-reload.
    std::unordered_map<std::string, Any> Fields;

    // Verbatim YAML of the "Fields" map as read from disk, kept ONLY when the
    // script class could not be resolved at load (module not built / class
    // renamed), so a subsequent save re-emits the authored values instead of
    // silently dropping them. Empty once the class resolves (Fields is then the
    // source of truth). Opaque string to avoid coupling this component to yaml-cpp.
    std::string RawFieldsYaml;

    ScriptableEntity* Instance = nullptr;

    ScriptComponent() = default;
    explicit ScriptComponent(std::string scriptClass): ScriptClass(std::move(scriptClass)) {}
};

}