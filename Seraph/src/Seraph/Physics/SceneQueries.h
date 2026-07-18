//
// Scene-query input/output structs (raycast for this pass). Jolt-free.
//

#pragma once

#include "Seraph/Core/Base.h"
#include "Seraph/Scene/Entity.h"

#include <glm/glm.hpp>

namespace Seraph
{

// Input for PhysicsScene::CastRay. Direction need not be normalized.
struct RayCastInfo
{
    glm::vec3 Origin = { 0.0f, 0.0f, 0.0f };
    glm::vec3 Direction = { 0.0f, 0.0f, -1.0f };
    float MaxDistance = 1.0e6f;

    // Bitmask of collision layers to test against: bit N set => layer N is hit.
    // Default = every layer.
    u32 LayerMask = 0xFFFFFFFFu;

    // When false (default) the ray passes through trigger/sensor bodies and only
    // reports solid geometry — a query shouldn't be blocked by a trigger volume.
    bool HitTriggers = false;
};

// Closest-hit result. HitEntity is empty (operator bool == false) on a miss.
struct SceneQueryHit
{
    Entity HitEntity;
    glm::vec3 Position = { 0.0f, 0.0f, 0.0f };
    glm::vec3 Normal = { 0.0f, 0.0f, 0.0f };
    float Distance = 0.0f;
};

} // namespace Seraph
