//
// Created by Claude Code on 2025/11/11.
//

#pragma once

#include <glm/glm.hpp>
#include <memory>
#include <string>
#include <unordered_map>

namespace Graphics
{
class TextureAtlas;
}

namespace Resources
{

/**
 * Information about a texture's location in an atlas.
 */
struct TextureInfo
{
    /// UV offset in atlas space [0-1]
    glm::vec2 uvOffset;

    /// UV size in atlas space [0-1]
    glm::vec2 uvSize;

    /// Grid coordinates in the atlas (x, y)
    glm::ivec2 gridCoords;

    /// Atlas this texture belongs to
    const Graphics::TextureAtlas* atlas = nullptr;

    /// Whether this texture is animated
    bool isAnimated = false;

    TextureInfo() : uvOffset(0, 0), uvSize(1, 1), gridCoords(0, 0) {}
};

/**
 * Manages texture atlases and provides texture lookup by resource name.
 * Supports multiple atlases (blocks, items, GUI, etc.) and animated textures.
 */
class TextureManager
{
public:
    TextureManager() = default;
    ~TextureManager();

    // Disable copy
    TextureManager(const TextureManager&) = delete;
    TextureManager& operator=(const TextureManager&) = delete;

    /**
     * Load a resource pack from the given path.
     * Scans for textures, parses metadata, and builds atlases.
     *
     * @param packPath Path to resource pack (should contain assets/)
     * @return True if loaded successfully
     */
    bool LoadResourcePack(const std::string& packPath);

    /**
     * Get a texture atlas by name.
     * @param atlasName Atlas name (e.g., "blocks", "items")
     * @return Pointer to atlas, or nullptr if not found
     */
    Graphics::TextureAtlas* GetAtlas(const std::string& atlasName);

    /**
     * Get texture information by resource name.
     * @param resourceName Resource name (e.g., "block/dirt")
     * @return Texture information
     */
    TextureInfo GetTextureInfo(const std::string& resourceName);

    /**
     * Check if a texture exists.
     */
    bool HasTexture(const std::string& resourceName) const;

    /**
     * Update animated textures.
     * Should be called each frame with deltaTime.
     */
    void UpdateAnimations(float deltaTime);

    /**
     * Clear all loaded data.
     */
    void Clear();

private:
    /**
     * Scan for texture files in a directory.
     */
    void ScanTextures(const std::string& texturesPath);

    /**
     * Build texture atlases from loaded textures.
     */
    void BuildAtlases();

    /**
     * Register a texture with its atlas coordinates.
     */
    void RegisterTexture(
        const std::string& resourceName, const glm::ivec2& gridCoords,
        const Graphics::TextureAtlas* atlas);

private:
    /// Texture atlases by name
    std::unordered_map<std::string, std::unique_ptr<Graphics::TextureAtlas>> m_Atlases;

    /// Texture lookup by resource name
    std::unordered_map<std::string, TextureInfo> m_TextureRegistry;

    /// Base path for resource pack
    std::string m_PackPath;
};

} // namespace Resources
