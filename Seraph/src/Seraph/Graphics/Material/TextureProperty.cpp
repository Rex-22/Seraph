//
// Created by Ruben on 2025/05/20.
//

#include "TextureProperty.h"

#include "MaterialProperty.h"
#include "Seraph/Graphics/Texture2D.h"

namespace Seraph
{

TextureProperty::TextureProperty(
    const std::string& name, Ref<Texture2D> texture,
    uint8_t samplerUniformIndex)
: MaterialProperty(name, MaterialProperty::Texture),
      m_Texture(texture), m_SamplerUniformIndex(samplerUniformIndex)
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
    if (bgfx::isValid(m_UniformHandle) && m_Texture->IsValid()) {
        bgfx::setTexture(m_SamplerUniformIndex, m_UniformHandle, m_Texture->Handle());
    }
}

} // namespace Graphics