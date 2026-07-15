//
// A scene as a first-class, loadable asset. Wraps a populated Ref<Scene> so a
// scene can be referenced by handle, cached by the AssetManager, and packed like
// any other asset. Pure CPU: the GPU assets its entities reference (meshes,
// textures) resolve lazily through the AssetManager at render time, so the
// SceneSerializer needs no Finalize.
//

#pragma once

#include "Seraph/Asset/Asset.h"
#include "Seraph/Asset/AssetHandle.h"
#include "Seraph/Core/Ref.h"
#include "Seraph/Scene/Scene.h"

#include <vector>

namespace Seraph
{

class SceneAsset : public Asset
{
public:
    ASSET_CLASS_TYPE(Scene)

    SceneAsset() = default;
    explicit SceneAsset(Ref<Scene> scene) : m_Scene(std::move(scene)) {}

    const Ref<Scene>& GetScene() const { return m_Scene; }
    void SetScene(Ref<Scene> scene) { m_Scene = std::move(scene); }

    // Meshes + material overrides referenced by the scene's MeshComponents.
    [[nodiscard]] std::vector<AssetHandle> GetDependencies() const override;

private:
    Ref<Scene> m_Scene;
};

} // namespace Seraph
