//
// Bridges Jolt's debug drawing onto the engine's DebugRenderer. Subclasses
// JPH::DebugRendererSimple (which decomposes everything to lines/triangles and
// self-initializes), overriding only DrawLine / DrawTriangle / DrawText3D.
//
// The whole file is guarded by JPH_DEBUG_RENDERER: the define arrives from
// linking the Jolt target and must match between the lib and every TU that
// includes <Jolt/Renderer/DebugRenderer.h> (vtable/ABI). When it is not set,
// this header is empty and nothing references the bridge.
//

#pragma once

#ifdef JPH_DEBUG_RENDERER

#include <Jolt/Jolt.h>
#include <Jolt/Renderer/DebugRendererSimple.h>

namespace Seraph
{

class JoltDebugRenderer final : public JPH::DebugRendererSimple
{
public:
    JoltDebugRenderer() = default;

    void DrawLine(JPH::RVec3Arg from, JPH::RVec3Arg to, JPH::ColorArg color) override;
    void DrawTriangle(
        JPH::RVec3Arg v1, JPH::RVec3Arg v2, JPH::RVec3Arg v3, JPH::ColorArg color,
        ECastShadow castShadow) override;
    void DrawText3D(
        JPH::RVec3Arg position, const JPH::string_view& str,
        JPH::ColorArg color, float height) override;
};

} // namespace Seraph

#endif // JPH_DEBUG_RENDERER
