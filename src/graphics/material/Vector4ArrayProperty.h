//
// Created by Ruben on 2025/05/20.
//

#pragma once
#include "MaterialProperty.h"
#include "glm/glm.hpp"

namespace Graphics
{
class Vector4ArrayProperty final : public MaterialProperty
{
public:
    Vector4ArrayProperty(
        const std::string& name, glm::vec4* value, uint16_t count);
    ~Vector4ArrayProperty() override;

    void Apply(bgfx::ProgramHandle program) const override;

    [[nodiscard]] glm::vec4* Values() const { return m_Values; }
    [[nodiscard]] uint16_t Count() const { return m_Count; }
    void SetValues(glm::vec4* values, uint16_t count)
    {
        m_Values = values;
        m_Count = count;
    }

private:
    glm::vec4* m_Values = nullptr;
    uint16_t m_Count;

    bgfx::UniformHandle m_UniformHandle {};
};

} // namespace Graphics
