//
// Created by Ruben on 2025/05/20.
//

#include "Vector3Property.h"

#include "bgfx/bgfx.h"
#include "glm/gtc/type_ptr.inl"

namespace Graphics
{

Vector3Property::Vector3Property(
    const std::string& name, const glm::vec3& value)
    : MaterialProperty(name, Vector3), m_Value(value)
{
    m_UniformHandle =
        bgfx::createUniform(m_Name.c_str(), bgfx::UniformType::Vec4);
}

Vector3Property::~Vector3Property()
{
    bgfx::destroy(m_UniformHandle);
}

void Vector3Property::Apply(bgfx::ProgramHandle program) const
{
    if (bgfx::isValid(m_UniformHandle)) {
        bgfx::setUniform(m_UniformHandle, glm::value_ptr(m_Value), 1);
    }
}

} // namespace Graphics