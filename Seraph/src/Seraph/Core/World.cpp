//
// Created by Ruben on 2026/07/02.
//

#include "World.h"
#include "Seraph/Core/Base.h"
#include "Seraph/Events/WindowEvent.h"
#include "Seraph/Scene/Scene.h"

namespace Seraph {

World::World()
{
}

void World::OnCreate()
{
}

void World::OnDestroy()
{
    if (m_ActiveScene != nullptr) {
        m_ActiveScene->OnDestroy();
    }
}

void World::OnUpdate(f64 dt)
{
    if (m_ActiveScene != nullptr) {
        m_ActiveScene->OnUpdate(dt);
    }
}

void World::OnImGuiDraw()
{
}

void World::OnEvent(Event& e)
{
    EventDispatcher dispatcher(e);

    dispatcher.Dispatch<WindowResizeEvent>(SP_BIND_EVENT_FN(World::OnWindowResizeEvent));

    if (m_ActiveScene != nullptr) {
        m_ActiveScene->OnEvent(e);
    }
}

void World::LoadScene(Scene* scene)
{
    if (m_ActiveScene == nullptr) {
        m_ActiveScene = scene;
        m_ActiveScene->OnLoaded();
        return;
    }

    m_ActiveScene->OnDestroy();
    delete m_ActiveScene;
    scene->OnLoaded();
    m_ActiveScene = scene;
}

bool World::OnWindowResizeEvent(WindowResizeEvent& e)
{
    if (m_ActiveScene != nullptr) {
        m_ActiveScene->OnViewportResize(e.Width(), e.Height());
    }
}

} // Seraph