//
// Created by Ruben on 2025/05/03.
//

#include "Renderer.h"

#include "Platform/Window.h"
#include "Seraph/Asset/AssetManager.h"
#include "Seraph/Core/Application.h"
#include "Seraph/Core/Assert.h"
#include "Seraph/Core/Base.h"
#include "Seraph/Core/Core.h"
#include "Seraph/Graphics/Camera.h"
#include "Seraph/Graphics/Material/Material.h"
#include "Seraph/Graphics/Material/UniformCache.h"
#include "Seraph/Graphics/Mesh.h"
#include "Seraph/Graphics/RenderPass.h"
#include "Seraph/Graphics/ShaderAsset.h"
#include "Seraph/Graphics/ShaderManager.h"
#include "Seraph/Graphics/ViewId.h"

#include <glm/gtc/type_ptr.hpp>
#include <SDL3/SDL.h>
#include <bgfx/bgfx.h>
#include <bx/platform.h>
#include <bx/string.h>

#include <cstdarg>
#include <string_view>

namespace Seraph
{

namespace
{

// Routes bgfx's diagnostics into our spdlog-backed logger. bgfx may invoke
// these from the render thread, which is safe because the sinks are _mt.
class BgfxCallback final : public bgfx::CallbackI
{
public:
    ~BgfxCallback() override = default;

    void fatal(
        const char* filePath, uint16_t line, bgfx::Fatal::Enum code,
        const char* str) override
    {
        SP_CORE_ERROR_TAG("BGFX",
            "fatal [{}:{}] (code {}): {}", filePath, line,
            static_cast<int>(code), str);

        // bgfx considers everything but DebugCheck unrecoverable. Break into
        // the debugger here; promote to a hard abort if you want release
        // builds to stop rather than limp on with a broken context.
        if (code != bgfx::Fatal::DebugCheck) {
            SP_DEBUG_BREAK;
        }
    }

    void traceVargs(
        const char* /*filePath*/, uint16_t /*line*/, const char* format,
        va_list argList) override
    {
        char buffer[2048];

        // bgfx trace formats use bx's extended printf specifiers (e.g. %S for
        // bx::StringView), so format with bx::vsnprintf rather than the C
        // library's vsnprintf, which would misread %S as a wide string.
        va_list argListCopy;
        va_copy(argListCopy, argList);
        const int written = bx::vsnprintf(
            buffer, static_cast<int32_t>(sizeof(buffer)), format, argListCopy);
        va_end(argListCopy);

        if (written <= 0) {
            return;
        }

        // Clamp to what was actually written, then drop bgfx's trailing
        // newline so spdlog doesn't emit a blank line after each message.
        size_t length = static_cast<size_t>(written) < sizeof(buffer)
                            ? static_cast<size_t>(written)
                            : sizeof(buffer) - 1;
        while (length > 0 &&
               (buffer[length - 1] == '\n' || buffer[length - 1] == '\r')) {
            --length;
        }


        const std::string_view message(buffer, length);

        // The Metal backend checks Metal object lifetimes with Apple's
        // retainCount(), which Apple documents as unreliable (the driver and
        // autorelease pools hold internal references). bgfx only trusts it
        // when no GPU debugger is attached, so "RefCount is N (expected 0)" is
        // benign noise on macOS. Drop it.
        if (message.find("RefCount is ") != std::string_view::npos) {
            return;
        }

        // bgfx routes warnings through this same trace channel, prefixed with
        // "WARN ". Surface those at our warn level instead of burying them in
        // trace output.
        if (message.find("WARN ") != std::string_view::npos) {
            SP_CORE_WARN_TAG("BGFX", " {}", message);
        } else {
            SP_CORE_TRACE_TAG("BGFX", " {}", message);
        }
    }

    void profilerBegin(
        const char* /*name*/, uint32_t /*abgr*/, const char* /*filePath*/,
        uint16_t /*line*/) override
    {
    }
    void profilerBeginLiteral(
        const char* /*name*/, uint32_t /*abgr*/, const char* /*filePath*/,
        uint16_t /*line*/) override
    {
    }
    void profilerEnd() override {}

