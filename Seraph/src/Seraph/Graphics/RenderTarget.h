//
// Offscreen render target: a bgfx framebuffer with one color attachment and one
// D24S8 depth attachment. The color format is selectable (default RGBA8 LDR; use
// HDRColorFormat() for an RGBA16F/RG11B10F HDR scene target). Destroy() and
// Resize() are safe to call on an invalid (default-constructed) instance.
//

#pragma once

#include "Seraph/Core/Base.h"

#include <bgfx/bgfx.h>

namespace Seraph
{

// Best-supported HDR color format for an offscreen render target: prefers
// RGBA16F, falls back to RG11B10F, then RGBA8 if neither is framebuffer-capable.
// Must be called after bgfx::init (queries caps).
bgfx::TextureFormat::Enum HDRColorFormat();

struct RenderTarget
{
    bgfx::FrameBufferHandle   fb          = BGFX_INVALID_HANDLE;
    bgfx::TextureHandle       color       = BGFX_INVALID_HANDLE;
    u32                       width       = 0;
    u32                       height      = 0;
    bgfx::TextureFormat::Enum colorFormat = bgfx::TextureFormat::RGBA8;

    void Create(u32 w, u32 h, bgfx::TextureFormat::Enum colorFormat = bgfx::TextureFormat::RGBA8);
    void Destroy();
    void Resize(u32 w, u32 h); // recreates with the stored colorFormat

    bool IsValid() const { return bgfx::isValid(fb); }
};

} // namespace Seraph
