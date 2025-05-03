//
// Created by ruben on 5/3/25.
//

#ifndef MESH_H
#define MESH_H
#include <bgfx/bgfx.h>

namespace Graphics
{
class Mesh
{
public:
    Mesh();
    ~Mesh();

public:
    Mesh& SetVertexData(
        const void* data, uint32_t size, const bgfx::VertexLayout& layout);
    Mesh& SetIndexData(const uint16_t* indices, uint32_t size);

    bgfx::VertexLayout Layout() const { return m_Layout; };
    bgfx::VertexBufferHandle VertexBuffer() const { return m_VertexBuffer; };
    bgfx::IndexBufferHandle IndexBuffer() const { return m_IndexBufferHandle; };

private:
    bgfx::VertexLayout m_Layout;

    bgfx::VertexBufferHandle m_VertexBuffer;
    bgfx::IndexBufferHandle m_IndexBufferHandle;
};

} // namespace Graphics

#endif // MESH_H