    // No shader/pipeline cache wired up yet.
    uint32_t cacheReadSize(uint64_t /*id*/) override { return 0; }
    bool cacheRead(uint64_t /*id*/, void* /*data*/, uint32_t /*size*/) override
    {
        return false;
    }
    void cacheWrite(
        uint64_t /*id*/, const void* /*data*/, uint32_t /*size*/) override
    {
    }

    void screenShot(
        const char* /*filePath*/, uint32_t /*width*/, uint32_t /*height*/,
        uint32_t /*pitch*/, bgfx::TextureFormat::Enum /*format*/,
        const void* /*data*/, uint32_t /*size*/, bool /*yflip*/) override
    {
    }

    void captureBegin(
        uint32_t /*width*/, uint32_t /*height*/, uint32_t /*pitch*/,
        bgfx::TextureFormat::Enum /*format*/, bool /*yflip*/) override
    {
    }
    void captureEnd() override {}
    void captureFrame(const void* /*data*/, uint32_t /*size*/) override {}
};

// Must outlive bgfx (bgfx::init stores the pointer until bgfx::shutdown).
BgfxCallback s_BgfxCallback;

} // namespace

struct RenderData
{
    u16 currentViewId;
    u32 windowWidth;
    u32 windowHeight;
    u32 resetFlags = BGFX_RESET_VSYNC;
    u32 frameNumber = 0; // last bgfx::frame() return; see Renderer::FrameNumber

