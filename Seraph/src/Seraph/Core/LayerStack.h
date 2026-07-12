//
// Created by Ruben on 2026/06/23.
//

#pragma once

#include "Base.h"
#include "Ref.h"

#include <vector>

namespace Seraph
{

class Layer;

class LayerStack
{
public:
    LayerStack()  = default;
    ~LayerStack();

    void PushLayer(const Ref<Layer>& layer);
    void PushOverlay(const Ref<Layer>& overlay);
    void PopLayer(Ref<Layer> layer);
    void PopOverlay(Ref<Layer> overlay);
    void Shutdown();

    std::vector<Ref<Layer>>::iterator begin() { return m_Layers.begin(); }
    std::vector<Ref<Layer>>::iterator end() { return m_Layers.end(); }
    std::vector<Ref<Layer>>::reverse_iterator rbegin() { return m_Layers.rbegin(); }
    std::vector<Ref<Layer>>::reverse_iterator rend() { return m_Layers.rend(); }
private:
    std::vector<Ref<Layer>> m_Layers;
    u32 m_LayerInsertIndex = 0;

};
}
