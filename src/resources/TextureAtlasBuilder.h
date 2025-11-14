//
// Created by Claude Code on 2025/11/14.
//

#pragma once

#include <glm/glm.hpp>
#include <string>
#include <unordered_map>
#include <vector>

namespace Graphics
{
class TextureAtlas;
}

namespace Resources
{

/**
 * Represents a texture loaded from file, ready to be packed into an atlas.
 */
struct TextureData
{
    std::string resourceName; // e.g., "block/dirt"
    std::string filePath;     // Full path to PNG file
    int width = 0;
    int height = 0;
    unsigned char* pixels = nullptr; // Raw RGBA pixel data

    TextureData() = default;
    ~TextureData()
    {
        if (pixels) {
            delete[] pixels;
            pixels = nullptr;
        }
    }

    // Disable copy, allow move
    TextureData(const TextureData&) = delete;
    TextureData& operator=(const TextureData&) = delete;
    TextureData(TextureData&& other) noexcept
        : resourceName(std::move(other.resourceName))
        , filePath(std::move(other.filePath))
        , width(other.width)
        , height(other.height)
        , pixels(other.pixels)
    {
        other.pixels = nullptr;
    }
    TextureData& operator=(TextureData&& other) noexcept
    {
        if (this != &other) {
            delete[] pixels;
            resourceName = std::move(other.resourceName);
            filePath = std::move(other.filePath);
            width = other.width;
            height = other.height;
            pixels = other.pixels;
            other.pixels = nullptr;
        }
        return *this;
    }
};

/**
 * Position of a texture in an atlas.
 */
struct AtlasPosition
{
    glm::ivec2 gridCoords; // Grid coordinates (x, y)
    glm::vec2 uvOffset;    // UV offset [0-1]
    glm::vec2 uvSize;      // UV size [0-1]
};

/**
 * Builds texture atlases by packing textures from files.
 * Supports multiple atlases and automatic texture layout.
 */
class TextureAtlasBuilder
{
public:
    TextureAtlasBuilder();
    ~TextureAtlasBuilder() = default;

    /**
     * Add a texture file to be packed.
     * @param resourceName Resource name (e.g., "block/dirt")
     * @param filePath Full path to PNG file
     * @return True if loaded successfully
     */
    bool AddTexture(const std::string& resourceName, const std::string& filePath);

    /**
     * Build an atlas from all added textures.
     * @param spriteSize Size of each sprite in pixels (default: 16)
     * @param padding Padding between sprites in pixels (default: 0, recommend 2 for mipmaps)
     * @return Graphics::TextureAtlas* if successful, nullptr otherwise
     */
    Graphics::TextureAtlas* BuildAtlas(int spriteSize = 16, int padding = 0);

    /**
     * Get the atlas position for a texture resource.
     * @param resourceName Resource name
     * @return Atlas position, or nullptr if not found
     */
    const AtlasPosition* GetTexturePosition(const std::string& resourceName) const;

    /**
     * Get the number of textures added.
     */
    size_t GetTextureCount() const { return m_Textures.size(); }

    /**
     * Get all texture positions (after packing).
     */
    const std::unordered_map<std::string, AtlasPosition>& GetPositions() const
    {
        return m_Positions;
    }

    /**
     * Clear all loaded textures.
     */
    void Clear();

private:
    /**
     * Load a PNG file into TextureData.
     * @param filePath Path to PNG file
     * @param outData Output texture data
     * @return True if successful
     */
    bool LoadPNG(const std::string& filePath, TextureData& outData);

    /**
     * Calculate atlas dimensions needed to fit all textures.
     * @param spriteSize Size of each sprite
     * @param padding Padding between sprites
     * @return Atlas dimensions (width, height) in pixels
     */
    glm::ivec2 CalculateAtlasDimensions(int spriteSize, int padding) const;

    /**
     * Pack textures into the atlas using simple grid layout.
     * @param atlasWidth Atlas width in pixels
     * @param atlasHeight Atlas height in pixels
     * @param spriteSize Sprite size in pixels
     * @param padding Padding in pixels
     */
    void PackTextures(int atlasWidth, int atlasHeight, int spriteSize, int padding);

private:
    /// Textures loaded from files, indexed by resource name
    std::unordered_map<std::string, TextureData> m_Textures;

    /// Atlas positions after packing
    std::unordered_map<std::string, AtlasPosition> m_Positions;

    /// Sprite size used for the current atlas
    int m_SpriteSize = 16;

    /// Padding used for the current atlas
    int m_Padding = 0;
};

} // namespace Resources
