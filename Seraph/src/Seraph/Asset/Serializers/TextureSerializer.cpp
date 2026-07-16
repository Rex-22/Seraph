#include "TextureSerializer.h"

#include "Seraph/Graphics/Texture2D.h"

namespace Seraph
{

Ref<Asset> TextureSerializer::LoadData(const AssetMetadata& metadata, const Buffer& bytes)
{
    if (!bytes)
        return nullptr;

    // Phase 1: CPU parse only (worker-safe). No GPU texture yet.
    Ref<Texture2D> texture = Texture2D::ParseEncoded(
        metadata.FilePath.string().c_str(), bytes.Data(), bytes.Size());
    return texture;
}

void TextureSerializer::Finalize(const Ref<Asset>& asset)
{
    if (Ref<Texture2D> texture = asset.As<Texture2D>())
        texture->Upload();
}

} // namespace Seraph
