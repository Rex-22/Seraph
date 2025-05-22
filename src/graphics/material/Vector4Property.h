//
// Created by Ruben on 2025/05/20.
//

#pragma once
#include "MaterialProperty.h"
#include "glm/glm.hpp"

namespace Graphics
{
class Vector4Property final : public MaterialProperty
{
public:
    Vector4Property(const std::string& name, const glm::vec4& value);
    ~Vector4Property() override;

    void Apply() const override;

    [[nodiscard]] const glm::vec4& Value() const { return m_Value; }
    void SetValue(const glm::vec4& value) { m_Value = value; }
private:
    glm::vec4 m_Value;
    bgfx::UniformHandle m_UniformHandle {};
};

} // namespace myNamespace
