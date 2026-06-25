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
    ~ImGuiLayer() override = default;

    void Begin();
    void End();
};

}