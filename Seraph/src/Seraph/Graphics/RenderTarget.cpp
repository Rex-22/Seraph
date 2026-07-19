//
// Created by ruben on 2026/07/12.
//

#include "RenderTarget.h"

#include "Seraph/Core/Log.h"

#include <bgfx/bgfx.h>

namespace Seraph
{

bgfx::TextureFormat::Enum HDRColorFormat()
{
    const bgfx::Caps* caps = bgfx::getCaps();

    const auto framebufferCapable = [caps](bgfx::TextureFormat::Enum fmt) {
        return (caps->formats[fmt] & BGFX_CAPS_FORMAT_TEXTURE_FRAMEBUFFER) != 0;
    };

    if (framebufferCapable(bgfx::TextureFormat::RGBA16F))
        return bgfx::TextureFormat::RGBA16F;

    if (framebufferCapable(bgfx::TextureFormat::RG11B10F))
        return bgfx::TextureFormat::RG11B10F;

    SP_CORE_WARN_TAG("Renderer",
        "No framebuffer-capable HDR color format (RGBA16F/RG11B10F); "
        "falling back to RGBA8 LDR scene target");
    return bgfx::TextureFormat::RGBA8;
}

void RenderTarget::Create(u32 w, u32 h, bgfx::TextureFormat::Enum colorFmt)
{
    width       = w;
    height      = h;
    colorFormat = colorFmt;

    bgfx::TextureHandle attachments[2];

    // Color — point-sampled, clamp to edge.
    attachments[0] = bgfx::createTexture2D(
        (u16)w, (u16)h, false, 1,
        colorFormat,
        BGFX_TEXTURE_RT |
        BGFX_SAMPLER_U_CLAMP | BGFX_SAMPLER_V_CLAMP |
        BGFX_SAMPLER_MIN_POINT | BGFX_SAMPLER_MAG_POINT);

    // Depth — write-only; cleared to 0.0 each frame (reversed-Z convention).
    attachments[1] = bgfx::createTexture2D(
        (u16)w, (u16)h, false, 1,
        bgfx::TextureFormat::D24S8,
        BGFX_TEXTURE_RT | BGFX_TEXTURE_RT_WRITE_ONLY);

    // destroyTextures=true: bgfx owns the attachment handles.
    fb    = bgfx::createFrameBuffer(2, attachments, true);
    color = bgfx::getTexture(fb, 0);
}

void RenderTarget::Destroy()
{
    if (bgfx::isValid(fb)) {
        bgfx::destroy(fb);
        fb    = BGFX_INVALID_HANDLE;
        color = BGFX_INVALID_HANDLE;
    }
    width = height = 0;
}

void RenderTarget::Resize(u32 w, u32 h)
{
    const bgfx::TextureFormat::Enum fmt = colorFormat;
    Destroy();
    Create(w, h, fmt);
}

} // namespace Seraph
