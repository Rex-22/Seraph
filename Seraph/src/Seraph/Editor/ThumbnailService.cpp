#include "ThumbnailService.h"

#include "Seraph/Asset/Asset.h"
#include "Seraph/Asset/AssetManager.h"
#include "Seraph/Graphics/Texture2D.h"

namespace Seraph
{

std::optional<bgfx::TextureHandle> ThumbnailService::GetThumbnail(AssetHandle handle)
{
    if (AssetManager::GetAssetType(handle) != AssetType::Texture2D)
        return std::nullopt;

    // Resolving the asset caches it; a valid GPU handle only exists once the
    // texture has finished its two-phase load + upload.
    Ref<Texture2D> texture = AssetManager::GetAsset<Texture2D>(handle);
    if (texture && texture->IsValid())
        return texture->Handle();

    return std::nullopt;
}

} // namespace Seraph
