#include "Material.h"

#include "Seraph/Asset/AssetManager.h"
#include "Seraph/Graphics/ShaderManager.h"
#include "Seraph/Graphics/Texture2D.h"

#include <functional>
#include <string_view>

namespace Seraph
{

std::vector<AssetHandle> Material::GetDependencies() const
{
    std::vector<AssetHandle> deps;
    // Shader is stored by name; ShaderHandleFromName is a pure hash (no bgfx),
    // matching the handle a cooked shader is registered under.
    if (!m_ShaderName.empty())
        if (const AssetHandle shader = ShaderHandleFromName(m_ShaderName);
            static_cast<u64>(shader) != c_NullAssetHandle)
            deps.push_back(shader);
    for (const MaterialParameter& p : m_Parameters)
        if (p.IsTexture() && static_cast<u64>(p.Texture.Texture) != c_NullAssetHandle)
            deps.push_back(p.Texture.Texture);
    return deps;
}

void Material::SetParameter(const MaterialParameter& param)
{
    for (MaterialParameter& existing : m_Parameters) {
        if (existing.Name == param.Name) {
            existing = param;
            m_ResolveDirty = true;
            return;
        }
    }
    m_Parameters.push_back(param);
    m_ResolveDirty = true;
}

MaterialParameter* Material::FindParameter(const std::string& name)
{
    for (MaterialParameter& param : m_Parameters)
        if (param.Name == name)
            return &param;
    return nullptr;
}

const ResolvedMaterial& Material::Resolve()
{
    if (m_ResolveDirty) {
        m_Resolved.Shader = ShaderManager::GetHandle(m_ShaderName);
        m_Resolved.Params = m_Parameters;
        m_Resolved.State = m_State;
        m_ResolveDirty = false;
    }
    return m_Resolved;
}

Ref<Material> Material::CreateDefault()
{
    auto material = Ref<Material>::Create("simple");

    MaterialParameter color;
    color.Name = "s_color";
    color.Type = MaterialParameterType::Color;
    color.Vector = glm::vec4(1.0f);
    material->SetParameter(color);

    MaterialParameter albedo;
    albedo.Name = "s_texColor";
    albedo.Type = MaterialParameterType::Texture;
    albedo.Texture.Texture = Texture2D::DefaultWhiteHandle();
    albedo.Texture.Stage = 0;
    material->SetParameter(albedo);

    return material;
}

AssetHandle Material::DefaultHandle()
{
    static const AssetHandle handle = [] {
        const u64 hash = std::hash<std::string_view>{}("Seraph::DefaultMaterial");
        return AssetHandle(hash != c_NullAssetHandle ? hash : 1);
    }();
    return handle;
}

Ref<Material> Material::GetDefault()
{
    const AssetHandle handle = DefaultHandle();
    if (Ref<Material> existing = AssetManager::GetAsset<Material>(handle))
        return existing;

    // Ensure the fallback white texture is registered so the default material's
    // texture parameter resolves.
    Texture2D::GetDefaultWhite();

    Ref<Material> material = CreateDefault();
    material->Handle = handle;
    AssetManager::AddMemoryAsset(material);
    return material;
}

} // namespace Seraph
