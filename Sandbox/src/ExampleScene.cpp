//
// Created by Ruben on 2026/06/29.
//

#include "ExampleScene.h"

#include <bgfx/bgfx.h>

struct PosColorVertex
{
    float    x, y, z;
    uint32_t abgr;
    float    u, v;

    static const bgfx::VertexLayout* Layout()
    {
        static const bgfx::VertexLayout layout = [] {
            bgfx::VertexLayout l;
            l.begin()
             .add(bgfx::Attrib::Position,  3, bgfx::AttribType::Float)
             .add(bgfx::Attrib::Color0,    4, bgfx::AttribType::Uint8, true)
             .add(bgfx::Attrib::TexCoord0, 2, bgfx::AttribType::Float)
             .end();
            return l;
        }();
        return &layout;
    }
};

static PosColorVertex s_CubeVerts[] =
{
    // +Z
    {-1,  1,  1, 0xffffffff, 0, 0}, { 1,  1,  1, 0xffffffff, 1, 0},
    {-1, -1,  1, 0xffffffff, 0, 1}, { 1, -1,  1, 0xffffffff, 1, 1},
    // -Z
    {-1,  1, -1, 0xffffffff, 1, 0}, { 1,  1, -1, 0xffffffff, 0, 0},
    {-1, -1, -1, 0xffffffff, 1, 1}, { 1, -1, -1, 0xffffffff, 0, 1},
    // -X
    {-1,  1,  1, 0xffffffff, 1, 0}, {-1, -1,  1, 0xffffffff, 1, 1},
    {-1,  1, -1, 0xffffffff, 0, 0}, {-1, -1, -1, 0xffffffff, 0, 1},
    // +X
    { 1,  1,  1, 0xffffffff, 0, 0}, { 1, -1,  1, 0xffffffff, 0, 1},
    { 1,  1, -1, 0xffffffff, 1, 0}, { 1, -1, -1, 0xffffffff, 1, 1},
    // +Y
    {-1,  1,  1, 0xffffffff, 0, 1}, { 1,  1,  1, 0xffffffff, 1, 1},
    {-1,  1, -1, 0xffffffff, 0, 0}, { 1,  1, -1, 0xffffffff, 1, 0},
    // -Y
    {-1, -1,  1, 0xffffffff, 0, 0}, { 1, -1,  1, 0xffffffff, 1, 0},
    {-1, -1, -1, 0xffffffff, 0, 1}, { 1, -1, -1, 0xffffffff, 1, 1},
};

static const uint16_t s_CubeIndices[] =
{
     2,  1,  0,   2,  3,  1,  // +Z
     5,  6,  4,   7,  6,  5,  // -Z
    10,  9,  8,  11,  9, 10,  // -X
    13, 14, 12,  13, 15, 14,  // +X
    17, 18, 16,  17, 19, 18,  // +Y
    22, 21, 20,  23, 21, 22,  // -Y
};

void ExampleScene::OnLoaded()
{
    Scene::OnLoaded();

    // ── Resources ────────────────────────────────────────────────────────────
    u32 texel = 0xffff00ff;
    m_Texture  = Seraph::Texture2D::Create("DemoTex", &texel, 1, 1);

    m_Material = Seraph::Ref<Seraph::Material>::Create(
        Seraph::ShaderManager::Get("simple"));
    m_Material->AddProperty<Seraph::ColorProperty>(
        "s_color", glm::vec4(0.0f, 0.0f, 1.0f, 1.0f));
    m_Material->AddProperty<Seraph::TextureProperty>(
        "s_texColor", m_Texture, 0);

    m_Mesh = Seraph::Ref<Seraph::Mesh>::Create(m_Material);
    m_Mesh->SetVertexLayout<PosColorVertex>();
    m_Mesh->SetVertexData(s_CubeVerts,   sizeof(s_CubeVerts));
    m_Mesh->SetIndexData (s_CubeIndices, sizeof(s_CubeIndices));

    // ── Entities ─────────────────────────────────────────────────────────────
    m_CameraEntity = CreateEntity("Camera");
    m_CameraEntity.Transform().Translation = { 0.0f, 0.0f, 10.0f };
    auto& cc = m_CameraEntity.AddComponent<Seraph::CameraComponent>();
    cc.Camera.SetPerspective(60.0f, 0.01f, 1000.0f);
    cc.Camera.SetViewId(1); // Scene view — matches EditorLayer::k_SceneViewId.
    cc.IsPrimary = true;

    m_CubeEntity = CreateEntity("Cube");
    m_CubeEntity.AddComponent<Seraph::MeshComponent>(m_Mesh);
}

void ExampleScene::OnUpdate(f64 dt)
{
    Scene::OnUpdate(dt);

    auto [mx, my] = Seraph::Input::GetMousePosition();
    const glm::vec2 mouse{ mx, my };
    const glm::vec2 delta = (mouse - m_LastMousePos) * 0.002f;
    m_LastMousePos = mouse;

    const bool captured = Seraph::Input::GetCursorMode() == Seraph::CursorMode::Captured;

    // Click to capture; Escape to release.
    if (!captured && Seraph::Input::IsMouseButtonDown(Seraph::Mouse::Left))
        Seraph::Input::SetCursorMode(Seraph::CursorMode::Captured);

    if (captured && Seraph::Input::IsKeyDown(Seraph::Key::Escape))
        Seraph::Input::SetCursorMode(Seraph::CursorMode::Normal);

    if (captured)
    {
        auto& t = m_CameraEntity.Transform();

        m_CameraYaw   -= delta.x * 0.3f;
        m_CameraPitch -= delta.y * 0.3f;
        m_CameraPitch  = glm::clamp(m_CameraPitch,
            -glm::half_pi<float>() + 0.01f,
             glm::half_pi<float>() - 0.01f);
        t.SetRotationEuler({ m_CameraPitch, m_CameraYaw, 0.0f });

        const float speed = static_cast<float>(dt) * 5.0f;
        if (Seraph::Input::IsKeyDown(Seraph::Key::W)) t.Translation += t.Forward() * speed;
        if (Seraph::Input::IsKeyDown(Seraph::Key::S)) t.Translation -= t.Forward() * speed;
        if (Seraph::Input::IsKeyDown(Seraph::Key::A)) t.Translation -= t.Right()   * speed;
        if (Seraph::Input::IsKeyDown(Seraph::Key::D)) t.Translation += t.Right()   * speed;
        if (Seraph::Input::IsKeyDown(Seraph::Key::E)) t.Translation.y += speed;
        if (Seraph::Input::IsKeyDown(Seraph::Key::Q)) t.Translation.y -= speed;
    }
}