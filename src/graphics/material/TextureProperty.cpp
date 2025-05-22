//
// Created by Ruben on 2025/05/20.
//

#include "TextureProperty.h"

#include "MaterialProperty.h"

namespace Graphics
{

TextureProperty::TextureProperty(
    const std::string& name, bgfx::TextureHandle textureHandle,
    uint8_t samplerUniformIndex)
    : MaterialProperty(name, Texture),
      m_TextureHandle(textureHandle), m_SamplerUniformIndex(samplerUniformIndex)
{
    m_UniformHandle = bgfx::createUniform(m_Name.c_str(), bgfx::UniformType::Sampler);
}

TextureProperty::~TextureProperty()
{
    if (bgfx::isValid(m_UniformHandle)) {
        bgfx::destroy(m_UniformHandle);
    }
}

void TextureProperty::Apply() const
{
    if (bgfx::isValid(m_UniformHandle) && bgfx::isValid(m_TextureHandle)) {
        bgfx::setTexture(m_SamplerUniformIndex, m_UniformHandle, m_TextureHandle);
    }
}

} // namespace Graphics