//
// Created by ruben (Render 7 — lighting bring-up).
//

#pragma once

#include "Seraph/Core/Base.h"
#include "Seraph/Reflection/Annotations.h"

#include <glm/glm.hpp>

namespace Seraph
{

// An omnidirectional light at the entity's world position. Range is the distance
// at which the light's contribution reaches zero (used for smooth windowed
// attenuation); the transform's translation is the light position.
struct SCLASS() PointLightComponent
{
    SPROPERTY(settings.color = true)
    glm::vec3 Color = { 1.0f, 1.0f, 1.0f };

    SPROPERTY(settings.display = "Intensity", settings.min = 0.0f)
    float Intensity = 1.0f;

    SPROPERTY(settings.min = 0.0f)
    float Range = 10.0f;

    PointLightComponent() = default;
    PointLightComponent(const PointLightComponent&) = default;
};

} // namespace Seraph
