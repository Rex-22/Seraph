//
// Created by ruben on 2026/06/27.
//

#pragma once

#include "Seraph/Graphics/SceneCamera.h"

namespace Seraph
{

struct CameraComponent
{
    Seraph::SceneCamera Camera;
    bool IsPrimary = true;

    operator SceneCamera& () { return Camera; }
    operator const SceneCamera& () const { return Camera; }
};

}