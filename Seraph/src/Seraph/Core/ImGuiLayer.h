//
// Created by Ruben on 2026/06/23.
//

#pragma once
#include "Layer.h"
#include "Memory.h"

namespace Seraph
{
class ImGuiLayer: public Layer
{
public:
    static ImGuiLayer* Create() { return snew ImGuiLayer(); }

    ImGuiLayer();
    ~ImGuiLayer() override;

    void Begin();
    void End();
};

}
