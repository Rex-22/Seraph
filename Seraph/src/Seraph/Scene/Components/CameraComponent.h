//
// Created by ruben on 2026/06/27.
//

#pragma once

#include "Seraph/Graphics/SceneCamera.h"
#include "Seraph/Reflection/Annotations.h"

namespace Seraph
{

// Camera is flattened into the serialized Camera block (SceneCamera's reflected
// fields emit at the block level); IsPrimary follows. See Reflection v3.5.
struct SCLASS() CameraComponent
{
    SPROPERTY(serialize.flatten = true)
    Seraph::SceneCamera Camera;

    SPROPERTY(settings.display = "Primary")
    bool IsPrimary = true;

    operator SceneCamera& () { return Camera; }
    operator const SceneCamera& () const { return Camera; }
};

}