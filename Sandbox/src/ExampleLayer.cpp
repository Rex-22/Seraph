//
// Example client layer implementation.
//

#include "ExampleLayer.h"
#include "ExampleScene.h"

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

ExampleLayer::ExampleLayer() : Layer("ExampleLayer"), m_Scene(nullptr)
{
}

void ExampleLayer::OnAttach()
{
    SP_INFO_TAG("ExampleLayer", "ExampleLayer attached");
    // Resources (owned by the layer)
    u32 data[] = { 0xffff00ff };
    m_Texture = Seraph::Texture2D::Create("Test", data, 1, 1);

    m_Material = Seraph::Ref<Seraph::Material>::Create(Seraph::ShaderManager::Get("simple"));
    m_Material->AddProperty<Seraph::ColorProperty>(
        "s_color", glm::vec4(0.0f, 0.0f, 1.0f, 1.0f));
    m_Material->AddProperty<Seraph::TextureProperty>("s_texColor", m_Texture, 0);

    m_Mesh = Seraph::Ref<Seraph::Mesh>::Create(*m_Material);
    m_Mesh->SetVertexLayout<PosColorVertex>();
    m_Mesh->SetVertexData(s_cubeVertices, sizeof(s_cubeVertices));
    m_Mesh->SetIndexData(s_cubeTriList, sizeof(s_cubeTriList));


    m_Scene = Seraph::Ref<ExampleScene>::Create(m_Mesh, m_Material);
    m_Scene->OnLoaded();
    m_EntityBrowser.SetScene(m_Scene);
    m_EntityInspector.SetDefaultMaterial(m_Material);
    m_Gizmo.SetScene(m_Scene);

    Seraph::SceneRendererSettings sceneSettings {
        glm::vec3(0.6f, 0.5f, 0.4f),
    };
    m_SceneRenderer = Seraph::Ref<Seraph::SceneRenderer>::Create(m_Scene, sceneSettings);
}

void ExampleLayer::OnDetach()
{
}

void ExampleLayer::OnUpdate(f64 deltaTime)
{
	auto& app = Seraph::Application::Instance();
    auto [width, height] = app.Window().Size();

    m_Scene->SetViewportBounds(0, 0, width, height);

    m_Scene->OnUpdate(deltaTime);

    m_Scene->OnRenderRuntime(m_SceneRenderer);
}

void ExampleLayer::OnEvent(Seraph::Event& e)
{
    m_Scene->OnEvent(e);
}

void ExampleLayer::OnImGuiRender()
{
    m_EntityBrowser.OnImGuiRender();

    Seraph::Entity selected = m_EntityBrowser.GetSelectedEntity();
    m_EntityInspector.SetSelectedEntity(selected);
    m_EntityInspector.OnImGuiRender();

    m_Gizmo.SetSelectedEntity(selected);
    m_Gizmo.OnImGuiRender();
}

} // namespace Sandbox
