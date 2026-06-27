//
// Created by ruben on 2026/06/27.
//

#pragma once

#include "Seraph/Graphics/Camera.h"

namespace Seraph
{

struct CameraComponent
{
    Seraph::Camera Camera;
    bool IsPrimary = true;
};

}