    void EndFrame()
    {
        currentViewId = -1;
    }
};

static RenderData s_RenderData;

// Lazily-created tonemap uniforms (bgfx keys uniforms globally by name). Declared
// here so Cleanup can destroy them before bgfx::shutdown.
static bgfx::UniformHandle s_TonemapSampler = BGFX_INVALID_HANDLE; // s_hdr
static bgfx::UniformHandle s_TonemapParams  = BGFX_INVALID_HANDLE; // u_tonemapParams

// Lazily-created skybox uniforms (destroyed in Cleanup before bgfx::shutdown).
static bgfx::UniformHandle s_SkyCube        = BGFX_INVALID_HANDLE; // s_skyCube
static bgfx::UniformHandle s_SkyInvViewProj = BGFX_INVALID_HANDLE; // u_skyInvViewProj
static bgfx::UniformHandle s_SkyCameraPos   = BGFX_INVALID_HANDLE; // u_skyCameraPos
static bgfx::UniformHandle s_SkyParams      = BGFX_INVALID_HANDLE; // u_skyParams

// Split-sum BRDF LUT, baked once (Renderer::BrdfLut). We own both the texture
// and its framebuffer (createFrameBuffer with destroyTextures=false), so both
// are destroyed in Cleanup.
static bgfx::TextureHandle     s_BrdfLut   = BGFX_INVALID_HANDLE;
static bgfx::FrameBufferHandle s_BrdfLutFb = BGFX_INVALID_HANDLE;

void Renderer::Init()
{
    s_RenderData = {};

    // Signal to bgfx that we want multithreaded rendering
    bgfx::renderFrame();
    auto& window = Application::Instance().Window();

    bgfx::PlatformData pd{};
#if BX_PLATFORM_WINDOWS
    pd.nwh = SDL_GetPointerProperty(
        SDL_GetWindowProperties(window.Handle()),
        SDL_PROP_WINDOW_WIN32_HWND_POINTER, nullptr);
#elif BX_PLATFORM_OSX
    pd.nwh = SDL_GetPointerProperty(
        SDL_GetWindowProperties(window.Handle()),
        SDL_PROP_WINDOW_COCOA_WINDOW_POINTER, nullptr);
    pd.ndt = nullptr;
#elif BX_PLATFORM_LINUX
    if (SDL_strcmp(SDL_GetCurrentVideoDriver(), "x11") == 0) {
        pd.ndt = SDL_GetPointerProperty(
            SDL_GetWindowProperties(window.Handle()),
            SDL_PROP_WINDOW_X11_DISPLAY_POINTER, nullptr);
        pd.nwh = reinterpret_cast<void*>(SDL_GetNumberProperty(
            SDL_GetWindowProperties(window.Handle()),
            SDL_PROP_WINDOW_X11_WINDOW_NUMBER, 0));
    } else if (SDL_strcmp(SDL_GetCurrentVideoDriver(), "wayland") == 0) {
        pd.type = bgfx::NativeWindowHandleType::Wayland;
        pd.ndt = SDL_GetPointerProperty(
            SDL_GetWindowProperties(window.Handle()),
            SDL_PROP_WINDOW_WAYLAND_DISPLAY_POINTER, nullptr);
        pd.nwh = SDL_GetPointerProperty(
            SDL_GetWindowProperties(window.Handle()),
            SDL_PROP_WINDOW_WAYLAND_SURFACE_POINTER, nullptr);
    }
#endif

    bgfx::Init bgfx_init;
    bgfx_init.type = bgfx::RendererType::Count; // auto choose renderer
    bgfx_init.resolution.width = window.Width();
    bgfx_init.resolution.height = window.Height();
    bgfx_init.resolution.reset = s_RenderData.resetFlags;
    bgfx_init.platformData = pd;
    bgfx_init.callback = &s_BgfxCallback; // route bgfx logging to our logger
    bgfx::init(bgfx_init);

    s_RenderData.windowWidth  = (u32)window.Width();
    s_RenderData.windowHeight = (u32)window.Height();

    // View 0 is reserved for the backbuffer clear. No scene geometry goes here;
    // scene geometry uses view 1 (offscreen framebuffer via RenderTarget).
    bgfx::setViewClear(0, BGFX_CLEAR_COLOR, 0x1A1C23FF, 0.0f, 0);
    bgfx::setViewRect(0, 0, 0, (u16)s_RenderData.windowWidth, (u16)s_RenderData.windowHeight);

    SP_CORE_INFO_TAG("Renderer", "Backend: {}", bgfx::getRendererName(bgfx::getRendererType()));
}

void Renderer::Cleanup()
{
    if (bgfx::isValid(s_TonemapSampler))
    {
        bgfx::destroy(s_TonemapSampler);
        s_TonemapSampler = BGFX_INVALID_HANDLE;
    }
    if (bgfx::isValid(s_TonemapParams))
    {
        bgfx::destroy(s_TonemapParams);
        s_TonemapParams = BGFX_INVALID_HANDLE;
    }
    for (bgfx::UniformHandle* h :
         { &s_SkyCube, &s_SkyInvViewProj, &s_SkyCameraPos, &s_SkyParams })
    {
        if (bgfx::isValid(*h))
        {
            bgfx::destroy(*h);
            *h = BGFX_INVALID_HANDLE;
        }
    }
    if (bgfx::isValid(s_BrdfLutFb))
    {
        bgfx::destroy(s_BrdfLutFb);
        s_BrdfLutFb = BGFX_INVALID_HANDLE;
    }
    if (bgfx::isValid(s_BrdfLut))
    {
        bgfx::destroy(s_BrdfLut);
        s_BrdfLut = BGFX_INVALID_HANDLE;
    }

    ShaderManager::Shutdown();
    UniformCache::Shutdown();
    bgfx::shutdown();
}

void Renderer::SubmitMesh(
    const Mesh& mesh, const glm::mat4& transform,
    const std::vector<AssetHandle>& materialOverrides)
{
    const bgfx::VertexBufferHandle vb = mesh.VertexBuffer();
    const bgfx::IndexBufferHandle ib = mesh.IndexBuffer();
    if (!bgfx::isValid(vb) || !bgfx::isValid(ib))
        return;

    Ref<MaterialAsset> engineDefault = Material::GetDefault();
    const u16 viewId = s_RenderData.currentViewId;

    // Resolve a material slot: per-entity override -> mesh baked default ->
    // shared engine default.
    const auto resolveSlot = [&](u32 slot) -> Ref<MaterialAsset> {
        if (slot < materialOverrides.size()) {
            if (Ref<MaterialAsset> m = MaterialAsset::Get(materialOverrides[slot]))
                return m;
        }
        if (Ref<MaterialAsset> m = MaterialAsset::Get(mesh.MaterialSlotDefault(slot)))
            return m;
        return engineDefault;
    };

    // Draw one index range per submesh. The material binds state + uniforms; the
    // renderer issues the submit. DISCARD_ALL clears bindings between submeshes.
    const auto drawRange = [&](u32 firstIndex, u32 indexCount, Ref<MaterialAsset> material) {
        if (!material)
            return;
        bgfx::setTransform(glm::value_ptr(transform));
        bgfx::setVertexBuffer(0, vb);
        bgfx::setIndexBuffer(ib, firstIndex, indexCount);
        material->Bind();
        bgfx::submit(viewId, material->Program(), 0, BGFX_DISCARD_ALL);
    };

    const std::vector<Mesh::Submesh>& submeshes = mesh.Submeshes();
    if (submeshes.empty()) {
        drawRange(0, mesh.IndexCount(), resolveSlot(0));
    } else {
        for (const Mesh::Submesh& submesh : submeshes)
            drawRange(submesh.BaseIndex, submesh.IndexCount, resolveSlot(submesh.MaterialSlot));
    }
}

void Renderer::Begin(uint16_t viewId)
{
    s_RenderData.currentViewId = viewId;

    bgfx::touch(s_RenderData.currentViewId);
}

void Renderer::End()
{
    s_RenderData.EndFrame();
}

namespace
{
// Position + texcoord vertex for fullscreen passes.
struct FullscreenVertex
{
    float x, y, z;
    float u, v;
};

const bgfx::VertexLayout& FullscreenLayout()
{
    static const bgfx::VertexLayout s_layout = []
    {
        bgfx::VertexLayout l;
        l.begin()
            .add(bgfx::Attrib::Position, 3, bgfx::AttribType::Float)
            .add(bgfx::Attrib::TexCoord0, 2, bgfx::AttribType::Float)
            .end();
        return l;
    }();
    return s_layout;
}

// Resolve the embedded `tonemap` program (built + cached lazily on first use).
// GetProgram resolves from the ShaderManager's own embedded cache, so the
// program is stable even before an AssetManager is installed — avoiding the
// per-frame rebuild that recycled the tonemap uniforms during early startup.
bgfx::ProgramHandle ResolveTonemapProgram()
{
    return ShaderManager::GetProgram("tonemap");
}
} // namespace

void Renderer::DrawFullscreen(
    uint16_t viewId, bgfx::ProgramHandle program, uint64_t state)
{
    if (!bgfx::isValid(program))
        return;

    const bgfx::VertexLayout& layout = FullscreenLayout();
    if (bgfx::getAvailTransientVertexBuffer(3, layout) < 3)
        return;

    bgfx::TransientVertexBuffer tvb;
    bgfx::allocTransientVertexBuffer(&tvb, 3, layout);
    FullscreenVertex* vtx = reinterpret_cast<FullscreenVertex*>(tvb.data);

    // One oversized triangle covering the [-1,1] clip region; the passthrough VS
    // writes these as clip-space positions directly. UVs map the visible [0,1]
    // region to the source texture. Framebuffer origin differs by backend, so V
    // is flipped when the backbuffer origin is bottom-left (OpenGL/GLES).
    const bool originBottomLeft = bgfx::getCaps()->originBottomLeft;
    const float v0 = originBottomLeft ? 0.0f : 1.0f;
    const float v2 = originBottomLeft ? 2.0f : -1.0f;

    vtx[0] = { -1.0f, -1.0f, 0.0f, 0.0f, v0 };
    vtx[1] = {  3.0f, -1.0f, 0.0f, 2.0f, v0 };
    vtx[2] = { -1.0f,  3.0f, 0.0f, 0.0f, v2 };

    bgfx::setVertexBuffer(0, &tvb, 0, 3);
    bgfx::setState(state);
    bgfx::submit(viewId, program);
}

void Renderer::TonemapResolve(
    uint16_t viewId, bgfx::TextureHandle hdrColor, float exposure, int op)
{
    const bgfx::ProgramHandle program = ResolveTonemapProgram();
    if (!bgfx::isValid(program) || !bgfx::isValid(hdrColor))
        return;

    if (!bgfx::isValid(s_TonemapSampler))
        s_TonemapSampler = bgfx::createUniform("s_hdr", bgfx::UniformType::Sampler);
    if (!bgfx::isValid(s_TonemapParams))
        s_TonemapParams = bgfx::createUniform("u_tonemapParams", bgfx::UniformType::Vec4);

    const float params[4] = { exposure, static_cast<float>(op), 0.0f, 0.0f };
    bgfx::setUniform(s_TonemapParams, params);
    bgfx::setTexture(0, s_TonemapSampler, hdrColor);

    // No depth test/write: a fullscreen resolve overwrites the whole target.
    DrawFullscreen(viewId, program, BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A);
}

void Renderer::DrawSkybox(
    uint16_t viewId, bgfx::TextureHandle cube, const glm::mat4& invViewProj,
    const glm::vec3& cameraPos, float intensity, float rotationYaw, float mipLod)
{
    const bgfx::ProgramHandle program = ShaderManager::GetProgram("skybox");
    if (!bgfx::isValid(program) || !bgfx::isValid(cube))
        return;

    if (!bgfx::isValid(s_SkyCube))
        s_SkyCube = bgfx::createUniform("s_skyCube", bgfx::UniformType::Sampler);
    if (!bgfx::isValid(s_SkyInvViewProj))
        s_SkyInvViewProj = bgfx::createUniform("u_skyInvViewProj", bgfx::UniformType::Mat4);
    if (!bgfx::isValid(s_SkyCameraPos))
        s_SkyCameraPos = bgfx::createUniform("u_skyCameraPos", bgfx::UniformType::Vec4);
    if (!bgfx::isValid(s_SkyParams))
        s_SkyParams = bgfx::createUniform("u_skyParams", bgfx::UniformType::Vec4);

    const glm::vec4 camPos(cameraPos, 1.0f);
    const float params[4] = { intensity, rotationYaw, mipLod, 0.0f };
    bgfx::setUniform(s_SkyInvViewProj, glm::value_ptr(invViewProj));
    bgfx::setUniform(s_SkyCameraPos, glm::value_ptr(camPos));
    bgfx::setUniform(s_SkyParams, params);
    bgfx::setTexture(0, s_SkyCube, cube);

    // Write color only (keep the geometry depth buffer), and reject any pixel a
    // mesh already wrote: the sky rasterizes at clip depth 0.0, which is the far
    // plane under reversed-Z, so GEQUAL passes only where depth is still the
    // cleared far value (no geometry) and fails where nearer geometry wrote > 0.
    const uint64_t state = BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A |
                           BGFX_STATE_DEPTH_TEST_GEQUAL;
    DrawFullscreen(viewId, program, state);
}

bgfx::TextureHandle Renderer::BrdfLut()
{
    if (bgfx::isValid(s_BrdfLut))
        return s_BrdfLut;

    const bgfx::ProgramHandle program = ShaderManager::GetProgram("brdf_lut");
    if (!bgfx::isValid(program))
        return BGFX_INVALID_HANDLE;

    // 256x256 RG16F is ample for the smooth split-sum response; clamp + bilinear
    // so the PBR shader can sample it by (NdotV, roughness). No depth attachment
    // is needed — the fullscreen triangle writes every texel with no depth test.
    constexpr uint16_t kSize = 256;
    s_BrdfLut = bgfx::createTexture2D(
        kSize, kSize, false, 1, bgfx::TextureFormat::RG16F,
        BGFX_TEXTURE_RT | BGFX_SAMPLER_U_CLAMP | BGFX_SAMPLER_V_CLAMP);
    if (!bgfx::isValid(s_BrdfLut))
        return BGFX_INVALID_HANDLE;

    // destroyTextures=false: we own s_BrdfLut (both freed in Cleanup).
    s_BrdfLutFb = bgfx::createFrameBuffer(1, &s_BrdfLut, false);

    RenderPass::ToTarget(ViewId::EnvBake, s_BrdfLutFb, kSize, kSize).Bind();
    DrawFullscreen(ViewId::EnvBake, program,
        BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A);

    return s_BrdfLut;
}

void Renderer::Clear(glm::vec3 clearColor, uint16_t flags)
{
    bgfx::setViewClear(
        s_RenderData.currentViewId, flags,
        EncodeColorRgba8(clearColor.r, clearColor.g, clearColor.b, 1.0f),
        0.0f, 0);
}

void Renderer::SetBackBufferSize(u32 width, u32 height)
{
    s_RenderData.windowWidth  = width;
    s_RenderData.windowHeight = height;
    bgfx::reset(width, height, s_RenderData.resetFlags);
    bgfx::setViewRect(0, 0, 0, (u16)width, (u16)height);
}

void Renderer::FlushFrame()
{
    // Ensure view 0 (backbuffer clear) fires even with no draw calls.
    bgfx::touch(0);
    s_RenderData.frameNumber = bgfx::frame(false);
}

u32 Renderer::FrameNumber()
{
    return s_RenderData.frameNumber;
}

} // namespace Graphics