//
// Created by Ruben on 2026/06/23.
//

#pragma once
#include "Layer.h"

namespace Seraph
{
class ImGuiLayer: public Layer
{
public:
    ImGuiLayer();
    ~ImGuiLayer() override;

    void Begin();
    void End();
};

}