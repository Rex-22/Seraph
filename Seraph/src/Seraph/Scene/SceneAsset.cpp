#include "SceneAsset.h"

#include "Seraph/Scene/Components/MeshComponent.h"
#include "Seraph/Scene/Entity.h"
#include "Seraph/Scene/EntityTemplates.h"

namespace Seraph
{

std::vector<AssetHandle> SceneAsset::GetDependencies() const
{
    std::vector<AssetHandle> deps;
    if (!m_Scene)
        return deps;

    // GetAllEntitiesWith / Entity are non-const APIs; reading the registry view
    // doesn't mutate scene data, so a const_cast keeps this query const.
    Scene* scene = const_cast<Scene*>(m_Scene.Raw());

    // Mirror the SceneSerializer's entity walk: only MeshComponent references
    // other assets (its mesh + per-slot material overrides).
    for (const auto entityHandle : scene->GetAllEntitiesWith<MeshComponent>()) {
        Entity entity{entityHandle, scene};
        const MeshComponent& mesh = entity.GetComponent<MeshComponent>();
        if (static_cast<u64>(mesh.Mesh.Handle()) != c_NullAssetHandle)
            deps.push_back(mesh.Mesh.Handle());
        for (const AssetHandle materialOverride : mesh.MaterialOverrides)
            if (static_cast<u64>(materialOverride) != c_NullAssetHandle)
                deps.push_back(materialOverride);
    }
    return deps;
}

} // namespace Seraph
