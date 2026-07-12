//
// Serializer for ShaderAsset — the file/pack path for compiled shader programs.
//
// A program needs both a vertex and a fragment stage, while a single bgfx
// compiled blob is one stage, so a ".sshader" is a small container pairing the
// two compiled blobs:
//
//   [ magic "SSHD" | u32 version | u32 vsSize | u32 fsSize ] [ vs bytes ][ fs bytes ]
//
// LoadData parses the container and stages the blobs; Finalize uploads the bgfx
// program on the main thread. Serialize writes the container back out (used to
// cook embedded shaders to disk / into a pack — see ShaderManager::ExportEmbeddedShader).
//
// The blobs are per-renderer bgfx binaries, so a .sshader is valid for the
// renderer it was compiled/cooked for (a build artifact, not portable).
//
// Embedded shaders do NOT go through this serializer; ShaderManager builds them
// directly as memory assets (see ShaderManager::GetHandle).
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

    // Writes the ShaderAsset's staged vs/fs bytecode into a .sshader container.
    // Only valid before the asset is uploaded (Upload releases the CPU bytes).
    bool Serialize(
        const AssetMetadata& metadata, const Ref<Asset>& asset, Buffer& out) override;

    [[nodiscard]] AssetType GetType() const override { return AssetType::Shader; }
};

} // namespace Seraph
