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
#include "Seraph/Graphics/Mesh.h"
#include "Seraph/Graphics/ShaderManager.h"

#include <glm/gtc/type_ptr.hpp>
#include <SDL3/SDL.h>
#include <bgfx/bgfx.h>
#include <bx/platform.h>

#include <cstdarg>
#include <cstdio>

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

        va_list argListCopy;
        va_copy(argListCopy, argList);
        const int written =
            std::vsnprintf(buffer, sizeof(buffer), format, argListCopy);
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
        buffer[length] = '\0';

        SP_CORE_TRACE_TAG("BGFX", " {}", buffer);
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
    bgfx_init.resolution.reset = BGFX_RESET_VSYNC;
    bgfx_init.platformData = pd;
    bgfx_init.callback = &s_BgfxCallback; // route bgfx logging to our logger
    bgfx::init(bgfx_init);

    bgfx::setViewClear(
        0, BGFX_CLEAR_COLOR | BGFX_CLEAR_DEPTH, 0x6495EDFF, 1.0f, 0);
    bgfx::setViewRect(0, 0, 0, window.Width(), window.Height());

    SP_CORE_INFO_TAG("Renderer", "Backend: {}", bgfx::getRendererName(bgfx::getRendererType()));
}

void Renderer::Cleanup()
{
    ShaderManager::Shutdown();
    bgfx::shutdown();
}

void Renderer::SubmitMesh(const Mesh& mesh, Transform& transform)
{
    mesh.Submit(s_RenderData.currentViewId,  transform);
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
        1.0f, 0);
}
void Renderer::OnWindowResize(u32 width, u32 height)
{
    s_RenderData.windowWidth = width;
    s_RenderData.windowHeight = height;
    bgfx::reset(width, height);
    bgfx::setViewRect(
        0, 0, 0, width, height);
}

void Renderer::FlushFrame()
{
    bgfx::frame(false);
}

} // namespace Graphics