//
// Created by ruben on 5/3/25.
//

#pragma once

#include "Material/Material.h"
#include "Seraph/Asset/Asset.h"
#include "Seraph/Core/Base.h"
#include "Seraph/Core/Ref.h"

#include <bgfx/bgfx.h>
#include <concepts>
#include <cstdint>
#include <vector>

namespace Seraph
{
class Transform;
}

namespace Seraph
{
class Camera;
class Material;

template<typename TVertex>
concept HasVertexLayout = requires
{
    { TVertex::Layout() } -> std::convertible_to<const bgfx::VertexLayout*>;
};

class Mesh: public Asset
{
public:
    ASSET_CLASS_TYPE(Mesh)

    explicit Mesh(const Ref<Material>& material);
    ~Mesh() override;

public:
    void SetName(const std::string& name);
    void SetMaterial(const Ref<Material>& material) { m_Material = material; }

    template<HasVertexLayout TVertex>
    void SetVertexLayout()
    {
        m_Layout = TVertex::Layout();
        if (m_Layout->getStride() == 0)
            SP_CORE_WARN_TAG("Mesh", "Vertex layout for mesh '{}' has no attributes", m_Name);
    }

    // Runtime layout (e.g. an imported mesh); the mesh copies and owns it.
    void SetVertexLayout(const bgfx::VertexLayout& layout);

    // Immediate upload (main thread). Used by procedural/factory meshes.
    void SetVertexData(const void* data, uint32_t size);
    void SetIndexData(const uint16_t* indices, size_t size);

    // Two-phase (asset loading): stage CPU bytes on a worker thread, then Upload
    // the GPU buffers on the main thread.
    void StageVertexData(const void* data, uint32_t size);
    void StageIndexData(const uint16_t* indices, uint32_t count);
    bool Upload();

    [[nodiscard]] const bgfx::VertexLayout* Layout() const { return m_Layout; }
    [[nodiscard]] bgfx::VertexBufferHandle VertexBuffer() const { return m_VertexBuffer; }
    [[nodiscard]] bgfx::IndexBufferHandle IndexBuffer() const { return m_IndexBuffer; }
    [[nodiscard]] const std::string& Name() const { return m_Name; }

    void Submit(uint16_t viewId, const glm::mat4& transform = glm::mat4(1.0f)) const;

private:
    const bgfx::VertexLayout* m_Layout = nullptr;
    bgfx::VertexLayout m_OwnedLayout{}; // used when a runtime layout is set
    Ref<Material> m_Material;

    bgfx::VertexBufferHandle m_VertexBuffer { bgfx::kInvalidHandle };
    bgfx::IndexBufferHandle m_IndexBuffer { bgfx::kInvalidHandle };

    // CPU data staged between StageVertexData/StageIndexData and Upload.
    std::vector<u8> m_StagedVertices;
    std::vector<u16> m_StagedIndices;

    std::string m_Name;
};

} // namespace Graphics
