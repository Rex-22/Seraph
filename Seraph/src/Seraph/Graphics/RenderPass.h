//
// A lightweight description of a bgfx view/pass: which view id it draws to, its
// target framebuffer, viewport rect, and clear. Bind() applies that state for the
// current frame. This is the seam new passes plug into — it replaces the ad-hoc
// setViewFrameBuffer / setViewRect / setViewClear triples that were duplicated
// across the editor and runtime layers, and centralizes the reversed-Z clear
// convention (depth clears to 0.0).
//
// This is intentionally a thin, explicit descriptor, not an auto-scheduling
// render graph: layers still decide pass order (by view id) and submit geometry.
//

#pragma once

#include "Seraph/Core/Base.h"

#include <bgfx/bgfx.h>

namespace Seraph
{

struct RenderPass
{
    u16 id = 0;

    // Target framebuffer; BGFX_INVALID_HANDLE (default) means the backbuffer.
    bgfx::FrameBufferHandle target = BGFX_INVALID_HANDLE;

    // Viewport rect (in target pixels).
    u16 x = 0, y = 0, width = 0, height = 0;

    // Clear. Defaults to no clear (many passes are cleared elsewhere, e.g. the
    // scene pass is cleared by SceneRenderer, and a fullscreen resolve overwrites
    // every pixel). Depth clears to 0.0 to match the engine's reversed-Z depth.
    u16   clearFlags   = BGFX_CLEAR_NONE;
    u32   clearColor   = 0x000000ff; // packed RGBA8
    float clearDepth   = 0.0f;       // reversed-Z: far plane = 0.0
    u8    clearStencil = 0;

    // A pass targeting the whole framebuffer at (0,0).
    static RenderPass ToTarget(
        u16 viewId, bgfx::FrameBufferHandle fb, u16 w, u16 h)
    {
        RenderPass p;
        p.id = viewId;
        p.target = fb;
        p.width = w;
        p.height = h;
        return p;
    }

    // A pass targeting the backbuffer.
    static RenderPass ToBackbuffer(u16 viewId, u16 w, u16 h)
    {
        return ToTarget(viewId, BGFX_INVALID_HANDLE, w, h);
    }

    RenderPass& Clear(u16 flags, u32 rgba, float depth = 0.0f, u8 stencil = 0)
    {
        clearFlags = flags;
        clearColor = rgba;
        clearDepth = depth;
        clearStencil = stencil;
        return *this;
    }

    // Apply framebuffer + rect (+ clear, if clearFlags != NONE) to this view for
    // the current frame.
    void Bind() const;
};

} // namespace Seraph
