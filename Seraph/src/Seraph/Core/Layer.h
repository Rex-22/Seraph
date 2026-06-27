//
// Created by Ruben on 2026/06/23.
//

#pragma once
#include "Base.h"

namespace Seraph
{

class Event;

class Layer
{
public:
    Layer(std::string name = "Layer");
    virtual ~Layer() = default;

    virtual void OnAttach() {}
    virtual void OnDetach() {}
    virtual void OnUpdate([[maybe_unused]] f64 deltaTime) {}
    virtual void OnEvent([[maybe_unused]] Event& e) {}
    virtual void OnImGuiRender() {}

    [[nodiscard]] const std::string& Name() const { return m_DebugName; }
private:
    std::string m_DebugName;
};

}