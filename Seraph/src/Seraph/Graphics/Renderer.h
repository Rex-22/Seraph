//
// Created by Ruben on 2025/05/03.
//

#pragma once
#include "Seraph/Asset/AssetHandle.h"
#include "Seraph/Core/Base.h"
#include "bgfx/defines.h"

#include <cstdint>
#include <vector>

namespace Seraph
{
class Camera;
class Mesh;
class Material;

struct Renderer
{
    static void Init();
    static void Cleanup();

    // Draw a mesh. `materialOverrides` is indexed by material slot; a valid
    // handle at a slot wins over the mesh's baked slot default, which wins over
    // the engine default material.
    static void SubmitMesh(
        const Mesh& mesh, const glm::mat4& transform = glm::mat4(1.0f),
        const std::vector<AssetHandle>& materialOverrides = {});

    static void Begin(uint16_t viewId);
    static void End();

    static void Clear(glm::vec3 clearColor, uint16_t flags = BGFX_CLEAR_COLOR | BGFX_CLEAR_DEPTH);
    static void SetBackBufferSize(u32 width, u32 height);

    static void FlushFrame();

    // bgfx frame counter (last value returned by bgfx::frame in FlushFrame).
    // Used to poll async GPU->CPU readbacks: bgfx::readTexture reports the frame
    // at which its copy is complete, so a caller waits until FrameNumber() has
    // advanced to (or past) that value. 0 until the first frame is flushed.
    static u32 FrameNumber();
};

} // namespace Graphics