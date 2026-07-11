//
// Created by ruben on 2026/07/11.
//

#pragma once

#include "Mesh.h"
#include "Seraph/Core/Ref.h"

#include <bgfx/bgfx.h>
#include <cstdint>

namespace Seraph
{

// Standard vertex layout used by all MeshFactory primitives.
// Matches: Position (3f) + Color (rgba u8) + TexCoord0 (2f)
struct PrimitiveVertex
{
    float x, y, z;
    uint32_t abgr;
    float u, v;

    static const bgfx::VertexLayout* Layout()
    {
        static const bgfx::VertexLayout s_layout = []
        {
            bgfx::VertexLayout l;
            l.begin()
             .add(bgfx::Attrib::Position,  3, bgfx::AttribType::Float)
             .add(bgfx::Attrib::Color0,    4, bgfx::AttribType::Uint8, true)
             .add(bgfx::Attrib::TexCoord0, 2, bgfx::AttribType::Float)
             .end();
            return l;
        }();
        return &s_layout;
    }
};

class MeshFactory
{
public:
    static Ref<Mesh> CreateCube(const Material& material);
    static Ref<Mesh> CreatePlane(const Material& material);
};

} // namespace Seraph
