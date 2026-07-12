//
// Created by Ruben on 2026/06/23.
//

#include "LayerStack.h"
#include "Layer.h"

namespace Seraph
{

LayerStack::~LayerStack()
{
    Shutdown();
}

void LayerStack::PushLayer(const Ref<Layer>& layer)
{
    m_Layers.emplace(m_Layers.begin() + m_LayerInsertIndex, layer);
    m_LayerInsertIndex++;
}

void LayerStack::PushOverlay(const Ref<Layer>& overlay)
{
    m_Layers.emplace_back(overlay);
}

void LayerStack::PopLayer(Ref<Layer> layer)
{
    auto it = std::find(m_Layers.begin(), m_Layers.begin() + m_LayerInsertIndex, layer);
    if (it != m_Layers.begin() + m_LayerInsertIndex) {
        layer->OnDetach();
        m_Layers.erase(it);
        m_LayerInsertIndex--;
    }
}

void LayerStack::PopOverlay(Ref<Layer> overlay)
{
    auto it = std::find(m_Layers.begin() + m_LayerInsertIndex, m_Layers.end(), overlay);
    if (it != m_Layers.end()) {
        overlay->OnDetach();
        m_Layers.erase(it);
    }
}
void LayerStack::Shutdown()
{
    for (Ref<Layer> layer: m_Layers) {
        layer->OnDetach();
    }
    m_Layers.clear();
    m_LayerInsertIndex = 0;
}

} // namespace Core
