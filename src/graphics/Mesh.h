//
// Created by ruben on 5/3/25.
//

#ifndef MESH_H
#define MESH_H
#include "material/Material.h"

#include <bgfx/bgfx.h>

namespace Core
{
class Transform;
}
namespace Graphics
{
class Camera;
class Material;
class Mesh
{
public:
    explicit Mesh(const Material& material);
    ~Mesh();

public:
    void SetVertexData(
        const void* data, uint32_t size, const bgfx::VertexLayout& layout);
    void SetIndexData(const uint16_t* indices, size_t size);

    bgfx::VertexLayout Layout() const { return m_Layout; }
    bgfx::VertexBufferHandle VertexBuffer() const { return m_VertexBuffer; }
    bgfx::IndexBufferHandle IndexBuffer() const { return m_IndexBuffer; }

    void Submit(uint16_t viewId, Core::Transform& transform) const;

private:
    bgfx::VertexLayout m_Layout;
    const Material* m_Material;

    bgfx::VertexBufferHandle m_VertexBuffer { bgfx::kInvalidHandle };
    bgfx::IndexBufferHandle m_IndexBuffer { bgfx::kInvalidHandle };
};

} // namespace Graphics

#endif // MESH_H
