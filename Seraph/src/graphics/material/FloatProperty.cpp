//
// Created by Ruben on 2025/05/20.
//

#include "FloatProperty.h"

#include "bgfx/bgfx.h"

namespace Graphics
{

FloatProperty::FloatProperty(const std::string& name, float value)
    : MaterialProperty(name, Float), m_Value(value)
{
    m_UniformHandle =
        bgfx::createUniform(m_Name.c_str(), bgfx::UniformType::Vec4);
}

FloatProperty::~FloatProperty()
{
    bgfx::destroy(m_UniformHandle);
}

void FloatProperty::Apply() const
{
    if (bgfx::isValid(m_UniformHandle)) {
        bgfx::setUniform(m_UniformHandle, &m_Value, 1);
    }
}

} // namespace Graphics