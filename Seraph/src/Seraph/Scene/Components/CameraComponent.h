//
// Created by ruben on 2026/06/27.
//

#pragma once

#include "Seraph/Graphics/SceneCamera.h"

namespace Seraph
{

struct CameraComponent
{
    // Projection type is owned solely by SceneCamera (Camera.GetProjectionType()).
    // The former CameraComponent::Type/ProjectionType mirror was write-only dead
    // state and has been removed — single source of truth. See Reflection v3.5.
    Seraph::SceneCamera Camera;
    bool IsPrimary = true;

    operator SceneCamera& () { return Camera; }
    operator const SceneCamera& () const { return Camera; }
};

}