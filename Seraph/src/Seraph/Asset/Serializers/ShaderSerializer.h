//
// Serializer for ShaderAsset — the file/pack path for compiled shader programs.
//
// NOTE: embedded shaders do NOT go through this serializer; ShaderManager builds
// them directly as memory assets (see ShaderManager::GetHandle). This exists so
// AssetType::Shader has a registered handler and so loading compiled shader
// binaries from disk/pack has a home. A single compiled blob is one stage, while
// a program needs both vertex+fragment, so a container format is still to be
// defined — LoadData is a stub until then.
//

#pragma once

#include "Seraph/Asset/AssetSerializer.h"

namespace Seraph
{

class ShaderSerializer : public AssetSerializer
{
public:
    Ref<Asset> LoadData(const AssetMetadata& metadata, const Buffer& bytes) override;
    void Finalize(const Ref<Asset>& asset) override;
    [[nodiscard]] bool RequiresFinalize() const override { return true; }

    [[nodiscard]] AssetType GetType() const override { return AssetType::Shader; }
};

} // namespace Seraph
