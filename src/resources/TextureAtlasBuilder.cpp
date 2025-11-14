//
// Created by Claude Code on 2025/11/14.
//

#include "TextureAtlasBuilder.h"

#include "core/Log.h"
#include "graphics/TextureAtlas.h"
#include "ext/stb_image.h"
#include "ext/stb_image_write.h"

#include <cmath>
#include <cstring>
#include <fstream>
#include <sstream>

namespace Resources
{

TextureAtlasBuilder::TextureAtlasBuilder() {}

bool TextureAtlasBuilder::AddTexture(
    const std::string& resourceName, const std::string& filePath)
{
    // Check if already loaded
    if (m_Textures.find(resourceName) != m_Textures.end()) {
        CORE_WARN("Texture already added: {}", resourceName);
        return true;
    }

    TextureData data;
    data.resourceName = resourceName;
    data.filePath = filePath;

    if (!LoadPNG(filePath, data)) {
        CORE_ERROR("Failed to load texture: {}", filePath);
        return false;
    }

    CORE_INFO("Loaded texture: {} ({}x{})", resourceName, data.width, data.height);

    m_Textures[resourceName] = std::move(data);
    return true;
}

Graphics::TextureAtlas* TextureAtlasBuilder::BuildAtlas(int spriteSize, int padding)
{
    if (m_Textures.empty()) {
        CORE_ERROR("No textures to build atlas from");
        return nullptr;
    }

    m_SpriteSize = spriteSize;
    m_Padding = padding;

    // Calculate atlas dimensions
    glm::ivec2 atlasDims = CalculateAtlasDimensions(spriteSize, padding);
    CORE_INFO(
        "Building atlas: {}x{} pixels, {} textures, sprite size {}, padding {}", atlasDims.x,
        atlasDims.y, m_Textures.size(), spriteSize, padding);

    // Allocate atlas pixel data (RGBA)
    int atlasWidth = atlasDims.x;
    int atlasHeight = atlasDims.y;
    size_t atlasDataSize = atlasWidth * atlasHeight * 4; // RGBA
    unsigned char* atlasData = new unsigned char[atlasDataSize];
    std::memset(atlasData, 0, atlasDataSize); // Clear to transparent black

    // Pack textures into atlas
    PackTextures(atlasWidth, atlasHeight, spriteSize, padding);

    // Calculate grid dimensions for Y-flipping
    int gridWidth = static_cast<int>(std::ceil(std::sqrt(m_Textures.size())));
    int gridHeight = static_cast<int>(std::ceil(static_cast<double>(m_Textures.size()) / gridWidth));

    // Copy texture data into atlas
    for (const auto& [resourceName, position] : m_Positions) {
        const TextureData& texture = m_Textures[resourceName];

        int destX = position.gridCoords.x * (spriteSize + padding);

        // Y-flip the destination to match OpenGL texture coordinates
        // Grid Y=0 (top) should map to bottom of atlas image (higher pixel Y)
        int flippedGridY = (gridHeight - 1) - position.gridCoords.y;
        int destY = flippedGridY * (spriteSize + padding);

        // Copy each row of the texture
        for (int y = 0; y < texture.height && y < spriteSize; ++y) {
            for (int x = 0; x < texture.width && x < spriteSize; ++x) {
                int srcIdx = (y * texture.width + x) * 4;
                int destIdx = ((destY + y) * atlasWidth + (destX + x)) * 4;

                // Copy RGBA
                atlasData[destIdx + 0] = texture.pixels[srcIdx + 0]; // R
                atlasData[destIdx + 1] = texture.pixels[srcIdx + 1]; // G
                atlasData[destIdx + 2] = texture.pixels[srcIdx + 2]; // B
                atlasData[destIdx + 3] = texture.pixels[srcIdx + 3]; // A
            }
        }

        // Fill padding if needed (replicate edge pixels for mipmap safety)
        if (padding > 0) {
            // Right padding
            for (int py = 0; py < spriteSize; ++py) {
                int edgeX = destX + texture.width - 1;
                for (int px = 0; px < padding; ++px) {
                    int srcIdx = ((destY + py) * atlasWidth + edgeX) * 4;
                    int destIdx = ((destY + py) * atlasWidth + edgeX + px + 1) * 4;
                    std::memcpy(&atlasData[destIdx], &atlasData[srcIdx], 4);
                }
            }

            // Bottom padding
            for (int px = 0; px < spriteSize + padding; ++px) {
                int edgeY = destY + texture.height - 1;
                for (int py = 0; py < padding; ++py) {
                    int srcIdx = (edgeY * atlasWidth + (destX + px)) * 4;
                    int destIdx = ((edgeY + py + 1) * atlasWidth + (destX + px)) * 4;
                    std::memcpy(&atlasData[destIdx], &atlasData[srcIdx], 4);
                }
            }
        }
    }

    // DEBUG: Save atlas to file for inspection
    std::string debugPath = "debug_atlas.png";
    std::string debugInfoPath = "debug_atlas_info.txt";

    if (stbi_write_png(debugPath.c_str(), atlasWidth, atlasHeight, 4, atlasData, atlasWidth * 4)) {
        CORE_INFO("=== DEBUG: Saved atlas to {} ===", debugPath);
    } else {
        CORE_ERROR("Failed to save debug atlas");
    }

    // DEBUG: Write detailed texture positions to file
    std::ofstream debugFile(debugInfoPath);
    if (debugFile.is_open()) {
        debugFile << "=== ATLAS DEBUG INFO ===\n";
        debugFile << "Atlas dimensions: " << atlasWidth << "x" << atlasHeight << " pixels\n";
        debugFile << "Logical grid: " << gridWidth << "x" << gridHeight << "\n";
        debugFile << "Sprite size: " << spriteSize << ", Padding: " << padding << "\n";
        debugFile << "Total textures: " << m_Positions.size() << "\n\n";
        debugFile << "Texture positions:\n";

        for (const auto& [resourceName, position] : m_Positions) {
            int pixelX = position.gridCoords.x * (spriteSize + padding);
            // Show actual flipped pixel Y position
            int flippedGridY = (gridHeight - 1) - position.gridCoords.y;
            int pixelY = flippedGridY * (spriteSize + padding);

            debugFile << "  '" << resourceName << "'\n";
            debugFile << "    Grid: (" << position.gridCoords.x << ", " << position.gridCoords.y << ") [logical]\n";
            debugFile << "    Pixel: (" << pixelX << ", " << pixelY << ") [actual Y-flipped]\n";
            debugFile << "    UV Offset: (" << position.uvOffset.x << ", " << position.uvOffset.y << ")\n";
            debugFile << "    UV Size: (" << position.uvSize.x << ", " << position.uvSize.y << ")\n\n";
        }

        debugFile << "=== END ATLAS DEBUG ===\n";
        debugFile.close();

        CORE_INFO("=== DEBUG: Saved atlas info to {} ===", debugInfoPath);
    } else {
        CORE_ERROR("Failed to save debug atlas info file");
    }

    // Create TextureAtlas from the generated data
    Graphics::TextureAtlas* atlas = Graphics::TextureAtlas::CreateFromMemory(
        atlasData, atlasWidth, atlasHeight, spriteSize, "generated_block_atlas");

    // Clean up our local copy of the data (bgfx has copied it)
    delete[] atlasData;

    if (!atlas) {
        CORE_ERROR("Failed to create TextureAtlas from memory");
        return nullptr;
    }

    CORE_INFO("Successfully built atlas: {}x{} pixels, {} textures packed", atlasWidth,
        atlasHeight, m_Positions.size());

    return atlas;
}

const AtlasPosition* TextureAtlasBuilder::GetTexturePosition(
    const std::string& resourceName) const
{
    auto it = m_Positions.find(resourceName);
    return it != m_Positions.end() ? &it->second : nullptr;
}

void TextureAtlasBuilder::Clear()
{
    m_Textures.clear();
    m_Positions.clear();
}

bool TextureAtlasBuilder::LoadPNG(const std::string& filePath, TextureData& outData)
{
    int width, height, channels;
    unsigned char* data = stbi_load(filePath.c_str(), &width, &height, &channels, 4); // Force RGBA

    if (!data) {
        CORE_ERROR("Failed to load PNG: {} ({})", filePath, stbi_failure_reason());
        return false;
    }

    outData.width = width;
    outData.height = height;
    outData.pixels = data; // Transfer ownership

    return true;
}

glm::ivec2 TextureAtlasBuilder::CalculateAtlasDimensions(int spriteSize, int padding) const
{
    // Simple approach: calculate grid dimensions
    int textureCount = static_cast<int>(m_Textures.size());

    // Calculate grid size (square or near-square)
    int gridWidth = static_cast<int>(std::ceil(std::sqrt(textureCount)));
    int gridHeight = static_cast<int>(std::ceil(static_cast<double>(textureCount) / gridWidth));

    // Calculate atlas size in pixels
    int cellSize = spriteSize + padding;
    int atlasWidth = gridWidth * cellSize;
    int atlasHeight = gridHeight * cellSize;

    // Round up to power of 2 for GPU efficiency
    auto nextPowerOf2 = [](int n) {
        n--;
        n |= n >> 1;
        n |= n >> 2;
        n |= n >> 4;
        n |= n >> 8;
        n |= n >> 16;
        n++;
        return n;
    };

    atlasWidth = nextPowerOf2(atlasWidth);
    atlasHeight = nextPowerOf2(atlasHeight);

    // Clamp to reasonable max (e.g., 4096x4096)
    atlasWidth = std::min(atlasWidth, 4096);
    atlasHeight = std::min(atlasHeight, 4096);

    return glm::ivec2(atlasWidth, atlasHeight);
}

void TextureAtlasBuilder::PackTextures(int atlasWidth, int atlasHeight, int spriteSize, int padding)
{
    // Calculate logical grid dimensions based on number of textures
    int textureCount = static_cast<int>(m_Textures.size());
    int gridWidth = static_cast<int>(std::ceil(std::sqrt(textureCount)));
    int gridHeight = static_cast<int>(std::ceil(static_cast<double>(textureCount) / gridWidth));

    CORE_INFO("Packing {} textures into logical {}x{} grid in {}x{} atlas",
        textureCount, gridWidth, gridHeight, atlasWidth, atlasHeight);

    int x = 0;
    int y = 0;

    for (auto& [resourceName, texture] : m_Textures) {
        if (x >= gridWidth) {
            x = 0;
            y++;
        }

        if (y >= gridHeight) {
            CORE_ERROR("Atlas too small to fit all textures!");
            break;
        }

        // Store position
        AtlasPosition position;
        position.gridCoords = glm::ivec2(x, y);

        // Calculate UV coordinates based on ACTUAL atlas dimensions
        float atlasWidthFloat = static_cast<float>(atlasWidth);
        float atlasHeightFloat = static_cast<float>(atlasHeight);
        float spriteSizeFloat = static_cast<float>(spriteSize + padding);

        // Calculate actual UV size for one sprite in atlas space
        position.uvSize = glm::vec2(
            spriteSizeFloat / atlasWidthFloat,
            spriteSizeFloat / atlasHeightFloat
        );

        // Calculate UV offset (Y-flipped for OpenGL)
        position.uvOffset.x = (x * spriteSizeFloat) / atlasWidthFloat;
        position.uvOffset.y = ((gridHeight - 1.0f - y) * spriteSizeFloat) / atlasHeightFloat;

        m_Positions[resourceName] = position;

        x++;
    }

    CORE_INFO("Packed {} textures into {}x{} grid", m_Positions.size(), gridWidth, gridHeight);
}

} // namespace Resources
