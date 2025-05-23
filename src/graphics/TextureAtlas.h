//
// Created by Ruben on 2025/05/23.
//
#pragma once
#include "bgfx/bgfx.h"
#include "glm/gtx/io.hpp"
#include "glm/vec2.hpp"

#include <string>

namespace Graphics
{
class TextureAtlas
{
private:
    TextureAtlas(bgfx::TextureHandle handle, std::string path, uint32_t width,uint32_t height, uint32_t spriteSize);
public:
    ~TextureAtlas();

    static TextureAtlas* Create(const std::string& path, uint32_t spriteSize);

    bgfx::TextureHandle TextureHandle() const;
    uint32_t Width() const { return m_Width; }
    uint32_t Height() const { return m_Height; }
    uint32_t SpriteSize() const { return m_SpriteSize; }

    // void AddTexture(const std::string& texturePath, const std::string& textureName);
    // bgfx::TextureHandle TextureByName(const std::string& textureName);
private:
    // std::unordered_map<std::string, bgfx::TextureHandle> m_Textures;
    bgfx::TextureHandle m_TextureHandle;

    std::string m_Path;
    uint32_t m_Width, m_Height;
    uint32_t m_SpriteSize;

};

}
