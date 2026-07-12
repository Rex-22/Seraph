//
// Created by ruben on 2026/06/27.
//

#pragma once

#include "Seraph/Graphics/SceneCamera.h"

namespace Seraph
{

struct CameraComponent
{
    enum class Type { None = -1, Perspective, Orthographic };
    Type ProjectionType;
    Seraph::SceneCamera Camera;
    bool IsPrimary = true;

    operator SceneCamera& () { return Camera; }
    operator const SceneCamera& () const { return Camera; }
};

}