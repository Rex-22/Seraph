//
// Created by ruben on 2026/07/11.
//

#pragma once

#include "Mesh.h"
#include "Seraph/Core/Ref.h"

#include <bgfx/bgfx.h>
#include <cstdint>
#include <glm/glm.hpp>

namespace Seraph
{

// Standard vertex layout used by all MeshFactory primitives.
// Position (3f) + Color (rgba u8) + TexCoord0 (2f) + Normal (3f) + Tangent (4f).
// The tangent's w component carries handedness (±1) so the shader can
// reconstruct the bitangent as cross(normal, tangent.xyz) * tangent.w.
struct PrimitiveVertex
{
    float x, y, z;
    uint32_t abgr;
    float u, v;
    float nx, ny, nz;
    float tx, ty, tz, tw;

    static const bgfx::VertexLayout* Layout()
    {
        static const bgfx::VertexLayout s_layout = []
        {
            bgfx::VertexLayout l;
            l.begin()
             .add(bgfx::Attrib::Position,  3, bgfx::AttribType::Float)
             .add(bgfx::Attrib::Color0,    4, bgfx::AttribType::Uint8, true)
             .add(bgfx::Attrib::TexCoord0, 2, bgfx::AttribType::Float)
             .add(bgfx::Attrib::Normal,    3, bgfx::AttribType::Float)
             .add(bgfx::Attrib::Tangent,   4, bgfx::AttribType::Float)
             .end();
            return l;
        }();
        return &s_layout;
    }
};

// Parameters for each procedural primitive. Defaults reproduce the original
// unit-ish primitives (half-extents of 1), so `CreateCube()` is unchanged.
struct CubeParams
{
    glm::vec3 HalfExtents{1.0f}; // half-size along each axis
};

struct PlaneParams
{
    glm::vec2 HalfExtents{1.0f}; // half-size along X and Z
};

// Generates procedural primitive geometry as a ready-to-render Mesh. Pure: it
// does NOT touch the asset system, so results can be used directly (e.g. runtime
// procedural meshes) or handed to EditorAssetManager::SaveAssetAs to be
// materialized as a shared .smesh asset. Future primitives (sphere, cylinder,
// ...) follow the same params-in / Ref<Mesh>-out shape.
class MeshFactory
{
public:
    static Ref<Mesh> CreateCube(const CubeParams& params = {});
    static Ref<Mesh> CreatePlane(const PlaneParams& params = {});
};

} // namespace Seraph
