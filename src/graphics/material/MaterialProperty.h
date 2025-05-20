//
// Created by Ruben on 2025/05/20.
//
#pragma once

#include "bgfx/bgfx.h"

#include <string>

namespace bgfx
{
struct ProgramHandle;
}

namespace Graphics
{
class MaterialProperty
{
public:
    enum PropertyType
    {
        Color,
        Texture,
        Float,
        Vector3,
        Vector4
    };

    MaterialProperty(const std::string& name, PropertyType type);
    virtual ~MaterialProperty() = default;

    const std::string& Name() const { return m_Name; }
    PropertyType Type() const { return m_Type; }

    virtual void Apply(bgfx::ProgramHandle program) const = 0;

protected:
    std::string m_Name;
    PropertyType m_Type;
};

} // namespace myNamespace
