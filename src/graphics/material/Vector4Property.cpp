//
// Created by Ruben on 2025/05/20.
//

#include "Vector4Property.h"

#include "bgfx/bgfx.h"
#include "glm/gtc/type_ptr.inl"

namespace Graphics
{

Vector4Property::Vector4Property(
    const std::string& name, const glm::vec4& value)
    : MaterialProperty(name, Vector4), m_Value(value)
{
    m_UniformHandle = bgfx::createUniform(m_Name.c_str(), bgfx::UniformType::Vec4);
}

Vector4Property::~Vector4Property()
{
    bgfx::destroy(m_UniformHandle);
}

void Vector4Property::Apply(bgfx::ProgramHandle program) const
{
    if (bgfx::isValid(m_UniformHandle)) {
        bgfx::setUniform(m_UniformHandle, glm::value_ptr(m_Value), 1);
    }
}

} // namespace Graphics