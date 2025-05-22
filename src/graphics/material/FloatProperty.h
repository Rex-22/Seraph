//
// Created by Ruben on 2025/05/20.
//

#pragma once
#include "MaterialProperty.h"

namespace Graphics
{
class FloatProperty final: public MaterialProperty
{
public:
    FloatProperty(const std::string& name, float value);
    ~FloatProperty() override;

    void Apply() const override;

    [[nodiscard]] float Value() const { return m_Value; }
    void SetValue(const float value) { m_Value = value; }

private:
    float m_Value;

    bgfx::UniformHandle m_UniformHandle{};
};

} // namespace myNamespace