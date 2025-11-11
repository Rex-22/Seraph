//
// Created by Claude Code on 2025/11/11.
//

#include "TextureManager.h"

#include "core/Log.h"
#include "graphics/TextureAtlas.h"

#include <filesystem>

namespace fs = std::filesystem;

namespace Resources
{

TextureManager::~TextureManager() { Clear(); }

bool TextureManager::LoadResourcePack(const std::string& packPath)
{
    m_PackPath = packPath;

    // Scan for textures
    std::string texturesPath = packPath + "/textures/block";
    if (fs::exists(texturesPath)) {
        ScanTextures(texturesPath);
    } else {
        CORE_WARN("Textures directory not found: {}", texturesPath);
    }

    // Build atlases
    BuildAtlases();

    CORE_INFO(
        "Loaded resource pack from {}: {} textures", packPath, m_TextureRegistry.size());

    return true;
}

Graphics::TextureAtlas* TextureManager::GetAtlas(const std::string& atlasName)
{
    auto it = m_Atlases.find(atlasName);
    return it != m_Atlases.end() ? it->second.get() : nullptr;
}

TextureInfo TextureManager::GetTextureInfo(const std::string& resourceName)
{
    auto it = m_TextureRegistry.find(resourceName);
    if (it != m_TextureRegistry.end()) {
        return it->second;
    }

    // Return empty texture info if not found
    CORE_WARN("Texture not found: {}", resourceName);
    return TextureInfo();
}

bool TextureManager::HasTexture(const std::string& resourceName) const
{
    return m_TextureRegistry.find(resourceName) != m_TextureRegistry.end();
}

void TextureManager::UpdateAnimations(float deltaTime)
{
    // TODO: Implement animation updates when AnimatedTexture is implemented
    // For now, this is a no-op
}

void TextureManager::Clear()
{
    m_Atlases.clear();
    m_TextureRegistry.clear();
    m_PackPath.clear();
}

void TextureManager::ScanTextures(const std::string& texturesPath)
{
    // TODO: Implement full texture scanning
    // For now, this is a placeholder that will be expanded in Phase 5

    CORE_INFO("Scanning textures in: {}", texturesPath);

    try {
        for (const auto& entry : fs::directory_iterator(texturesPath)) {
            if (entry.is_regular_file()) {
                std::string filename = entry.path().filename().string();
                std::string extension = entry.path().extension().string();

                // Check if it's a PNG file
                if (extension == ".png") {
                    // Remove extension to get resource name
                    std::string resourceName =
                        filename.substr(0, filename.length() - extension.length());

                    // Add "block/" prefix to make full resource path
                    std::string fullResourceName = "block/" + resourceName;

                    CORE_INFO("Found texture: {}", fullResourceName);
                }
            }
        }
    } catch (const std::exception& e) {
        CORE_ERROR("Error scanning textures: {}", e.what());
    }
}

void TextureManager::BuildAtlases()
{
    // TODO: Implement dynamic atlas building in Phase 5
    // For now, use the existing hardcoded atlas approach

    // Create blocks atlas using existing TextureAtlas
    auto blocksAtlas = Graphics::TextureAtlas::Create("textures/block_sheet.png", 16);
    if (blocksAtlas) {
        // Register the atlas
        m_Atlases["blocks"] = std::unique_ptr<Graphics::TextureAtlas>(blocksAtlas);

        // Register known textures with their grid coordinates
        // These correspond to the existing block_sheet.png layout
        RegisterTexture("block/grass_block_side", glm::ivec2(0, 0), blocksAtlas);
        RegisterTexture("block/dirt", glm::ivec2(0, 1), blocksAtlas);
        RegisterTexture("block/grass_block_top", glm::ivec2(1, 0), blocksAtlas);
        RegisterTexture("block/stone", glm::ivec2(2, 0), blocksAtlas);

        CORE_INFO("Built blocks atlas: {} textures registered", m_TextureRegistry.size());
    } else {
        CORE_ERROR("Failed to create blocks atlas");
    }
}

void TextureManager::RegisterTexture(
    const std::string& resourceName, const glm::ivec2& gridCoords,
    const Graphics::TextureAtlas* atlas)
{
    TextureInfo info;
    info.gridCoords = gridCoords;
    info.atlas = atlas;

    // Calculate UV offset and size based on grid coords
    float atlasWidth = static_cast<float>(atlas->Width());
    float atlasHeight = static_cast<float>(atlas->Height());
    float spriteSize = static_cast<float>(atlas->SpriteSize());

    float numSpritesWidth = atlasWidth / spriteSize;
    float numSpritesHeight = atlasHeight / spriteSize;

    info.uvSize = glm::vec2(1.0f / numSpritesWidth, 1.0f / numSpritesHeight);

    // Calculate UV offset (Y-flipped for OpenGL)
    info.uvOffset.x = gridCoords.x * info.uvSize.x;
    info.uvOffset.y = (numSpritesHeight - 1.0f - gridCoords.y) * info.uvSize.y;

    m_TextureRegistry[resourceName] = info;
}

} // namespace Resources
