//
// Created by ruben on 5/3/25.
//

#include "Mesh.h"

namespace Graphics
{

Mesh::Mesh() : m_VertexBuffer(), m_IndexBufferHandle()
{
}

Mesh::~Mesh()
{
    bgfx::destroy(m_VertexBuffer);
    bgfx::destroy(m_IndexBufferHandle);
}

Mesh& Mesh::SetVertexData(
    const void* data, const uint32_t size, const bgfx::VertexLayout& layout)
{
    bgfx::destroy(m_VertexBuffer);

    m_VertexBuffer =
        bgfx::createVertexBuffer(bgfx::makeRef(data, size), layout);
    m_Layout = layout;

    return *this;
}

Mesh& Mesh::SetIndexData(const uint16_t* indices, const uint32_t size)
{
    bgfx::destroy(m_IndexBufferHandle);

    m_IndexBufferHandle = bgfx::createIndexBuffer(bgfx::makeRef(indices, size));
    return *this;
}

} // namespace Graphics