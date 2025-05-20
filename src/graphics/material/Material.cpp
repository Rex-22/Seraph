//
// Created by Ruben on 2025/05/20.
//

#include "Material.h"
#include "MaterialProperty.h"

#include <ranges>

namespace Graphics
{

Material::Material(bgfx::ProgramHandle program) : m_Program(program)
{
}

Material::~Material()
{
    if (bgfx::isValid(m_Program)) {
        bgfx::destroy(m_Program);
    }
}

Material::Material(Material&& other) noexcept
    : m_Program(other.m_Program)
    , m_Properties(std::move(other.m_Properties))
{
    // Invalidate the moved-from program handle
    other.m_Program.idx = bgfx::kInvalidHandle;
}

Material& Material::operator=(Material&& other) noexcept
{
    if (this != &other) {
        // Clean up existing resources
        if (bgfx::isValid(m_Program)) {
            bgfx::destroy(m_Program);
        }

        // Move resources from other
        m_Program = other.m_Program;
        m_Properties = std::move(other.m_Properties);

        // Invalidate the moved-from program handle
        other.m_Program.idx = bgfx::kInvalidHandle;
    }
    return *this;
}

MaterialProperty* Material::GetProperty(const std::string& name) const
{
    if (const auto it = m_Properties.find(name); it != m_Properties.end()) {
        return it->second.get();
    }
    return nullptr;
}

void Material::Apply(uint8_t viewId, uint64_t state) const
{
    bgfx::setState(state);

    for (const auto& val : m_Properties | std::views::values) {
        val->Apply(m_Program);
    }

    bgfx::submit(viewId, m_Program);
}

} // namespace Graphics