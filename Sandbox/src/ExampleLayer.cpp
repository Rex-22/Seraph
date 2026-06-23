//
// Example client layer implementation.
//

#include "ExampleLayer.h"

#include "SDL3/SDL_mouse.h"
#include "bx/math.h"
#include "core/Application.h"
#include "imgui_internal.h"
#include "platform/Window.h"

#include <Seraph.h>

namespace Sandbox
{

ExampleLayer::ExampleLayer() : Layer("ExampleLayer")
{
}

void ExampleLayer::OnAttach()
{
    CORE_INFO("ExampleLayer attached");
    const auto& app = Core::Application::Instance();
    auto& window = app.Window();
    m_Camera = Graphics::Camera(
        60.0f,
        static_cast<float>(window.Width()) /
            static_cast<float>(window.Height()),
        0.01f, 1000.0f);
    Graphics::Renderer::SetCamera(&m_Camera);
}

void ExampleLayer::OnDetach()
{

}

void ExampleLayer::OnUpdate(f64 deltaTime)
{
    Core::Application& app = Core::Application::Instance();

    if (app.IsMouseCaptured()) {
        f32 delta_x, delta_y;
        SDL_GetRelativeMouseState(&delta_x, &delta_y);
        m_Camera.RotatePitch(-delta_y * m_RotScale);
        m_Camera.RotateYaw(-delta_x * m_RotScale);
    }
    f32 speed = 10;

    glm::vec3 forward = m_Camera.Forward();
    glm::vec3 right = m_Camera.Right();

    auto move_direction = glm::vec3(0.0f);
    if (m_UpPressed) {
        move_direction = move_direction + forward;
    }
    if (m_DownPressed) {
        move_direction = move_direction - forward;
    }

    if (m_LeftPressed) {
        move_direction = move_direction - right;
    }
    if (m_RightPressed) {
        move_direction = move_direction + right;
    }

    float length_sq = glm::dot(move_direction, move_direction);
    if (length_sq > 0.0f) {
        f32 inv_length = 1.0f / bx::sqrt(length_sq);
        move_direction = move_direction * inv_length;
        auto pos = m_Camera.Position();
        pos += move_direction * speed * static_cast<f32>(deltaTime);
        m_Camera.SetPosition(pos);
    }

    Graphics::Renderer::Begin(0);
    Graphics::Renderer::Clear(m_ClearColor);

    Graphics::Renderer::End();
}

void ExampleLayer::OnEvent(Event::Event& e)
{
    Event::EventDispatcher dispatcher(e);
    dispatcher.Dispatch<Event::WindowResizeEvent>(SP_BIND_EVENT_FN(ExampleLayer::OnWindowResizeEvent));
    dispatcher.Dispatch<Event::KeyPressedEvent>(SP_BIND_EVENT_FN(ExampleLayer::OnKeyPressedEvent));
    dispatcher.Dispatch<Event::KeyReleasedEvent>(SP_BIND_EVENT_FN(ExampleLayer::OnKeyReleasedEvent));
    dispatcher.Dispatch<Event::MouseButtonReleasedEvent>(SP_BIND_EVENT_FN(ExampleLayer::OnMouseButtonReleasedEvent));
}

void ExampleLayer::OnImGuiRender()
{
    ImGui::Begin("Settings");
    ImGui::End();
}

bool ExampleLayer::OnWindowResizeEvent(Event::WindowResizeEvent& e)
{
    m_Camera.SetAspectRatio(
            static_cast<float>(e.Width()) /
            static_cast<float>(e.Height()));
    return true;
}
bool ExampleLayer::OnKeyPressedEvent(Event::KeyPressedEvent& e)
{
    auto& app = Core::Application::Instance();

    if (e.KeyCode() == SDLK_W) {
        m_UpPressed = true;
    }
    if (e.KeyCode() == SDLK_S) {
        m_DownPressed = true;
    }
    if (e.KeyCode() == SDLK_A) {
        m_LeftPressed = true;
    }
    if (e.KeyCode() == SDLK_D) {
        m_RightPressed = true;
    }
    if (e.KeyCode() == SDLK_ESCAPE && app.IsMouseCaptured()) {
        app.SetMouseCaptured(false);
    }
    return false;
}

bool ExampleLayer::OnKeyReleasedEvent(Event::KeyReleasedEvent& e)
{
    if (e.KeyCode() == SDLK_W) {
        m_UpPressed = false;
    }
    if (e.KeyCode() == SDLK_S) {
        m_DownPressed = false;
    }
    if (e.KeyCode() == SDLK_A) {
        m_LeftPressed = false;
    }
    if (e.KeyCode() == SDLK_D) {
        m_RightPressed = false;
    }
    if (e.KeyCode() == SDLK_F4) {
        m_Camera.LookAt(glm::vec3(0, 10, 0));
    }

    return false;
}

bool ExampleLayer::OnMouseButtonReleasedEvent(Event::MouseButtonReleasedEvent& e)
{
    auto& app = Core::Application::Instance();

    if (e.MouseButton() == SDL_BUTTON_LEFT && !ImGui::GetIO().WantCaptureMouse && !app.IsMouseCaptured()) {
        app.SetMouseCaptured(true);
    }

    return false;
}

} // namespace Sandbox