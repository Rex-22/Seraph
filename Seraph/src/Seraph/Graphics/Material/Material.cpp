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

Material::Material(bgfx::ProgramHandle program) : m_Program(program)
{
}

Material::Material(AssetHandle shader) : m_Shader(shader)
{
}

Material::Material(Material&& other) noexcept
    : m_Program(other.m_Program)
    , m_Properties(std::move(other.m_Properties))
    , m_Shader(other.m_Shader)
{
    // Invalidate the moved-from handles.
    other.m_Program.idx = bgfx::kInvalidHandle;
    other.m_Shader = c_NullAssetHandle;
}

Material& Material::operator=(Material&& other) noexcept
{
    if (this != &other) {
        m_Program = other.m_Program;
        m_Properties = std::move(other.m_Properties);
        m_Shader = other.m_Shader;

        // Invalidate the moved-from handles.
        other.m_Program.idx = bgfx::kInvalidHandle;
        other.m_Shader = c_NullAssetHandle;
    }
    return *this;
}

bgfx::ProgramHandle Material::Program() const
{
    // Asset-backed shader (migration target): resolve through the AssetManager.
    if (static_cast<u64>(m_Shader) != c_NullAssetHandle) {
        if (Ref<ShaderAsset> shader =
                AssetManager::GetAsset<ShaderAsset>(m_Shader))
            return shader->Program();
        return BGFX_INVALID_HANDLE;
    }
    // Legacy: directly-supplied program handle.
    return m_Program;
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
