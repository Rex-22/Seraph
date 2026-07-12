//
// Created by Ruben on 2025/05/20.
//

#include "Material.h"
#include "MaterialProperty.h"

#include "Seraph/Asset/AssetManager.h"
#include "Seraph/Graphics/ShaderAsset.h"

#include <ranges>

namespace Seraph
{

Material::Material(AssetHandle shader) : m_Shader(shader)
{
}

Material::Material(Material&& other) noexcept
    : m_Properties(std::move(other.m_Properties))
    , m_Shader(other.m_Shader)
{
    other.m_Shader = c_NullAssetHandle;
}

Material& Material::operator=(Material&& other) noexcept
{
    if (this != &other) {
        m_Properties = std::move(other.m_Properties);
        m_Shader = other.m_Shader;
        other.m_Shader = c_NullAssetHandle;
    }
    return *this;
}

bgfx::ProgramHandle Material::Program() const
{
    if (Ref<ShaderAsset> shader = AssetManager::GetAsset<ShaderAsset>(m_Shader))
        return shader->Program();
    return BGFX_INVALID_HANDLE;
}

MaterialProperty* Material::GetProperty(const std::string& name) const
{
    if (const auto it = m_Properties.find(name); it != m_Properties.end()) {
        return it->second.get();
    }
    return nullptr;
}

void Material::Apply(u16 viewId, uint64_t flags) const
{
    bgfx::setState(m_State);

    for (const auto& val : m_Properties | std::views::values) {
        val->Apply();
    }

    bgfx::submit(viewId, Program(), 0, flags);
}

} // namespace Seraph
