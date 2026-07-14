//
// Created for Jolt Physics integration (Physics 9).
//

#include "JoltDebugRenderer.h"

#ifdef JPH_DEBUG_RENDERER

#include "JoltUtils.h"

#include "Seraph/Core/Core.h"
#include "Seraph/Graphics/DebugRenderer.h"

namespace Seraph
{

namespace
{
    // JPH::Color is 8-bit RGBA (0..255). Route through EncodeColorRgba8 for a
    // portable abgr pack rather than assuming the little-endian mU32 layout.
    uint32_t ToAbgr(JPH::ColorArg c)
    {
        return EncodeColorRgba8(
            c.r / 255.0f, c.g / 255.0f, c.b / 255.0f, c.a / 255.0f);
    }
}

void JoltDebugRenderer::DrawLine(
    JPH::RVec3Arg from, JPH::RVec3Arg to, JPH::ColorArg color)
{
    // Fully qualified: unqualified `DebugRenderer` would resolve to the inherited
    // JPH::DebugRenderer injected-class-name, not the engine's renderer.
    Seraph::DebugRenderer::DrawLine(
        JoltUtils::FromJoltVector(from), JoltUtils::FromJoltVector(to), ToAbgr(color));
}

void JoltDebugRenderer::DrawTriangle(
    JPH::RVec3Arg v1, JPH::RVec3Arg v2, JPH::RVec3Arg v3, JPH::ColorArg color,
    ECastShadow /*castShadow*/)
{
    Seraph::DebugRenderer::DrawTriangle(
        JoltUtils::FromJoltVector(v1), JoltUtils::FromJoltVector(v2),
        JoltUtils::FromJoltVector(v3), ToAbgr(color));
}

void JoltDebugRenderer::DrawText3D(
    JPH::RVec3Arg /*position*/, const JPH::string_view& /*str*/,
    JPH::ColorArg /*color*/, float /*height*/)
{
    // Debug text is out of scope for the wireframe bridge.
}

} // namespace Seraph

#endif // JPH_DEBUG_RENDERER
