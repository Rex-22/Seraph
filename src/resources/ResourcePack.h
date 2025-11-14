//
// Created by Claude Code on 2025/11/14.
//

#pragma once

#include <string>

namespace Resources
{

/**
 * Resource pack metadata from pack.mcmeta.
 */
struct ResourcePackInfo
{
    /// Pack format version
    int packFormat = 0;

    /// Pack description
    std::string description;

    /// Pack name (from directory)
    std::string name;

    /// Full path to pack directory
    std::string path;

    ResourcePackInfo() = default;
};

/**
 * Parses and validates resource packs.
 */
class ResourcePack
{
public:
    ResourcePack() = default;
    ~ResourcePack() = default;

    /**
     * Load resource pack metadata from pack.mcmeta.
     * @param packPath Path to resource pack directory
     * @param outInfo Output pack info
     * @return True if successful
     */
    static bool LoadPackInfo(const std::string& packPath, ResourcePackInfo& outInfo);

    /**
     * Validate that a directory is a valid resource pack.
     * @param packPath Path to check
     * @return True if valid
     */
    static bool IsValidResourcePack(const std::string& packPath);

    /**
     * Get the pack format version required by this engine.
     */
    static int GetRequiredPackFormat() { return 6; } // Minecraft 1.16+
};

} // namespace Resources
