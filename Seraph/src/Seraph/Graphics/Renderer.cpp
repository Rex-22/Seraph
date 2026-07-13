//
// Created by Ruben on 2025/05/03.
//

#include "Renderer.h"

#include "Platform/Window.h"
#include "Seraph/Core/Application.h"
#include "Seraph/Core/Assert.h"
#include "Seraph/Core/Base.h"
#include "Seraph/Core/Core.h"
#include "Seraph/Graphics/Camera.h"
#include "Seraph/Graphics/Material/Material.h"
#include "Seraph/Graphics/Mesh.h"
#include "Seraph/Graphics/ShaderManager.h"

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

    void EndFrame()
    {
        currentViewId = -1;
    }
};

static RenderData s_RenderData;

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
    ShaderManager::Shutdown();
    bgfx::shutdown();
}

void Renderer::SubmitMesh(const Mesh& mesh, const glm::mat4& transform)
{
    const bgfx::VertexBufferHandle vb = mesh.VertexBuffer();
    const bgfx::IndexBufferHandle ib = mesh.IndexBuffer();
    if (!bgfx::isValid(vb) || !bgfx::isValid(ib))
        return;

    // v1: every material slot resolves to the shared engine default. Per-slot
    // defaults and per-entity overrides arrive with the material-asset phase.
    Ref<Material> material = Material::GetDefault();
    if (!material)
        return;

    const u16 viewId = s_RenderData.currentViewId;

    // Draw one index range per submesh; DISCARD_ALL after each submit clears the
    // buffer/transform/uniform bindings, so every submesh re-binds cleanly.
    const auto drawRange = [&](u32 firstIndex, u32 indexCount) {
        bgfx::setTransform(glm::value_ptr(transform));
        bgfx::setVertexBuffer(0, vb);
        bgfx::setIndexBuffer(ib, firstIndex, indexCount);
        material->Apply(viewId, BGFX_DISCARD_ALL);
    };

    const std::vector<Mesh::Submesh>& submeshes = mesh.Submeshes();
    if (submeshes.empty()) {
        drawRange(0, mesh.IndexCount());
    } else {
        for (const Mesh::Submesh& submesh : submeshes)
            drawRange(submesh.BaseIndex, submesh.IndexCount);
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
    bgfx::frame(false);
}

} // namespace Graphics