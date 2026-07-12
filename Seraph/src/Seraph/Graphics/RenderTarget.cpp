//
// Created by ruben on 2026/07/12.
//

#include "RenderTarget.h"

#include <bgfx/bgfx.h>

namespace Seraph
{

void RenderTarget::Create(u32 w, u32 h)
{
    width  = w;
    height = h;

    bgfx::TextureHandle attachments[2];

    // Color — point-sampled, clamp to edge.
    attachments[0] = bgfx::createTexture2D(
        (u16)w, (u16)h, false, 1,
        bgfx::TextureFormat::RGBA8,
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
    Destroy();
    Create(w, h);
}

} // namespace Seraph
