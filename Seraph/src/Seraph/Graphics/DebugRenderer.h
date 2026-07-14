//
// A small immediate-mode line/triangle debug renderer in the engine's bgfx
// style. Batches colored vertices into transient buffers each frame and submits
// them on a caller-provided view, piggybacking on that view's already-set
// view transform (SceneRenderer sets it for the whole frame). Reused for
// edit-time collider wireframes and the Jolt debug-draw bridge (Physics 9).
//
// Usage per frame:
//   DebugRenderer::Begin(k_SceneViewId, viewProj);
//   DebugRenderer::DrawBox(...); DebugRenderer::DrawLine(...);
//   DebugRenderer::Flush();
//   DebugRenderer::End();
//

#pragma once

#include "Seraph/Core/Base.h"

#include <bgfx/bgfx.h>
#include <glm/glm.hpp>

#include <cstdint>

namespace Seraph
{

// Position + packed-abgr color. Mirrors PrimitiveVertex minus the texcoord;
// matches the `debug` shader's a_position / a_color0 inputs.
struct DebugVertex
{
    float x, y, z;
    uint32_t abgr;

    static const bgfx::VertexLayout* Layout();
};

class DebugRenderer
{
public:
    static void Init();
    static void Shutdown();

    // Start a batch for `viewId`. `viewProj` is currently unused — the debug
    // pass piggybacks on the scene view's existing setViewTransform and draws at
    // model identity; the parameter is kept for a future dedicated debug view.
    static void Begin(uint16_t viewId, const glm::mat4& viewProj);
    static void End();

    // Submit the accumulated batches to the current view and clear them.
    static void Flush();

    // Depth mode for subsequent draws: false = occluded by scene geometry
    // (reversed-Z DEPTH_TEST_GREATER, no depth write); true = always on top.
    static void SetDepthTested(bool onTop);

    // --- packed-abgr color ---
    static void DrawLine(const glm::vec3& a, const glm::vec3& b, uint32_t abgr);
    static void DrawTriangle(
        const glm::vec3& a, const glm::vec3& b, const glm::vec3& c, uint32_t abgr);
    static void DrawBox(
        const glm::mat4& transform, const glm::vec3& halfExtents, uint32_t abgr);
    static void DrawSphere(
        const glm::vec3& center, float radius, uint32_t abgr, int segments = 24);
    static void DrawCapsule(
        const glm::mat4& transform, float radius, float halfHeight, uint32_t abgr,
        int segments = 16);

    // --- glm::vec4 color (encoded via EncodeColorRgba8) ---
    static void DrawLine(const glm::vec3& a, const glm::vec3& b, const glm::vec4& color);
    static void DrawTriangle(
        const glm::vec3& a, const glm::vec3& b, const glm::vec3& c, const glm::vec4& color);
    static void DrawBox(
        const glm::mat4& transform, const glm::vec3& halfExtents, const glm::vec4& color);
    static void DrawSphere(
        const glm::vec3& center, float radius, const glm::vec4& color, int segments = 24);
    static void DrawCapsule(
        const glm::mat4& transform, float radius, float halfHeight, const glm::vec4& color,
        int segments = 16);
};

} // namespace Seraph
