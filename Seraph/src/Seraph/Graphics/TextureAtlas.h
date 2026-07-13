//
// Created by Ruben on 2025/05/23.
//
#pragma once

#include "Seraph/Core/Base.h"
#include "Seraph/Core/Ref.h"
#include "Seraph/Graphics/Texture2D.h"

#include <string>

namespace Seraph
{

class TextureAtlas: public RefCounted
{
public:
    TextureAtlas(
        Ref<Texture2D> texture, std::string path, uint32_t width,uint32_t height, uint32_t spriteSize);

    // Wraps an already-loaded texture asset (resolved through the AssetManager).
    // Textures are never read from a path here — that is the asset system's job.
    static Ref<TextureAtlas> Create(AssetHandle texture, uint32_t spriteSize);

    /**
     * Create a TextureAtlas from raw RGBA pixel data in memory.
     * @param data Raw RGBA pixel data (4 bytes per pixel)
     * @param width Atlas width in pixels
     * @param height Atlas height in pixels
     * @param spriteSize Size of each sprite in pixels
     * @param name Optional name for the texture (for debugging)
     * @return TextureAtlas* if successful, nullptr otherwise
     */
    // static TextureAtlas* CreateFromMemory(
    //     const unsigned char* data,
    //     uint32_t width,
    //     uint32_t height,
    //     uint32_t spriteSize,
    //     const std::string& name = "atlas_from_memory");

    [[nodiscard]] Ref<Texture2D> Texture() const { return m_Texture;}
    [[nodiscard]] u32 Width() const { return m_Width; }
    [[nodiscard]] u32 Height() const { return m_Height; }
    [[nodiscard]] u32 SpriteSize() const { return m_SpriteSize; }

    // void AddTexture(const std::string& texturePath, const std::string& textureName);
    // bgfx::TextureHandle TextureByName(const std::string& textureName);
private:
    // std::unordered_map<std::string, bgfx::TextureHandle> m_Textures;
    Ref<Texture2D> m_Texture;

    std::string m_Path;
    uint32_t m_Width, m_Height;
    uint32_t m_SpriteSize;

};

}
