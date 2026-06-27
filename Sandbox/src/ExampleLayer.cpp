//
// Example client layer implementation.
//

#include "ExampleLayer.h"

#include <SDL3/SDL_mouse.h>
#include <bx/math.h>
#include <imgui_internal.h>

namespace Sandbox
{

struct PosColorVertex
{
    float x;
    float y;
    float z;
    uint32_t abgr;
    float u;
    float v;

    static const bgfx::VertexLayout* Layout()
    {
        static const bgfx::VertexLayout layout = [] {
            bgfx::VertexLayout l;
            l.begin()
            .add(bgfx::Attrib::Position, 3, bgfx::AttribType::Float)
            .add(bgfx::Attrib::Color0,   4, bgfx::AttribType::Uint8, true)
            .add(bgfx::Attrib::TexCoord0, 2, bgfx::AttribType::Float)
            .end();
            return l;
        }();
        return &layout;
    }
};

PosColorVertex s_cubeVertices[] =
{
    // +Z (front)
    {-1.0f,  1.0f,  1.0f, 0xffffffff, 0.0f, 0.0f }, // 0
    { 1.0f,  1.0f,  1.0f, 0xffffffff, 1.0f, 0.0f }, // 1
    {-1.0f, -1.0f,  1.0f, 0xffffffff, 0.0f, 1.0f }, // 2
    { 1.0f, -1.0f,  1.0f, 0xffffffff, 1.0f, 1.0f }, // 3

    // -Z (back)
    {-1.0f,  1.0f, -1.0f, 0xffffffff, 1.0f, 0.0f }, // 4
    { 1.0f,  1.0f, -1.0f, 0xffffffff, 0.0f, 0.0f }, // 5
    {-1.0f, -1.0f, -1.0f, 0xffffffff, 1.0f, 1.0f }, // 6
    { 1.0f, -1.0f, -1.0f, 0xffffffff, 0.0f, 1.0f }, // 7

    // -X (left)
    {-1.0f,  1.0f,  1.0f, 0xffffffff, 1.0f, 0.0f }, // 8
    {-1.0f, -1.0f,  1.0f, 0xffffffff, 1.0f, 1.0f }, // 9
    {-1.0f,  1.0f, -1.0f, 0xffffffff, 0.0f, 0.0f }, // 10
    {-1.0f, -1.0f, -1.0f, 0xffffffff, 0.0f, 1.0f }, // 11

    // +X (right)
    { 1.0f,  1.0f,  1.0f, 0xffffffff, 0.0f, 0.0f }, // 12
    { 1.0f, -1.0f,  1.0f, 0xffffffff, 0.0f, 1.0f }, // 13
    { 1.0f,  1.0f, -1.0f, 0xffffffff, 1.0f, 0.0f }, // 14
    { 1.0f, -1.0f, -1.0f, 0xffffffff, 1.0f, 1.0f }, // 15

    // +Y (top)
    {-1.0f,  1.0f,  1.0f, 0xffffffff, 0.0f, 1.0f }, // 16
    { 1.0f,  1.0f,  1.0f, 0xffffffff, 1.0f, 1.0f }, // 17
    {-1.0f,  1.0f, -1.0f, 0xffffffff, 0.0f, 0.0f }, // 18
    { 1.0f,  1.0f, -1.0f, 0xffffffff, 1.0f, 0.0f }, // 19

    // -Y (bottom)
    {-1.0f, -1.0f,  1.0f, 0xffffffff, 0.0f, 0.0f }, // 20
    { 1.0f, -1.0f,  1.0f, 0xffffffff, 1.0f, 0.0f }, // 21
    {-1.0f, -1.0f, -1.0f, 0xffffffff, 0.0f, 1.0f }, // 22
    { 1.0f, -1.0f, -1.0f, 0xffffffff, 1.0f, 1.0f }, // 23
};

static const uint16_t s_cubeTriList[] =
{
    2,  1,  0,  2,  3,  1, // +Z
    5,  6,  4,  7,  6,  5, // -Z
   10,  9,  8, 11,  9, 10, // -X
   13, 14, 12, 13, 15, 14, // +X
   17, 18, 16, 17, 19, 18, // +Y
   22, 21, 20, 23, 21, 22, // -Y
};

ExampleLayer::ExampleLayer() : Layer("ExampleLayer")
{
}

void ExampleLayer::OnAttach()
{
    APP_INFO("ExampleLayer attached");
    const auto& app = Seraph::Application::Instance();
    auto& window = app.Window();

    // Camera entity
    m_CameraEntity = m_Scene.CreateEntity("Camera");
    auto& cc = m_CameraEntity.AddComponent<Seraph::CameraComponent>();
    cc.Camera = Seraph::Camera(
        60.0f,
        static_cast<float>(window.Width()) / static_cast<float>(window.Height()),
        0.01f, 1000.0f);
    cc.IsPrimary = true;

    // Resources (owned by the layer)
    u32 data[] = { 0xffff00ff };
    m_Texture = Seraph::Texture2D::Create("Test", data, 1, 1);

    m_Material = new Seraph::Material(Seraph::ShaderManager::Get("simple"));
    m_Material->AddProperty<Seraph::ColorProperty>(
        "s_color", glm::vec4(0.0f, 0.0f, 1.0f, 1.0f));
    m_Material->AddProperty<Seraph::TextureProperty>("s_texColor", *m_Texture, 0);

    m_Mesh = new Seraph::Mesh(*m_Material);
    m_Mesh->SetVertexLayout<PosColorVertex>();
    m_Mesh->SetVertexData(s_cubeVertices, sizeof(s_cubeVertices));
    m_Mesh->SetIndexData(s_cubeTriList, sizeof(s_cubeTriList));

    // Cube entity
    m_CubeEntity = m_Scene.CreateEntity("Cube");
    m_CubeEntity.AddComponent<Seraph::TransformComponent>();
    m_CubeEntity.AddComponent<Seraph::MeshComponent>(m_Mesh);
}

void ExampleLayer::OnDetach()
{
    delete m_Mesh;
    delete m_Material;
    delete m_Texture;
}

void ExampleLayer::OnUpdate(f64 deltaTime)
{
    auto& app = Seraph::Application::Instance();
    auto& cam = m_CameraEntity.Component<Seraph::CameraComponent>().Camera;

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
        cam.SetPosition(cam.Position() + moveDir * speed * static_cast<f32>(deltaTime));
    }

