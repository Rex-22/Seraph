//
// Created by ruben on 5/3/25.
//

#include "Mesh.h"

#include "core/Log.h"
#include "core/Transform.h"
#include "glm/gtc/type_ptr.hpp"
#include "material/Material.h"

namespace Graphics
{

Mesh::Mesh(const Material& material) : m_Material(&material)
{
    m_Name = "NoName";
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
void Mesh::SetName(const std::string& name)
{
    m_Name = name;
}

void Mesh::SetVertexData(const void* data, const uint32_t size)
{
    if (bgfx::isValid(m_VertexBuffer)) {
        bgfx::destroy(m_VertexBuffer);
    }

    if (m_Layout == nullptr) {
        CORE_ERROR("Vertex layout for mesh '{}' has no layout", m_Name);
        return;
    }

    m_VertexBuffer =
        bgfx::createVertexBuffer(bgfx::makeRef(data, size), *m_Layout);
}

void Mesh::SetIndexData(const uint16_t* indices, const size_t size)
{
    if (bgfx::isValid(m_IndexBuffer)) {
        bgfx::destroy(m_IndexBuffer);
    }
    m_IndexBuffer = bgfx::createIndexBuffer(bgfx::makeRef(indices, size));
}

void Mesh::Submit(uint16_t viewId, Core::Transform& transform) const
{
    bgfx::setTransform(glm::value_ptr(transform.TransformMatrix()));

    bgfx::setVertexBuffer(0, VertexBuffer());
    bgfx::setIndexBuffer(IndexBuffer());

    m_Material->Apply(
        viewId, BGFX_DISCARD_INDEX_BUFFER | BGFX_DISCARD_VERTEX_STREAMS);
    bgfx::discard();
}

} // namespace Graphics