//
// Created by Jolt Physics integration (component authored in Physics 4; UI &
// serialization in Physics 7).
//

#pragma once

#include "Seraph/Core/Base.h"
#include "Seraph/Physics/PhysicsTypes.h"
#include "Seraph/Reflection/Annotations.h"

#include <glm/glm.hpp>

namespace Seraph
{

// Marks an entity as a simulated rigid body. Requires a collider component to
// actually create a Jolt body (a body needs a shape). Reflected via SHT — the
// inspector reads settings.* attrs, the scene serializer reads serialize.* attrs
// (settings.flags = 2u is SettingFlag_Hidden; the Layer combo is drawn bespoke).
struct SCLASS() RigidBodyComponent
{
    SPROPERTY(settings.display = "Body Type", serialize.key = "BodyType")
    BodyType Type = BodyType::Static;

    SPROPERTY(settings.flags = 2u) // Hidden from inspector; still serialized
    u32 LayerID = 0; // index into PhysicsLayerManager (0 = Static by default)

    SPROPERTY(settings.min = 0.0f)
    float Mass = 1.0f;

    SPROPERTY(settings.display = "Linear Drag", settings.min = 0.0f)
    float LinearDrag = 0.01f;

    SPROPERTY(settings.display = "Angular Drag", settings.min = 0.0f)
    float AngularDrag = 0.05f;

    SPROPERTY(settings.display = "Gravity Factor")
    float GravityFactor = 1.0f; // 0 = ignore gravity

    SPROPERTY(settings.display = "Init Lin Vel")
    glm::vec3 InitialLinearVelocity = { 0.0f, 0.0f, 0.0f };

    SPROPERTY(settings.display = "Init Ang Vel")
    glm::vec3 InitialAngularVelocity = { 0.0f, 0.0f, 0.0f };

    RigidBodyComponent() = default;
    RigidBodyComponent(const RigidBodyComponent&) = default;
};

} // namespace Seraph
