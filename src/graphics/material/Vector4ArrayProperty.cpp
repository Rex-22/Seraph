//
// Created by Ruben on 2025/05/20.
//

#include "Vector4ArrayProperty.h"

#include "bgfx/bgfx.h"
#include "glm/gtc/type_ptr.inl"

namespace Graphics
{

Vector4ArrayProperty::Vector4ArrayProperty(
    const std::string& name, glm::vec4* values, uint16_t count)
    : MaterialProperty(name, Vector4), m_Values(values), m_Count(count)
{
    m_UniformHandle = bgfx::createUniform(m_Name.c_str(), bgfx::UniformType::Vec4, m_Count);
}

Vector4ArrayProperty::~Vector4ArrayProperty()
{
    bgfx::destroy(m_UniformHandle);
}

void Vector4ArrayProperty::Apply(bgfx::ProgramHandle program) const
{
    if (bgfx::isValid(m_UniformHandle)) {
        bgfx::setUniform(m_UniformHandle, m_Values, m_Count);
    }
}

} // namespace Graphics