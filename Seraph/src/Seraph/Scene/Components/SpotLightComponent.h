//
// Created by ruben (Render 7 — lighting bring-up).
//

#pragma once

#include "Seraph/Core/Base.h"
#include "Seraph/Reflection/Annotations.h"

#include <glm/glm.hpp>

namespace Seraph
{

// A cone light at the entity's world position, aimed along its forward axis
// (-Z). Inside InnerAngle the cone is at full intensity; between Inner and Outer
// it falls off smoothly to zero. Both are half-angles in degrees.
struct SCLASS() SpotLightComponent
{
    SPROPERTY(settings.color = true)
    glm::vec3 Color = { 1.0f, 1.0f, 1.0f };

    SPROPERTY(settings.display = "Intensity", settings.min = 0.0f)
    float Intensity = 1.0f;

    SPROPERTY(settings.min = 0.0f)
    float Range = 10.0f;

    SPROPERTY(settings.display = "Inner Angle", settings.min = 0.0f, settings.max = 89.0f)
    float InnerAngle = 20.0f; // half-angle, degrees

    SPROPERTY(settings.display = "Outer Angle", settings.min = 0.0f, settings.max = 90.0f)
    float OuterAngle = 30.0f; // half-angle, degrees

    SpotLightComponent() = default;
    SpotLightComponent(const SpotLightComponent&) = default;
};

} // namespace Seraph
