//
// Example client layer implementation.
//

#include "ExampleLayer.h"
#include <Seraph.h>

#include <SDL3/SDL_mouse.h>
#include <bgfx/embedded_shader.h>
#include <bx/math.h>
#include <imgui_internal.h>
#include <array>

#define SHADER_NAME vs_simple
#include "Seraph/Graphics/ShaderIncluder.h"
#define SHADER_NAME fs_simple
#include "Seraph/Graphics/ShaderIncluder.h"


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

const std::array<bgfx::EmbeddedShader, 3> k_EmbeddedShaders = {{
    BGFX_EMBEDDED_SHADER(vs_simple), BGFX_EMBEDDED_SHADER(fs_simple),
    BGFX_EMBEDDED_SHADER_END() //
}};

ExampleLayer::ExampleLayer() : Layer("ExampleLayer")
{
}

void ExampleLayer::OnAttach()
{
    CORE_INFO("ExampleLayer attached");
    const auto& app = Seraph::Application::Instance();
    auto& window = app.Window();
    m_Camera = Seraph::Camera(
        60.0f,
        static_cast<float>(window.Width()) /
            static_cast<float>(window.Height()),
        0.01f, 1000.0f);

    Seraph::Renderer::SetCamera(&m_Camera);

    // m_Texture = Seraph::Texture2D::Create("textures/test_texture.png");
    u32 data[] = {
        0xffff00ff,
    };
    m_Texture = Seraph::Texture2D::Create("Test", data, 1, 1);

    const auto type = bgfx::getRendererType();
    const auto vsSimple =
        bgfx::createEmbeddedShader(k_EmbeddedShaders.data(), type, "vs_simple");
    const auto fsSimple =
        bgfx::createEmbeddedShader(k_EmbeddedShaders.data(), type, "fs_simple");
    const auto program = bgfx::createProgram(vsSimple, fsSimple, true);

    m_Material = new Seraph::Material(program);
    m_Material->AddProperty<Seraph::ColorProperty>(
       "s_color", glm::vec4(0.0f, 0.0f, 1.0f, 1.0f));
    m_Material->AddProperty<Seraph::TextureProperty>("s_texColor", *m_Texture, 0);

    m_Cube = new Seraph::Mesh(*m_Material);
    m_Cube->SetVertexLayout<PosColorVertex>();
    m_Cube->SetVertexData(s_cubeVertices, sizeof(s_cubeVertices));
    m_Cube->SetIndexData(s_cubeTriList, sizeof(s_cubeTriList));
}

void ExampleLayer::OnDetach()
{
    delete m_Cube;
    delete m_Material;
    delete m_Texture;
}

void ExampleLayer::OnUpdate(f64 deltaTime)
{
    auto& app = Seraph::Application::Instance();

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
        move_direction += forward;
    }
    if (m_DownPressed) {
        move_direction -= forward;
    }

    if (m_LeftPressed) {
        move_direction -= right;
    }
    if (m_RightPressed) {
        move_direction += right;
    }

    float length_sq = glm::dot(move_direction, move_direction);
    if (length_sq > 0.0f) {
        f32 inv_length = 1.0f / bx::sqrt(length_sq);
        move_direction = move_direction * inv_length;
        auto pos = m_Camera.Position();
        pos += move_direction * speed * static_cast<f32>(deltaTime);
        m_Camera.SetPosition(pos);
    }

    Seraph::Renderer::Begin(0);
    Seraph::Renderer::Clear(m_ClearColor);

    Seraph::Transform transform;
    Seraph::Renderer::SubmitMesh(*m_Cube, transform);

    Seraph::Renderer::End();
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
    ImGui::Begin("Settings");
    ImGui::Text("Camera: %s", Seraph::ToString(m_Camera.Position()).c_str());
    ImGui::End();
}

bool ExampleLayer::OnWindowResizeEvent(Seraph::WindowResizeEvent& e)
{
    m_Camera.SetAspectRatio(
            static_cast<float>(e.Width()) /
            static_cast<float>(e.Height()));
    return false;
}

bool ExampleLayer::OnKeyPressedEvent(Seraph::KeyPressedEvent& e)
{
    auto& app = Seraph::Application::Instance();

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

bool ExampleLayer::OnKeyReleasedEvent(Seraph::KeyReleasedEvent& e)
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

bool ExampleLayer::OnMouseButtonReleasedEvent(Seraph::MouseButtonReleasedEvent& e)
{
    auto& app = Seraph::Application::Instance();

    if (e.MouseButton() == SDL_BUTTON_LEFT && !ImGui::GetIO().WantCaptureMouse && !app.IsMouseCaptured()) {
        app.SetMouseCaptured(true);
    }

    return false;
}

} // namespace Sandbox