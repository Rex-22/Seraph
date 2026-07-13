//
// Created by Ruben on 2025/05/20.
//

#include "Material.h"
#include "ColorProperty.h"
#include "MaterialProperty.h"
#include "TextureProperty.h"

#include "Seraph/Asset/AssetManager.h"
#include "Seraph/Graphics/ShaderAsset.h"
#include "Seraph/Graphics/ShaderManager.h"
#include "Seraph/Graphics/Texture2D.h"

#include <ranges>

namespace Seraph
{

Material::Material(AssetHandle shader) : m_Shader(shader)
{
}

Ref<Material> Material::CreateDefault()
{
    Ref<Material> material =
        Ref<Material>::Create(ShaderManager::GetHandle("simple"));

    const u32 white = 0xffffffff;
    Ref<Texture2D> texture = Texture2D::Create("DefaultWhite", &white, 1, 1);

    material->AddProperty<ColorProperty>("s_color", glm::vec4(1.0f));
    material->AddProperty<TextureProperty>("s_texColor", texture, 0);
    return material;
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
