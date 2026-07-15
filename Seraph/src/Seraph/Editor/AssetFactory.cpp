#include "AssetFactory.h"

#include "Seraph/Asset/AssetManager.h"
#include "Seraph/Asset/EditorAssetManager.h"
#include "Seraph/Core/FileSystem.h"
#include "Seraph/Graphics/Material/Material.h"
#include "Seraph/Graphics/Material/MaterialInstance.h"

#include <string>

namespace Seraph
{

namespace
{

// A unique "<base>", "<base> 2", ... path under `dir` that clashes with neither
// an existing file nor a registered asset.
std::filesystem::path UniquePath(
    EditorAssetManager& ed, const std::filesystem::path& dir,
    const std::string& base, const std::string& ext)
{
    for (int i = 0;; ++i) {
        const std::string name = i == 0 ? base : base + " " + std::to_string(i + 1);
        const std::filesystem::path rel = dir / (name + ext);
        if (!FileSystem::Exists(Root::Project, rel) &&
            static_cast<u64>(ed.GetAssetHandleFromFilePath(rel)) == c_NullAssetHandle)
            return rel;
    }
}

} // namespace

AssetHandle CreateMaterialAsset(const std::filesystem::path& dir)
{
    Ref<EditorAssetManager> ed = AssetManager::Get().As<EditorAssetManager>();
    if (!ed)
        return c_NullAssetHandle;

    Ref<Material> material = Material::CreateDefault();
    const std::filesystem::path rel = UniquePath(*ed, dir, "New Material", ".smaterial");
    return ed->SaveAssetAs(material, rel);
}

AssetHandle CreateMaterialInstanceAsset(const std::filesystem::path& dir)
{
    Ref<EditorAssetManager> ed = AssetManager::Get().As<EditorAssetManager>();
    if (!ed)
        return c_NullAssetHandle;

    Ref<MaterialInstance> instance = Ref<MaterialInstance>::Create();
    const std::filesystem::path rel =
        UniquePath(*ed, dir, "New Material Instance", ".smatinst");
    return ed->SaveAssetAs(instance, rel);
}

} // namespace Seraph
