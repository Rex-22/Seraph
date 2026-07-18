//
// Created for Jolt Physics integration (Physics 2).
//

#include "PhysicsSettings.h"

#include "Seraph/Core/Assert.h"
#include "Seraph/Core/Log.h"

#include <iterator>
#include <string>

namespace Seraph
{

std::array<std::string, PhysicsLayerManager::LayerCount> PhysicsLayerManager::s_LayerNames;

void PhysicsLayerManager::InitDefaults()
{
    // A few useful named slots; the rest are "Layer N". Names are just editor
    // labels — collision is decided by each body's CollisionLayer/CollisionMask.
    static const char* const kDefaults[] = {
        "Default", "Static", "Player", "Enemy", "Environment", "Trigger",
        "Projectile", "Interactable",
    };

    for (u32 i = 0; i < LayerCount; ++i)
    {
        if (i < std::size(kDefaults))
            s_LayerNames[i] = kDefaults[i];
        else
            s_LayerNames[i] = "Layer " + std::to_string(i);
    }
}

const std::string& PhysicsLayerManager::GetLayerName(u32 index)
{
    SP_CORE_ASSERT(IsValidLayerIndex(index), "Invalid physics layer index");
    return s_LayerNames[index];
}

void PhysicsLayerManager::SetLayerName(u32 index, const std::string& name)
{
    if (!IsValidLayerIndex(index))
    {
        SP_CORE_WARN_TAG("Physics", "SetLayerName: invalid layer index {}", index);
        return;
    }
    s_LayerNames[index] = name;
}

} // namespace Seraph
