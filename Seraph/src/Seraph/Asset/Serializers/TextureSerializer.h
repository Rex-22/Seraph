//
// Serializer for Texture2D. Phase 1 parses encoded image bytes on any thread;
// Phase 2 uploads the GPU texture on the main thread.
//

#pragma once

#include "Seraph/Asset/AssetSerializer.h"

namespace Seraph
{

class TextureSerializer : public AssetSerializer
{
public:
    Ref<Asset> LoadData(const AssetMetadata& metadata, const Buffer& bytes) override;
    void Finalize(const Ref<Asset>& asset) override;
    [[nodiscard]] bool RequiresFinalize() const override { return true; }

    // Textures are source-only: the imported png/jpg on disk is the authoritative
    // form, so there is nothing to re-encode on save.
    [[nodiscard]] bool CanSerialize() const override { return false; }

    [[nodiscard]] AssetType GetType() const override { return AssetType::Texture2D; }
};

} // namespace Seraph
