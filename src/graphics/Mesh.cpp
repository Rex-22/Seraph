//
// Created by ruben on 5/3/25.
//

#include "Mesh.h"

namespace Graphics
{

Mesh::Mesh()
{
}

Mesh::~Mesh()
{
    if (bgfx::isValid(m_VertexBuffer)) {
        bgfx::destroy(m_VertexBuffer);
    }
    if (bgfx::isValid(m_IndexBuffer)) {
        bgfx::destroy(m_IndexBuffer);
    }
}

void Mesh::SetVertexData(
    const void* data, const uint32_t size, const bgfx::VertexLayout& layout)
{
    if (bgfx::isValid(m_VertexBuffer)) {
        bgfx::destroy(m_VertexBuffer);
    }

    m_VertexBuffer =
        bgfx::createVertexBuffer(bgfx::makeRef(data, size), layout);
    m_Layout = layout;
}

void Mesh::SetIndexData(const uint16_t* indices, const uint32_t size)
{
    if (bgfx::isValid(m_IndexBuffer)) {
        bgfx::destroy(m_IndexBuffer);
    }
    m_IndexBuffer = bgfx::createIndexBuffer(bgfx::makeRef(indices, size));
}

} // namespace Graphics