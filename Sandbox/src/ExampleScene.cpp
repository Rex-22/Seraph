//
// Created by Ruben on 2026/06/29.
//

#include "ExampleScene.h"

#include <SDL3/SDL_mouse.h>
#include <imgui.h>

void ExampleScene::OnLoaded()
{
    Scene::OnLoaded();

    const auto& app = Seraph::Application::Instance();
    auto& window = app.Window();

    m_CameraEntity = CreateEntity("Camera");
    m_CameraEntity.Transform().Translation = { 0.0f, 0.0f, 10.0f };
    auto& cc = m_CameraEntity.AddComponent<Seraph::CameraComponent>();
    cc.Camera.SetPerspective(60.0f, 0.01f, 1000.0f);
    cc.ProjectionType = Seraph::CameraComponent::Type::Perspective;
    cc.IsPrimary = true;

    // Cube entity
    m_CubeEntity = CreateEntity("Cube");
    m_CubeEntity.AddComponent<Seraph::MeshComponent>(m_Mesh);
}

bool ExampleScene::OnKeyPressedEvent(Seraph::KeyPressedEvent& e)
{
    auto& app = Seraph::Application::Instance();

    if (e.KeyCode() == SDLK_W) m_UpPressed    = true;
    if (e.KeyCode() == SDLK_S) m_DownPressed   = true;
    if (e.KeyCode() == SDLK_A) m_LeftPressed   = true;
    if (e.KeyCode() == SDLK_D) m_RightPressed  = true;
    if (e.KeyCode() == SDLK_ESCAPE && app.IsMouseCaptured())
        app.SetMouseCaptured(false);

    return false;
}

bool ExampleScene::OnKeyReleasedEvent(Seraph::KeyReleasedEvent& e)
{
    if (e.KeyCode() == SDLK_W) m_UpPressed    = false;
    if (e.KeyCode() == SDLK_S) m_DownPressed   = false;
    if (e.KeyCode() == SDLK_A) m_LeftPressed   = false;
    if (e.KeyCode() == SDLK_D) m_RightPressed  = false;

    return false;
}

bool ExampleScene::OnMouseButtonReleasedEvent(Seraph::MouseButtonReleasedEvent& e)
{
    auto& app = Seraph::Application::Instance();

    if (e.MouseButton() == SDL_BUTTON_LEFT && !ImGui::GetIO().WantCaptureMouse && !app.IsMouseCaptured())
        app.SetMouseCaptured(true);

    return false;
}

void ExampleScene::OnUpdate(f64 dt)
{
    Scene::OnUpdate(dt);

    auto& app = Seraph::Application::Instance();
    auto& tc = m_CameraEntity.Transform();

    if (app.IsMouseCaptured()) {
        f32 delta_x, delta_y;
        SDL_GetRelativeMouseState(&delta_x, &delta_y);
        glm::vec3 euler = tc.GetRotationEuler();
        euler.x = glm::clamp(euler.x + (-delta_y * m_RotScale), glm::radians(-89.0f), glm::radians(89.0f));
        euler.y += -delta_x * m_RotScale;
        tc.SetRotationEuler(euler);
    }

    const f32 speed = 10;
    glm::vec3 forward = tc.Forward();
    glm::vec3 right   = tc.Right();
    glm::vec3 moveDir{0.0f};

    if (m_UpPressed)    moveDir += forward;
    if (m_DownPressed)  moveDir -= forward;
    if (m_LeftPressed)  moveDir -= right;
    if (m_RightPressed) moveDir += right;

    const float lengthSq = glm::dot(moveDir, moveDir);
    if (lengthSq > 0.0f) {
        moveDir *= 1.0f / bx::sqrt(lengthSq);
        tc.Translation += moveDir * speed * static_cast<f32>(dt);
    }

}
void ExampleScene::OnEvent(Seraph::Event& e)
{
    Scene::OnEvent(e);

    Seraph::EventDispatcher dispatcher(e);
    dispatcher.Dispatch<Seraph::KeyPressedEvent>(SP_BIND_EVENT_FN(ExampleScene::OnKeyPressedEvent));
    dispatcher.Dispatch<Seraph::KeyReleasedEvent>(SP_BIND_EVENT_FN(ExampleScene::OnKeyReleasedEvent));
    dispatcher.Dispatch<Seraph::MouseButtonReleasedEvent>(SP_BIND_EVENT_FN(ExampleScene::OnMouseButtonReleasedEvent));

}