//
// Created by Ruben on 2026/07/02.
//

#pragma once
#include "Base.h"

namespace Seraph
{
class WindowResizeEvent;
class Scene;
class Event;

class World
{
public:
    World();
    virtual ~World() = default;

    void OnCreate();
    void OnDestroy();
    void OnUpdate(f64 dt);
    void OnImGuiDraw();
    void OnEvent(Event& e);

    bool OnWindowResizeEvent(WindowResizeEvent& e);

    void LoadScene(Scene* scene);

private:
    Scene* m_ActiveScene = nullptr;
};

} // namespace Seraph
