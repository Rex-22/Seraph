//
// Offscreen render target: a bgfx framebuffer with one RGBA8 color attachment
// and one D24S8 depth attachment. Destroy() and Resize() are safe to call on
// an invalid (default-constructed) instance.
//

#pragma once

#include "Seraph/Core/Base.h"

#include <bgfx/bgfx.h>

namespace Seraph
{

struct RenderTarget
{
    bgfx::FrameBufferHandle fb    = BGFX_INVALID_HANDLE;
    bgfx::TextureHandle     color = BGFX_INVALID_HANDLE;
    u32 width  = 0;
    u32 height = 0;

    void Create(u32 w, u32 h);
    void Destroy();
    void Resize(u32 w, u32 h);

    bool IsValid() const { return bgfx::isValid(fb); }
};

} // namespace Seraph
