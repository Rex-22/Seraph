//
// Created by Ruben on 2026/06/23.
//

#pragma once
#include "Layer.h"

namespace Core
{
class ImGuiLayer: public Layer
{
public:
    ImGuiLayer();

    void Begin();
    void End();
};

}