//
// Created by Ruben on 2025/05/20.
//

#pragma once
#include "MaterialProperty.h"
#include "glm/glm.hpp"

namespace Graphics
{
class Vector3Property final : public MaterialProperty
{
public:
    Vector3Property(const std::string& name, const glm::vec3& value);
    ~Vector3Property() override;

    void Apply() const override;

    const glm::vec3& Value() const { return m_Value; }
    void SetValue(const glm::vec3& value) { m_Value = value; }
private:
    glm::vec3 m_Value;
    bgfx::UniformHandle m_UniformHandle {};
};

} // namespace myNamespace
