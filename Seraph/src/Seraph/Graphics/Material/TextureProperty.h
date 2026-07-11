//
// Created by Ruben on 2025/05/20.
//
#pragma once

#include "MaterialProperty.h"
#include "Seraph/Graphics/Texture2D.h"
#include "bgfx/bgfx.h"

namespace Seraph
{
class TextureProperty final: public MaterialProperty
{
public:
    TextureProperty(const std::string& name, Ref<Texture2D> texture, uint8_t samplerUniformIndex);
    ~TextureProperty() override;

    [[nodiscard]] bgfx::TextureHandle TextureHandle() const { return m_Texture->Handle(); }
    [[nodiscard]] Ref<Texture2D> Texture() const { return m_Texture; }

    void Apply() const override;

private:
    Ref<Texture2D> m_Texture;
    uint8_t m_SamplerUniformIndex = 0;

    bgfx::UniformHandle m_UniformHandle {};
};

} // namespace myNamespace