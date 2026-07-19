//
// Created by ruben (Render 7 — lighting bring-up).
//

#pragma once

#include "Seraph/Core/Base.h"
#include "Seraph/Reflection/Annotations.h"

#include <glm/glm.hpp>

namespace Seraph
{

// An infinitely-distant light (the sun). The light's direction is the entity's
// forward axis (-Z of its world transform), so it is oriented by the transform,
// not stored here; only the radiometric parameters live on the component.
struct SCLASS() DirectionalLightComponent
{
    SPROPERTY(settings.color = true)
    glm::vec3 Color = { 1.0f, 1.0f, 1.0f };

    SPROPERTY(settings.display = "Intensity", settings.min = 0.0f)
    float Intensity = 1.0f;

    DirectionalLightComponent() = default;
    DirectionalLightComponent(const DirectionalLightComponent&) = default;
};

} // namespace Seraph
