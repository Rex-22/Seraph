//
// Created by Claude Code on 2025/11/11.
//

#include "TextureManager.h"

#include "TextureAtlasBuilder.h"
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

    // Create texture atlas builder
    TextureAtlasBuilder builder;

    // Scan for textures and add to builder
    std::string texturesPath = packPath + "/textures/block";
    if (fs::exists(texturesPath)) {
        ScanTextures(texturesPath, builder);
    } else {
        CORE_WARN("Textures directory not found: {}", texturesPath);
    }

    // Build atlases using the builder
    BuildAtlases(builder);

    CORE_INFO(
        "Loaded resource pack from {}: {} textures", packPath, m_TextureRegistry.size());

    return true;
}

Graphics::TextureAtlas* TextureManager::GetAtlas(const std::string& atlasName)
{
    auto it = m_Atlases.find(atlasName);
    return it != m_Atlases.end() ? it->second.get() : nullptr;
}

TextureInfo TextureManager::GetTextureInfo(const std::string& resourceName) const
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

void TextureManager::UpdateAnimations([[maybe_unused]] float deltaTime)
{
    // TODO: Implement animation updates when AnimatedTexture is implemented
    // For now, this is a no-op
}

bool TextureManager::ReloadResourcePack()
{
    if (m_PackPath.empty()) {
        CORE_ERROR("No resource pack loaded, cannot reload");
        return false;
    }

    CORE_INFO("Hot-reloading resource pack from: {}", m_PackPath);

    std::string currentPackPath = m_PackPath;
    Clear();
    return LoadResourcePack(currentPackPath);
}

bool TextureManager::SwitchResourcePack(const std::string& packPath)
{
    CORE_INFO("Switching resource pack to: {}", packPath);

    Clear();
    return LoadResourcePack(packPath);
}

void TextureManager::Clear()
{
    m_Atlases.clear();
    m_TextureRegistry.clear();
    m_PackPath.clear();
}

void TextureManager::ScanTextures(const std::string& texturesPath, TextureAtlasBuilder& builder)
{
    int textureCount = 0;
    try {
        for (const auto& entry : fs::directory_iterator(texturesPath)) {
            if (entry.is_regular_file()) {
                std::string filename = entry.path().filename().string();
                std::string extension = entry.path().extension().string();

                // Check if it's a PNG file (but not a .mcmeta file)
                if (extension == ".png") {
                    // Remove extension to get resource name
                    std::string resourceName =
                        filename.substr(0, filename.length() - extension.length());

                    // Add "block/" prefix to make full resource path
                    std::string fullResourceName = "block/" + resourceName;

                    // Get full file path
                    std::string filePath = entry.path().string();

                    // Add texture to builder
                    if (builder.AddTexture(fullResourceName, filePath)) {
                        textureCount++;
                    }
                }
            }
        }
        CORE_INFO("Scanned {} textures from {}", textureCount, texturesPath);
    } catch (const std::exception& e) {
        CORE_ERROR("Error scanning textures: {}", e.what());
    }
}

void TextureManager::BuildAtlases(TextureAtlasBuilder& builder)
{
    if (builder.GetTextureCount() == 0) {
        CORE_WARN("No textures to build atlas from, using fallback");

        // Fallback: Try to load existing hardcoded atlas if it exists
        auto blocksAtlas = Graphics::TextureAtlas::Create("textures/block_sheet.png", 16);
        if (blocksAtlas) {
            m_Atlases["blocks"] = std::unique_ptr<Graphics::TextureAtlas>(blocksAtlas);

            // Register known textures with their grid coordinates
            RegisterTexture("block/grass_block_side", glm::ivec2(0, 0), blocksAtlas);
            RegisterTexture("block/dirt", glm::ivec2(0, 1), blocksAtlas);
            RegisterTexture("block/grass_block_top", glm::ivec2(1, 0), blocksAtlas);
            RegisterTexture("block/stone", glm::ivec2(2, 0), blocksAtlas);

            CORE_INFO("Using fallback atlas: {} textures registered", m_TextureRegistry.size());
        }
        return;
    }

    // Build the atlas using TextureAtlasBuilder
    // Using sprite size 16 (Minecraft standard) and padding 0 (no padding)
    Graphics::TextureAtlas* blocksAtlas = builder.BuildAtlas(16, 0);

    if (!blocksAtlas) {
        CORE_ERROR("Failed to build blocks atlas");
        return;
    }

    // Store the atlas
    m_Atlases["blocks"] = std::unique_ptr<Graphics::TextureAtlas>(blocksAtlas);

    // Register all textures from the builder
    const auto& positions = builder.GetPositions();
    for (const auto& [resourceName, position] : positions) {
        // Create TextureInfo from AtlasPosition
        TextureInfo info;
        info.gridCoords = position.gridCoords;
        info.uvOffset = position.uvOffset;
        info.uvSize = position.uvSize;
        info.atlas = blocksAtlas;
        info.isAnimated = false; // TODO: Check for .mcmeta files

        m_TextureRegistry[resourceName] = info;
    }

    CORE_INFO("Built blocks atlas: {}x{} pixels, {} textures registered",
        blocksAtlas->Width(), blocksAtlas->Height(), m_TextureRegistry.size());
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