    m_Scene.UpdateInternal(deltaTime);
    m_Scene.RenderScene();
}

void ExampleLayer::OnEvent(Seraph::Event& e)
{
    Seraph::EventDispatcher dispatcher(e);
    dispatcher.Dispatch<Seraph::WindowResizeEvent>(SP_BIND_EVENT_FN(ExampleLayer::OnWindowResizeEvent));
    dispatcher.Dispatch<Seraph::KeyPressedEvent>(SP_BIND_EVENT_FN(ExampleLayer::OnKeyPressedEvent));
    dispatcher.Dispatch<Seraph::KeyReleasedEvent>(SP_BIND_EVENT_FN(ExampleLayer::OnKeyReleasedEvent));
    dispatcher.Dispatch<Seraph::MouseButtonReleasedEvent>(SP_BIND_EVENT_FN(ExampleLayer::OnMouseButtonReleasedEvent));
}

void ExampleLayer::OnImGuiRender()
{
    auto& cam = m_CameraEntity.Component<Seraph::CameraComponent>().Camera;
    ImGui::Begin("Settings");
    ImGui::Text("Camera: %s", Seraph::ToString(cam.Position()).c_str());
    ImGui::End();
}

bool ExampleLayer::OnWindowResizeEvent(Seraph::WindowResizeEvent& e)
{
    m_Scene.OnViewportResize(e.Width(), e.Height());
    return false;
}

bool ExampleLayer::OnKeyPressedEvent(Seraph::KeyPressedEvent& e)
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

bool ExampleLayer::OnKeyReleasedEvent(Seraph::KeyReleasedEvent& e)
{
    if (e.KeyCode() == SDLK_W) m_UpPressed    = false;
    if (e.KeyCode() == SDLK_S) m_DownPressed   = false;
    if (e.KeyCode() == SDLK_A) m_LeftPressed   = false;
    if (e.KeyCode() == SDLK_D) m_RightPressed  = false;
    if (e.KeyCode() == SDLK_F4)
        m_CameraEntity.Component<Seraph::CameraComponent>().Camera.LookAt(glm::vec3(0, 10, 0));

    return false;
}

bool ExampleLayer::OnMouseButtonReleasedEvent(Seraph::MouseButtonReleasedEvent& e)
{
    auto& app = Seraph::Application::Instance();

    if (e.MouseButton() == SDL_BUTTON_LEFT && !ImGui::GetIO().WantCaptureMouse && !app.IsMouseCaptured())
        app.SetMouseCaptured(true);

    return false;
}

} // namespace Sandbox