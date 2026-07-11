//
// Created by Ruben on 2025/05/03.
//

#pragma once
#include "Seraph/Core/Base.h"
#include "bgfx/defines.h"
#include "glm/vec3.hpp"

#include <cstdint>

namespace Seraph
{
class Transform;
}
namespace Seraph
{
class Camera;
class Mesh;
class Material;

struct Renderer
{
    static void Init();
    static void Cleanup();

    static void SubmitMesh(const Mesh& mesh, Transform& transform);

    static void Begin(uint16_t viewId);
    static void End();

    static void Clear(glm::vec3 clearColor, uint16_t flags = BGFX_CLEAR_COLOR | BGFX_CLEAR_DEPTH);
    static void OnWindowResize(u32 width, u32 height);

    static void FlushFrame();
};

} // namespace Graphics