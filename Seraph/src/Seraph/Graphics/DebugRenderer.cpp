//
// Created for Jolt Physics integration (Physics 8).
//

#include "DebugRenderer.h"

#include "Seraph/Asset/AssetManager.h"
#include "Seraph/Core/Core.h"
#include "Seraph/Core/Log.h"
#include "Seraph/Graphics/ShaderAsset.h"
#include "Seraph/Graphics/ShaderManager.h"

#include <cmath>
#include <cstring>
#include <vector>

namespace Seraph
{

const bgfx::VertexLayout* DebugVertex::Layout()
{
    static const bgfx::VertexLayout s_layout = []
    {
        bgfx::VertexLayout l;
        l.begin()
            .add(bgfx::Attrib::Position, 3, bgfx::AttribType::Float)
            .add(bgfx::Attrib::Color0, 4, bgfx::AttribType::Uint8, true)
            .end();
        return l;
    }();
    return &s_layout;
}

namespace
{

constexpr float k_Pi = 3.14159265358979323846f;

std::vector<DebugVertex> s_Lines;
std::vector<DebugVertex> s_Tris;
uint16_t s_ViewId = 0;
bool s_OnTop = false;
bool s_WarnedOverflow = false;
bool s_WarnedNoProgram = false;

DebugVertex MakeVertex(const glm::vec3& p, uint32_t abgr)
{
    return DebugVertex{ p.x, p.y, p.z, abgr };
}

// Resolve the `debug` program from the live ShaderAsset every flush. Cheap (a
// name->handle map lookup) and robust across project reloads, where a cached
// handle would dangle. Null until an AssetManager is active (a project is open).
bgfx::ProgramHandle ResolveProgram()
{
    const AssetHandle handle = ShaderManager::GetHandle("debug");
    if (Ref<ShaderAsset> shader = AssetManager::GetAsset<ShaderAsset>(handle))
        return shader->Program();
    return BGFX_INVALID_HANDLE;
}

// Submit one transient batch, clamping to the available transient space (warns
// once on truncation). `verticesPerPrim` keeps the drawn count a whole multiple
// of the primitive size. Returns nothing; leaves the caller to clear its store.
void SubmitBatch(
    std::vector<DebugVertex>& store, bgfx::ProgramHandle program, uint64_t state,
    uint32_t verticesPerPrim, const char* what)
{
    if (store.empty())
        return;

    const bgfx::VertexLayout& layout = *DebugVertex::Layout();
    uint32_t count = static_cast<uint32_t>(store.size());

    const uint32_t avail = bgfx::getAvailTransientVertexBuffer(count, layout);
    if (avail < count)
    {
        if (!s_WarnedOverflow)
        {
            SP_CORE_WARN_TAG("DebugRenderer",
                "transient buffer overflow ({}): {} of {} vertices drawn",
                what, avail, count);
            s_WarnedOverflow = true;
        }
        count = avail;
    }

    count -= count % verticesPerPrim;
    if (count < verticesPerPrim)
        return;

    bgfx::TransientVertexBuffer tvb;
    bgfx::allocTransientVertexBuffer(&tvb, count, layout);
    std::memcpy(tvb.data, store.data(), static_cast<size_t>(count) * sizeof(DebugVertex));
    bgfx::setVertexBuffer(0, &tvb, 0, count);
    bgfx::setState(state);
    bgfx::submit(s_ViewId, program);
}

} // namespace

void DebugRenderer::Init()
{
    // The program is resolved lazily on first Flush — the AssetManager isn't
    // active yet at Renderer::Init time. Just reset batch state here.
    s_Lines.clear();
    s_Tris.clear();
    s_ViewId = 0;
    s_OnTop = false;
    s_WarnedOverflow = false;
    s_WarnedNoProgram = false;
}

void DebugRenderer::Shutdown()
{
    // The bgfx program is owned by its ShaderAsset (released via
    // AssetManager::Shutdown before bgfx::shutdown), so nothing to destroy here.
    s_Lines.clear();
    s_Tris.clear();
    s_Lines.shrink_to_fit();
    s_Tris.shrink_to_fit();
}

void DebugRenderer::Begin(uint16_t viewId, const glm::mat4& /*viewProj*/)
{
    s_ViewId = viewId;
    s_OnTop = false;
    s_Lines.clear();
    s_Tris.clear();
}

void DebugRenderer::End()
{
    // Nothing to tear down; Flush already cleared the batches. Kept for symmetry
    // with Begin and to leave room for future per-batch view state.
}

void DebugRenderer::SetDepthTested(bool onTop)
{
    s_OnTop = onTop;
}

void DebugRenderer::Flush()
{
    if (s_Lines.empty() && s_Tris.empty())
        return;

    const bgfx::ProgramHandle program = ResolveProgram();
    if (!bgfx::isValid(program))
    {
        if (!s_WarnedNoProgram)
        {
            SP_CORE_WARN_TAG("DebugRenderer",
                "'debug' shader program unavailable; debug draw disabled");
            s_WarnedNoProgram = true;
        }
        s_Lines.clear();
        s_Tris.clear();
        return;
    }

    // Reversed-Z: pass fragments nearer than what's in the depth buffer, but
    // never write depth (debug overlays shouldn't occlude scene geometry).
    // "On top" ignores depth entirely.
    const uint64_t depthState =
        s_OnTop ? BGFX_STATE_DEPTH_TEST_ALWAYS : BGFX_STATE_DEPTH_TEST_GREATER;

    SubmitBatch(s_Lines, program,
        BGFX_STATE_WRITE_RGB | BGFX_STATE_PT_LINES | depthState, 2, "lines");
    SubmitBatch(s_Tris, program,
        BGFX_STATE_WRITE_RGB | depthState, 3, "triangles");

    s_Lines.clear();
    s_Tris.clear();
}

// ---------------------------------------------------------------------------
// packed-abgr primitives
// ---------------------------------------------------------------------------

void DebugRenderer::DrawLine(const glm::vec3& a, const glm::vec3& b, uint32_t abgr)
{
    s_Lines.push_back(MakeVertex(a, abgr));
    s_Lines.push_back(MakeVertex(b, abgr));
}

void DebugRenderer::DrawTriangle(
    const glm::vec3& a, const glm::vec3& b, const glm::vec3& c, uint32_t abgr)
{
    s_Tris.push_back(MakeVertex(a, abgr));
    s_Tris.push_back(MakeVertex(b, abgr));
    s_Tris.push_back(MakeVertex(c, abgr));
}

void DebugRenderer::DrawBox(
    const glm::mat4& transform, const glm::vec3& he, uint32_t abgr)
{
    // 8 corners, indexed by (sx<<2 | sy<<1 | sz) with sign bit 0 => -, 1 => +.
    glm::vec3 c[8];
    int i = 0;
    for (int sx = -1; sx <= 1; sx += 2)
        for (int sy = -1; sy <= 1; sy += 2)
            for (int sz = -1; sz <= 1; sz += 2)
                c[i++] = glm::vec3(transform *
                    glm::vec4(sx * he.x, sy * he.y, sz * he.z, 1.0f));

    // 12 edges: pairs of corners differing in exactly one axis bit.
    static const int edges[12][2] = {
        {0,1},{2,3},{4,5},{6,7}, // differ in sz
        {0,2},{1,3},{4,6},{5,7}, // differ in sy
        {0,4},{1,5},{2,6},{3,7}, // differ in sx
    };
    for (const auto& e : edges)
        DrawLine(c[e[0]], c[e[1]], abgr);
}

void DebugRenderer::DrawSphere(
    const glm::vec3& center, float radius, uint32_t abgr, int segments)
{
    if (segments < 3)
        segments = 3;

    const auto ring = [&](const glm::vec3& axisA, const glm::vec3& axisB)
    {
        glm::vec3 prev = center + radius * axisA;
        for (int s = 1; s <= segments; ++s)
        {
            const float t = 2.0f * k_Pi * static_cast<float>(s) / static_cast<float>(segments);
            const glm::vec3 p = center + radius * (std::cos(t) * axisA + std::sin(t) * axisB);
            DrawLine(prev, p, abgr);
            prev = p;
        }
    };

    ring({1, 0, 0}, {0, 1, 0}); // XY
    ring({1, 0, 0}, {0, 0, 1}); // XZ
    ring({0, 1, 0}, {0, 0, 1}); // YZ
}

void DebugRenderer::DrawCapsule(
    const glm::mat4& transform, float radius, float halfHeight, uint32_t abgr,
    int segments)
{
    if (segments < 3)
        segments = 3;

    const auto tp = [&](const glm::vec3& local) {
        return glm::vec3(transform * glm::vec4(local, 1.0f));
    };

    // Cylinder rings at both ends (Y-up).
    const auto ringXZ = [&](float y)
    {
        glm::vec3 prev = tp({radius, y, 0.0f});
        for (int s = 1; s <= segments; ++s)
        {
            const float t = 2.0f * k_Pi * static_cast<float>(s) / static_cast<float>(segments);
            const glm::vec3 p = tp({radius * std::cos(t), y, radius * std::sin(t)});
            DrawLine(prev, p, abgr);
            prev = p;
        }
    };
    ringXZ(halfHeight);
    ringXZ(-halfHeight);

    // 4 vertical connectors between the two rings.
    for (int k = 0; k < 4; ++k)
    {
        const float t = 0.5f * k_Pi * static_cast<float>(k);
        const float cx = radius * std::cos(t);
        const float cz = radius * std::sin(t);
        DrawLine(tp({cx, -halfHeight, cz}), tp({cx, halfHeight, cz}), abgr);
    }

    // Two half-circle cap arcs per hemisphere (XY and ZY planes).
    const auto arc = [&](bool zyPlane, float baseY, float dir)
    {
        glm::vec3 prev;
        for (int s = 0; s <= segments; ++s)
        {
            const float a = k_Pi * static_cast<float>(s) / static_cast<float>(segments);
            const float h = baseY + dir * radius * std::sin(a);
            const float r = radius * std::cos(a);
            const glm::vec3 local = zyPlane ? glm::vec3(0.0f, h, r) : glm::vec3(r, h, 0.0f);
            const glm::vec3 p = tp(local);
            if (s > 0)
                DrawLine(prev, p, abgr);
            prev = p;
        }
    };
    arc(false, halfHeight, 1.0f);
    arc(true, halfHeight, 1.0f);
    arc(false, -halfHeight, -1.0f);
    arc(true, -halfHeight, -1.0f);
}

// ---------------------------------------------------------------------------
// glm::vec4 color overloads
// ---------------------------------------------------------------------------

void DebugRenderer::DrawLine(const glm::vec3& a, const glm::vec3& b, const glm::vec4& color)
{
    DrawLine(a, b, EncodeColorRgba8(color.r, color.g, color.b, color.a));
}

void DebugRenderer::DrawTriangle(
    const glm::vec3& a, const glm::vec3& b, const glm::vec3& c, const glm::vec4& color)
{
    DrawTriangle(a, b, c, EncodeColorRgba8(color.r, color.g, color.b, color.a));
}

void DebugRenderer::DrawBox(
    const glm::mat4& transform, const glm::vec3& halfExtents, const glm::vec4& color)
{
    DrawBox(transform, halfExtents, EncodeColorRgba8(color.r, color.g, color.b, color.a));
}

void DebugRenderer::DrawSphere(
    const glm::vec3& center, float radius, const glm::vec4& color, int segments)
{
    DrawSphere(center, radius, EncodeColorRgba8(color.r, color.g, color.b, color.a), segments);
}

void DebugRenderer::DrawCapsule(
    const glm::mat4& transform, float radius, float halfHeight, const glm::vec4& color,
    int segments)
{
    DrawCapsule(transform, radius, halfHeight,
        EncodeColorRgba8(color.r, color.g, color.b, color.a), segments);
}

} // namespace Seraph
