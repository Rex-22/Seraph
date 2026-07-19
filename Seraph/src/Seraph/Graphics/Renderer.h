//
// Created by Ruben on 2025/05/03.
//

#pragma once
#include "Seraph/Asset/AssetHandle.h"
#include "Seraph/Core/Base.h"
#include "bgfx/bgfx.h"

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

    // Image-based-lighting environment bound for the frame's mesh submits. The
    // cubes are the scene's prefiltered radiance (mipped) + irradiance; `brdfLut`
    // is Renderer::BrdfLut(). SubmitMesh binds these per-submesh (a material's
    // BGFX_DISCARD_ALL clears bindings each submesh), so the PBR shader gets
    // image-based ambient. `radianceMips` scales roughness -> radiance LOD;
    // `rotationYaw`/`intensity` match the skybox. All handles must be valid.
    struct EnvironmentBinding
    {
        bgfx::TextureHandle radiance   = BGFX_INVALID_HANDLE;
        bgfx::TextureHandle irradiance = BGFX_INVALID_HANDLE;
        bgfx::TextureHandle brdfLut    = BGFX_INVALID_HANDLE;
        float intensity    = 1.0f;
        float rotationYaw  = 0.0f;
        float radianceMips = 1.0f;
    };

    // Set (or clear) the active IBL environment for subsequent SubmitMesh calls.
    // Call once per frame before the mesh loop (SceneRenderer does this from the
    // scene's SceneEnvironment). Cleared state binds neutral fallbacks and tells
    // the shader to fall back to the flat ambient term.
    static void SetEnvironment(const EnvironmentBinding& env);
    static void ClearEnvironment();

    // Submit a single fullscreen triangle on `viewId` with `program`. The caller
    // binds any source textures/uniforms first (like a material). Geometry is a
    // transient oversized clip-space triangle; texture V is flipped for
    // originBottomLeft backbuffers. No-op if the program is invalid or there is no
    // transient vertex space this frame.
    static void DrawFullscreen(
        uint16_t viewId, bgfx::ProgramHandle program,
        uint64_t state = BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A);

    // Resolve an HDR (linear) scene color texture to the target currently bound to
    // `viewId` via the `tonemap` pass: exposure, tone-mapping operator, and gamma
    // encode. The caller sets the view's framebuffer + rect first. `op`: 0 None,
    // 1 Reinhard, 2 ACES.
    static void TonemapResolve(
        uint16_t viewId, bgfx::TextureHandle hdrColor, float exposure, int op);

    // Draw the environment cube as the scene background on `viewId`, filling only
    // pixels no geometry covered. A fullscreen pass reconstructs the per-pixel
    // world ray from `invViewProj` + `cameraPos`; the sample direction is yaw-
    // rotated by `rotationYaw` and scaled by `intensity`. Rasterized at the far
    // plane with a GEQUAL depth test (reversed-Z far = 0.0). `mipLod` picks the
    // radiance mip (0 = sharpest). Must run on the scene view after opaque
    // geometry, while its depth buffer is still bound. No-op if the cube or the
    // `skybox` program is invalid.
    static void DrawSkybox(
        uint16_t viewId, bgfx::TextureHandle cube, const glm::mat4& invViewProj,
        const glm::vec3& cameraPos, float intensity, float rotationYaw,
        float mipLod = 0.0f);

    // The split-sum BRDF integration LUT (RG16F, x=NdotV, y=roughness ->
    // F0 scale + bias). Baked once on first request into a persistent texture via
    // a fullscreen GGX-integration pass on ViewId::EnvBake; subsequent calls
    // return the cached handle. The bake is submitted in the calling frame, so
    // the LUT is only sampleable from the NEXT frame on (bgfx runs views in id
    // order — EnvBake is after the scene view). Returns BGFX_INVALID_HANDLE if the
    // `brdf_lut` program is unavailable. Bind it per-view for IBL specular.
    static bgfx::TextureHandle BrdfLut();

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