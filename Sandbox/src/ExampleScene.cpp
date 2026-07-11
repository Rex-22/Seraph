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
    auto& cc = m_CameraEntity.AddComponent<Seraph::CameraComponent>();
    cc.Camera = Seraph::Camera(
        60.0f,
        static_cast<float>(window.Width()) / static_cast<float>(window.Height()),
        0.01f, 1000.0f);
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
    if (e.KeyCode() == SDLK_F4)
        m_CameraEntity.GetComponent<Seraph::CameraComponent>().Camera.LookAt(glm::vec3(0, 10, 0));

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
    auto& cam = m_CameraEntity.GetComponent<Seraph::CameraComponent>().Camera;

    if (app.IsMouseCaptured()) {
        f32 delta_x, delta_y;
        SDL_GetRelativeMouseState(&delta_x, &delta_y);
        cam.RotatePitch(-delta_y * m_RotScale);
        cam.RotateYaw(-delta_x * m_RotScale);
    }

    const f32 speed = 10;
    glm::vec3 moveDir{0.0f};

    if (m_UpPressed)    moveDir += cam.Forward();
    if (m_DownPressed)  moveDir -= cam.Forward();
    if (m_LeftPressed)  moveDir -= cam.Right();
    if (m_RightPressed) moveDir += cam.Right();

    const float lengthSq = glm::dot(moveDir, moveDir);
    if (lengthSq > 0.0f) {
        moveDir *= 1.0f / bx::sqrt(lengthSq);
        cam.SetPosition(cam.Position() + moveDir * speed * static_cast<f32>(dt));
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