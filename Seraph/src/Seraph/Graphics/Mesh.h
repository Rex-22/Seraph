//
// Created by ruben on 5/3/25.
//

#pragma once

#include "Seraph/Core/Base.h"
#include "Material/Material.h"

#include <bgfx/bgfx.h>
#include <concepts>

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

class Mesh
{
public:
    explicit Mesh(const Material& material);
    ~Mesh();

public:
    void SetName(const std::string& name);

    template<HasVertexLayout TVertex>
    void SetVertexLayout()
    {
        m_Layout = TVertex::Layout();
        if (m_Layout->getStride() == 0)
            CORE_WARN("Vertex layout for mesh '{}' has no attributes", m_Name);
    }
    void SetVertexData(const void* data, uint32_t size);
    void SetIndexData(const uint16_t* indices, size_t size);

    [[nodiscard]] const bgfx::VertexLayout* Layout() const { return m_Layout; }
    [[nodiscard]] bgfx::VertexBufferHandle VertexBuffer() const { return m_VertexBuffer; }
    [[nodiscard]] bgfx::IndexBufferHandle IndexBuffer() const { return m_IndexBuffer; }
    [[nodiscard]] const std::string& Name() const { return m_Name; }


    void Submit(uint16_t viewId, Seraph::Transform& transform) const;

private:
    const bgfx::VertexLayout* m_Layout = nullptr;
    const Material* m_Material = nullptr;

    bgfx::VertexBufferHandle m_VertexBuffer { bgfx::kInvalidHandle };
    bgfx::IndexBufferHandle m_IndexBuffer { bgfx::kInvalidHandle };

    std::string m_Name;
};

} // namespace Graphics
