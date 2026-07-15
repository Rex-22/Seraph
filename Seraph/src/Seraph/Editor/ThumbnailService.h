//
// Supplies preview textures for the Asset Browser grid. v1 resolves only
// Texture2D assets, which display their own GPU texture directly; every other
// type returns nullopt so the panel draws a typed placeholder tile.
//
// The GetThumbnail() seam is intentionally type-agnostic: a future phase will
// render mesh/material previews to offscreen RenderTargets and cache them here
// without changing the panel.
//

#pragma once

#include "Seraph/Asset/AssetHandle.h"

#include <bgfx/bgfx.h>

#include <optional>

namespace Seraph
{

class ThumbnailService
{
public:
    // A GPU texture to preview `handle`, or nullopt when none is available yet
    // (unsupported type, or a texture still loading). Resolving a texture asset
    // triggers its lazy load, so the thumbnail may appear a frame or two later.
    std::optional<bgfx::TextureHandle> GetThumbnail(AssetHandle handle);
};

} // namespace Seraph
