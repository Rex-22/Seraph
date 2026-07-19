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
    auto material = Ref<Material>::Create("pbr");

    const auto scalar = [&](const char* name, float value, glm::vec2 range) {
        MaterialParameter p;
        p.Name = name;
        p.Type = MaterialParameterType::Float;
        p.Vector = glm::vec4(value, 0.0f, 0.0f, 0.0f);
        p.Range = range;
        material->SetParameter(p);
    };
    const auto color = [&](const char* name, glm::vec4 value) {
        MaterialParameter p;
        p.Name = name;
        p.Type = MaterialParameterType::Color;
        p.Vector = value;
        material->SetParameter(p);
    };
    const auto sampler = [&](const char* name, AssetHandle tex, u8 stage) {
        MaterialParameter p;
        p.Name = name;
        p.Type = MaterialParameterType::Texture;
        p.Texture.Texture = tex;
        p.Texture.Stage = stage;
        material->SetParameter(p);
    };

    color("u_baseColorFactor", glm::vec4(0.8f, 0.8f, 0.8f, 1.0f));
    scalar("u_metallicFactor", 0.0f, {0.0f, 1.0f});
    scalar("u_roughnessFactor", 0.5f, {0.0f, 1.0f});
    color("u_emissiveFactor", glm::vec4(0.0f, 0.0f, 0.0f, 1.0f));
    scalar("u_normalScale", 1.0f, {0.0f, 2.0f});
    scalar("u_occlusionStrength", 1.0f, {0.0f, 1.0f});

    const AssetHandle white = Texture2D::DefaultWhiteHandle();
    sampler("s_albedo", white, 0);
    sampler("s_normal", Texture2D::DefaultFlatNormalHandle(), 1);
    sampler("s_metalRough", white, 2);
    sampler("s_ao", white, 3);
    sampler("s_emissive", white, 4);

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

    // Ensure the fallback textures are registered so the default material's
    // texture parameters resolve (white for color/metal-rough/ao, flat normal).
    Texture2D::GetDefaultWhite();
    Texture2D::GetDefaultFlatNormal();

    Ref<Material> material = CreateDefault();
    material->Handle = handle;
    AssetManager::AddMemoryAsset(material);
    return material;
}

} // namespace Seraph
