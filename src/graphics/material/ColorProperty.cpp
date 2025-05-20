//
// Created by Ruben on 2025/05/20.
//

#include "ColorProperty.h"

#include "glm/gtc/type_ptr.inl"

#include <bgfx/bgfx.h>

namespace Graphics
{

ColorProperty::ColorProperty(const std::string& name, const glm::vec4& color)
    : MaterialProperty(name, PropertyType::Color), m_Color(color)
{
    m_UniformHandle =
        bgfx::createUniform(m_Name.c_str(), bgfx::UniformType::Vec4);
}
ColorProperty::~ColorProperty()
{
    bgfx::destroy(m_UniformHandle);
}

void ColorProperty::Apply(bgfx::ProgramHandle program) const
{
    if (bgfx::isValid(m_UniformHandle)) {
        bgfx::setUniform(m_UniformHandle, glm::value_ptr(m_Color));
    }
}

} // namespace Graphics