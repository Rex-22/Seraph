//
// Serializer for Mesh, backed by Assimp (OBJ / glTF). Phase 1 imports the model
// and stages CPU vertex/index data on any thread; Phase 2 uploads the GPU
// buffers and assigns a material on the main thread.
//
// Material handling is intentionally simple for now: every imported mesh gets a
// shared default material. Per-mesh / per-submesh materials from the source file
// are a documented follow-up.
//

#pragma once

#include "Seraph/Asset/AssetSerializer.h"
#include "Seraph/Core/Ref.h"

namespace Seraph
{

class Material;
class Texture2D;

class MeshSerializer : public AssetSerializer
{
public:
    MeshSerializer() = default;
    // Defined in the .cpp so the Ref<Material>/Ref<Texture2D> members' destructors
    // are instantiated where those types are complete.
    ~MeshSerializer() override;

    Ref<Asset> LoadData(const AssetMetadata& metadata, const Buffer& bytes) override;
    void Finalize(const Ref<Asset>& asset) override;
    [[nodiscard]] bool RequiresFinalize() const override { return true; }

    [[nodiscard]] AssetType GetType() const override { return AssetType::Mesh; }

    // Assign the material given to imported meshes. If left null, a shared
    // default material is created lazily from the "simple" shader.
    void SetDefaultMaterial(Ref<Material> material) { m_DefaultMaterial = material; }

private:
    // Created on the main thread in Finalize, released when the serializer is
    // destroyed at AssetImporter::Shutdown (before bgfx is torn down).
    Ref<Material> GetOrCreateDefaultMaterial();

    Ref<Material> m_DefaultMaterial;
    Ref<Texture2D> m_DefaultTexture;
};

} // namespace Seraph
