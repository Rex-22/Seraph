//
// Created by Jolt Physics integration (component authored in Physics 4; UI &
// serialization in Physics 7).
//

#pragma once

#include "Seraph/Core/Base.h"
#include "Seraph/Physics/PhysicsTypes.h"

#include <glm/glm.hpp>

namespace Seraph
{

// Marks an entity as a simulated rigid body. Requires a collider component to
// actually create a Jolt body (a body needs a shape).
struct RigidBodyComponent
{
    BodyType Type = BodyType::Static;
    u32 LayerID = 0; // index into PhysicsLayerManager (0 = Static by default)

    float Mass = 1.0f;
    float LinearDrag = 0.01f;
    float AngularDrag = 0.05f;
    float GravityFactor = 1.0f; // 0 = ignore gravity

    glm::vec3 InitialLinearVelocity = { 0.0f, 0.0f, 0.0f };
    glm::vec3 InitialAngularVelocity = { 0.0f, 0.0f, 0.0f };

    RigidBodyComponent() = default;
    RigidBodyComponent(const RigidBodyComponent&) = default;
};

} // namespace Seraph
