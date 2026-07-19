//
// Created for the multi-pass rendering seam (RenderingBoard "Render 3").
//

#include "RenderPass.h"

namespace Seraph
{

void RenderPass::Bind() const
{
    bgfx::setViewFrameBuffer(id, target);
    bgfx::setViewRect(id, x, y, width, height);
    if (clearFlags != BGFX_CLEAR_NONE)
        bgfx::setViewClear(id, clearFlags, clearColor, clearDepth, clearStencil);
}

} // namespace Seraph
