//
// Created by ruben on 5/3/25.
//

#include "Mesh.h"

#include "Camera.h"
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

void Mesh::SetIndexData(const uint16_t* indices, const uint32_t size)
{
    if (bgfx::isValid(m_IndexBuffer)) {
        bgfx::destroy(m_IndexBuffer);
    }
    m_IndexBuffer = bgfx::createIndexBuffer(bgfx::makeRef(indices, size));
}

void Mesh::Submit(
    uint8_t viewId, Camera& camera, uint64_t state) const
{
    if (BGFX_STATE_MASK == state)
    {
        state = 0
            | BGFX_STATE_WRITE_RGB
            | BGFX_STATE_WRITE_A
            | BGFX_STATE_WRITE_Z
            | BGFX_STATE_DEPTH_TEST_LESS
            | BGFX_STATE_CULL_CCW
            | BGFX_STATE_MSAA
            ;
    }

    bgfx::setState(state);

    bgfx::setViewTransform(
        viewId, glm::value_ptr(camera.ViewMatrix()),
        glm::value_ptr(camera.ProjectionMatrix())
    );

    bgfx::setVertexBuffer(0, VertexBuffer());
    bgfx::setIndexBuffer(IndexBuffer());

    m_Material->Apply(viewId, BGFX_DISCARD_INDEX_BUFFER | BGFX_DISCARD_VERTEX_STREAMS);
    bgfx::discard();
}

} // namespace Graphics