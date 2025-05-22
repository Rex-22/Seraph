//
// Created by Ruben on 2025/05/20.
//

#pragma once
#include "MaterialProperty.h"
#include "glm/vec4.hpp"

namespace Graphics
{
class ColorProperty final : public MaterialProperty
{
public:
    ColorProperty(const std::string& name, const glm::vec4& color);
    ~ColorProperty() override;

    void Apply() const override;

    [[nodiscard]] const glm::vec4& Color() const { return m_Color; }
    void SetColor(const glm::vec4& color) { m_Color = color; }

private:
    glm::vec4 m_Color;

    bgfx::UniformHandle m_UniformHandle{};
};

} // namespace myNamespace