#include "ShaderSerializer.h"

#include "Seraph/Core/Log.h"
#include "Seraph/Graphics/ShaderAsset.h"

namespace Seraph
{

Ref<Asset> ShaderSerializer::LoadData(
    const AssetMetadata& metadata, const Buffer& /*bytes*/)
{
    // File/pack-based compiled shaders are not wired up yet: a program needs a
    // vertex + fragment stage, which requires a container format that pairs the
    // two compiled blobs. Embedded shaders are the only current source and are
    // registered as memory assets by ShaderManager, bypassing this path.
    SP_CORE_WARN_TAG(
        "ShaderManager",
        "File-based shader loading is not implemented yet ('{}'); embedded "
        "shaders are provided by ShaderManager as memory assets.",
        metadata.FilePath.string());
    return nullptr;
}

void ShaderSerializer::Finalize(const Ref<Asset>& asset)
{
    if (Ref<ShaderAsset> shader = asset.As<ShaderAsset>())
        shader->Upload();
}

} // namespace Seraph
