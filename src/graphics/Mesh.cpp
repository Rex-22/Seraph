//
// Created by ruben on 5/3/25.
//

#include "Mesh.h"

#include "Camera.h"
#include "core/Transform.h"
#include "glm/gtc/type_ptr.hpp"
#include "material/Material.h"

namespace Graphics
{

Mesh::Mesh(const Material& material) : m_Material(&material)
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

void Mesh::SetIndexData(const uint16_t* indices, const size_t size)
{
    if (bgfx::isValid(m_IndexBuffer)) {
        bgfx::destroy(m_IndexBuffer);
    }
    m_IndexBuffer = bgfx::createIndexBuffer(bgfx::makeRef(indices, size));
}

void Mesh::Submit(
    uint16_t viewId, Core::Transform& transform) const
{
    bgfx::setTransform(glm::value_ptr(transform.TransformMatrix()));

    bgfx::setVertexBuffer(0, VertexBuffer());
    bgfx::setIndexBuffer(IndexBuffer());

    m_Material->Apply(viewId, BGFX_DISCARD_INDEX_BUFFER | BGFX_DISCARD_VERTEX_STREAMS);
    bgfx::discard();
}

} // namespace Graphics