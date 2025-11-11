//
// Created by Claude Code on 2025/11/11.
//

#pragma once

#include <glm/glm.hpp>
#include <string>

namespace Resources
{

/**
 * Represents a single face of a block element in a Minecraft-compatible model.
 * Defines texture mapping, UV coordinates, culling, rotation, and tinting.
 */
struct BlockFace
{
    /// Texture variable reference (e.g., "#side") or direct path (e.g., "block/dirt")
    std::string texture;

    /// UV coordinates in texture space [u0, v0, u1, v1]
    /// Range: 0-16 (maps to texture pixels)
    /// Default is calculated from face bounds
    glm::vec4 uv = glm::vec4(0, 0, 16, 16);

    /// Direction to cull against ("down", "up", "north", "south", "west", "east")
    /// If adjacent block is opaque on this side, don't render this face
    /// Empty string means no culling
    std::string cullface;

    /// Rotate texture clockwise in 90° increments (0, 90, 180, 270)
    int rotation = 0;

    /// Tint layer index
    /// -1: No tint (default)
    /// 0+: Apply biome/block-specific tint (e.g., grass color)
    int tintindex = -1;

    BlockFace() = default;

    BlockFace(
        const std::string& texture_, const glm::vec4& uv_ = glm::vec4(0, 0, 16, 16),
        const std::string& cullface_ = "", int rotation_ = 0, int tintindex_ = -1)
        : texture(texture_)
        , uv(uv_)
        , cullface(cullface_)
        , rotation(rotation_)
        , tintindex(tintindex_)
    {
    }
};

} // namespace Resources
