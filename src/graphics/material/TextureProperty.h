//
// Created by Ruben on 2025/05/20.
//
#pragma once

#include "MaterialProperty.h"
#include "bgfx/bgfx.h"

namespace Graphics
{
class TextureProperty final: public MaterialProperty
{
public:
    TextureProperty(const std::string& name, bgfx::TextureHandle textureHandle, uint8_t samplerUniformIndex);
    ~TextureProperty() override;

    [[nodiscard]] bgfx::TextureHandle TextureHandle() const { return m_TextureHandle; }
    void SetTextureHandle(bgfx::TextureHandle handle)
    {
        m_TextureHandle = handle;
    }

    void Apply(bgfx::ProgramHandle program) const override;

private:
    bgfx::TextureHandle m_TextureHandle {};
    uint8_t m_SamplerUniformIndex = 0;

    bgfx::UniformHandle m_UniformHandle {};
};

} // namespace myNamespace