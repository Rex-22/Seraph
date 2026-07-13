//
// Serializer for Mesh. Handles two byte formats behind one AssetType::Mesh:
//
//   * .smesh — the engine's native binary mesh (magic 'SMSH'): a self-describing
//     header (vertex layout + submesh table + material-slot count) followed by
//     the raw vertex and index blobs. This is what Serialize writes and what the
//     editor/runtime load for meshes authored in-engine. Lossless.
//   * import formats (.obj/.gltf/.glb/.fbx) — parsed via Assimp into the same
//     in-memory Mesh (one submesh per source mesh). Import-only; never written.
//
// LoadData dispatches on the leading magic and is fully self-describing (it never
// reads metadata.FilePath), so packed meshes load with empty metadata. Phase 1
// stages CPU vertex/index data; Phase 2 (Finalize) uploads the GPU buffers.
//

#pragma once

#include "Seraph/Asset/AssetSerializer.h"
#include "Seraph/Core/Ref.h"

namespace Seraph
{

class MeshSerializer : public AssetSerializer
{
public:
    MeshSerializer() = default;
    ~MeshSerializer() override;

    Ref<Asset> LoadData(const AssetMetadata& metadata, const Buffer& bytes) override;
    void Finalize(const Ref<Asset>& asset) override;
    [[nodiscard]] bool RequiresFinalize() const override { return true; }

    // Emits the native .smesh binary from a Mesh's retained CPU data + layout +
    // submesh table. Used by EditorAssetManager::SaveAsset / SaveAssetAs.
    bool Serialize(
        const AssetMetadata& metadata, const Ref<Asset>& asset, Buffer& out) override;

    [[nodiscard]] AssetType GetType() const override { return AssetType::Mesh; }
};

} // namespace Seraph
