//
// Created by Claude Code on 2025/11/14.
//

#pragma once

#include "AnimatedTexture.h"
#include <string>

namespace Resources
{

/**
 * Parses .mcmeta files for animation metadata.
 */
class AnimationParser
{
public:
    /**
     * Parse a .mcmeta file.
     * @param mcmetaPath Path to the .mcmeta file
     * @param outMetadata Output animation metadata
     * @return True if successful
     */
    static bool ParseMcmeta(const std::string& mcmetaPath, AnimationMetadata& outMetadata);

    /**
     * Check if a .mcmeta file exists for a texture.
     * @param texturePath Path to texture (e.g., "assets/textures/block/water_still.png")
     * @return Path to .mcmeta file, or empty string if not found
     */
    static std::string FindMcmetaFile(const std::string& texturePath);
};

} // namespace Resources
