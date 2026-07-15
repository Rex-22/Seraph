//
// Created by ruben on 5/3/25.
//
// A Mesh is a pure GPU resource + geometry metadata: one vertex buffer, one
// index buffer, a vertex layout, a table of submeshes (drawable index ranges,
// each bound to a material slot), and a material-slot count. It owns no material
// and does not know how to draw itself — binding, material resolution and
// submission live in the renderer. CPU-side vertex/index bytes are retained so
// the mesh can be serialized to a .smesh file.
//

#pragma once

#include "Seraph/Asset/Asset.h"
#include "Seraph/Core/Base.h"
#include "Seraph/Core/Log.h"
#include "Seraph/Core/Ref.h"

#include <bgfx/bgfx.h>
#include <concepts>
#include <cstdint>
#include <string>
#include <vector>

namespace Seraph
{

template<typename TVertex>
concept HasVertexLayout = requires
{
    { TVertex::Layout() } -> std::convertible_to<const bgfx::VertexLayout*>;
};

class Mesh : public Asset
{
public:
    ASSET_CLASS_TYPE(Mesh)

    // One drawable range within the shared vertex/index buffers, bound to a
    // material slot. Indices are absolute (they already include BaseVertex),
    // matching how the importer concatenates sources.
    struct Submesh
    {
        u32 BaseVertex = 0;
        u32 BaseIndex = 0;
        u32 IndexCount = 0;
        u32 MaterialSlot = 0;
    };

    Mesh() = default;
    ~Mesh() override;

    void SetName(const std::string& name);
    [[nodiscard]] const std::string& Name() const { return m_Name; }

    template<HasVertexLayout TVertex>
    void SetVertexLayout()
    {
        m_Layout = TVertex::Layout();
        if (m_Layout->getStride() == 0)
            SP_CORE_WARN_TAG("Mesh", "Vertex layout for mesh '{}' has no attributes", m_Name);
    }

    // Runtime layout (e.g. an imported / deserialized mesh); the mesh copies and
    // owns it.
    void SetVertexLayout(const bgfx::VertexLayout& layout);

    // Immediate upload (main thread), used by procedural/factory meshes. Both
    // retain a CPU copy so the mesh can be serialized. `indexSize` is bytes per
    // index (2 or 4).
    void SetVertexData(const void* data, u32 byteSize);
    void SetIndexData(const void* data, u32 byteSize, u32 indexSize = sizeof(u16));

    // Two-phase (async loading): stage CPU bytes on a worker thread, then Upload
    // the GPU buffers on the main thread. The CPU copy is retained after upload.
    // Both sizes are in BYTES (was: index count — normalized here).
    void StageVertexData(const void* data, u32 byteSize);
    void StageIndexData(const void* data, u32 byteSize, u32 indexSize = sizeof(u16));
    bool Upload();

    void SetSubmeshes(std::vector<Submesh> submeshes) { m_Submeshes = std::move(submeshes); }
    void SetMaterialSlotCount(u32 count) { m_MaterialSlotCount = count; }

    // Per-slot default material handles baked into the mesh (index == slot).
    // Empty, or a null handle at a slot, means "no baked default" — the renderer
    // falls back to the engine default material.
    void SetMaterialSlotDefaults(std::vector<AssetHandle> defaults)
    {
        m_MaterialSlotDefaults = std::move(defaults);
    }
    [[nodiscard]] AssetHandle MaterialSlotDefault(u32 slot) const
    {
        return slot < m_MaterialSlotDefaults.size() ? m_MaterialSlotDefaults[slot]
                                                    : AssetHandle(c_NullAssetHandle);
    }

    // --- Accessors --------------------------------------------------------
    [[nodiscard]] const bgfx::VertexLayout* Layout() const { return m_Layout; }
    [[nodiscard]] bgfx::VertexBufferHandle VertexBuffer() const { return m_VertexBuffer; }
    [[nodiscard]] bgfx::IndexBufferHandle IndexBuffer() const { return m_IndexBuffer; }
    [[nodiscard]] const std::vector<Submesh>& Submeshes() const { return m_Submeshes; }
    [[nodiscard]] u32 MaterialSlotCount() const { return m_MaterialSlotCount; }
    [[nodiscard]] const std::vector<AssetHandle>& MaterialSlotDefaults() const
    {
        return m_MaterialSlotDefaults;
    }
    [[nodiscard]] u32 IndexSize() const { return m_IndexSize; }

    // CPU-side geometry, retained for serialization.
    [[nodiscard]] const std::vector<u8>& VertexData() const { return m_Vertices; }
    [[nodiscard]] const std::vector<u8>& IndexData() const { return m_Indices; }
    [[nodiscard]] u32 VertexCount() const
    {
        const u32 stride = m_Layout != nullptr ? m_Layout->getStride() : 0;
        return stride != 0 ? static_cast<u32>(m_Vertices.size()) / stride : 0;
    }
    [[nodiscard]] u32 IndexCount() const
    {
        return m_IndexSize != 0 ? static_cast<u32>(m_Indices.size()) / m_IndexSize : 0;
    }

    // Retained CPU geometry (kept after upload for serialization); the GPU
    // buffers mirror it, so this is a reasonable footprint estimate.
    [[nodiscard]] u64 GetMemoryFootprint() const override
    {
        return static_cast<u64>(m_Vertices.size()) + m_Indices.size();
    }

    // The mesh's baked per-slot default materials.
    [[nodiscard]] std::vector<AssetHandle> GetDependencies() const override
    {
        std::vector<AssetHandle> deps;
        for (const AssetHandle h : m_MaterialSlotDefaults)
            if (h != c_NullAssetHandle)
                deps.push_back(h);
        return deps;
    }

private:
    bool CreateBuffers();

    const bgfx::VertexLayout* m_Layout = nullptr;
    bgfx::VertexLayout m_OwnedLayout{}; // used when a runtime layout is set

    bgfx::VertexBufferHandle m_VertexBuffer{bgfx::kInvalidHandle};
    bgfx::IndexBufferHandle m_IndexBuffer{bgfx::kInvalidHandle};

    // Retained CPU geometry — kept after upload so the mesh can be serialized.
    std::vector<u8> m_Vertices;
    std::vector<u8> m_Indices;
    u32 m_IndexSize = sizeof(u16); // bytes per index (2 or 4)

    std::vector<Submesh> m_Submeshes;
    u32 m_MaterialSlotCount = 1;
    std::vector<AssetHandle> m_MaterialSlotDefaults; // index == slot; may be empty

    std::string m_Name = "NoName";
};

} // namespace Seraph
