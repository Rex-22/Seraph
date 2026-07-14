//
// Created for Jolt Physics integration (Physics 2).
//

#include "PhysicsSettings.h"

#include "Seraph/Core/Assert.h"
#include "Seraph/Core/Log.h"

namespace Seraph
{

std::vector<PhysicsLayer> PhysicsLayerManager::s_Layers;

void PhysicsLayerManager::InitDefaults()
{
    s_Layers.clear();
    const u32 staticLayer = AddLayer("Static", /*collidesWithAll=*/true);
    const u32 movingLayer = AddLayer("Moving", /*collidesWithAll=*/true);

    // Static bodies never move, so two static bodies never need to collide.
    SetLayerCollision(staticLayer, staticLayer, false);
    (void)movingLayer;
}

u32 PhysicsLayerManager::AddLayer(const std::string& name, bool collidesWithAll)
{
    const auto layerID = static_cast<u32>(s_Layers.size());

    PhysicsLayer layer;
    layer.LayerID = layerID;
    layer.Name = name;
    layer.BitValue = 1u << layerID;
    layer.CollidesWith = 0;
    layer.CollidesWithSelf = true;
    s_Layers.push_back(layer);

    if (collidesWithAll)
    {
        // New layer collides with every existing layer, and they with it.
        for (auto& other : s_Layers)
        {
            other.CollidesWith |= layer.BitValue;
            s_Layers[layerID].CollidesWith |= other.BitValue;
        }
    }

    return layerID;
}

void PhysicsLayerManager::SetLayerCollision(u32 layerA, u32 layerB, bool shouldCollide)
{
    if (!IsLayerValid(layerA) || !IsLayerValid(layerB))
    {
        SP_CORE_WARN_TAG("Physics", "SetLayerCollision: invalid layer {} or {}", layerA, layerB);
        return;
    }

    if (layerA == layerB)
    {
        s_Layers[layerA].CollidesWithSelf = shouldCollide;
        return;
    }

    if (shouldCollide)
    {
        s_Layers[layerA].CollidesWith |= s_Layers[layerB].BitValue;
        s_Layers[layerB].CollidesWith |= s_Layers[layerA].BitValue;
    }
    else
    {
        s_Layers[layerA].CollidesWith &= ~s_Layers[layerB].BitValue;
        s_Layers[layerB].CollidesWith &= ~s_Layers[layerA].BitValue;
    }
}

bool PhysicsLayerManager::ShouldCollide(u32 layerA, u32 layerB)
{
    if (!IsLayerValid(layerA) || !IsLayerValid(layerB))
        return false;

    if (layerA == layerB)
        return s_Layers[layerA].CollidesWithSelf;

    return (s_Layers[layerA].CollidesWith & s_Layers[layerB].BitValue) != 0;
}

const PhysicsLayer& PhysicsLayerManager::GetLayer(u32 layerID)
{
    SP_CORE_ASSERT(IsLayerValid(layerID), "Invalid physics layer id");
    return s_Layers[layerID];
}

const std::vector<PhysicsLayer>& PhysicsLayerManager::GetLayers()
{
    return s_Layers;
}

u32 PhysicsLayerManager::GetLayerCount()
{
    return static_cast<u32>(s_Layers.size());
}

bool PhysicsLayerManager::IsLayerValid(u32 layerID)
{
    return layerID < s_Layers.size();
}

} // namespace Seraph
