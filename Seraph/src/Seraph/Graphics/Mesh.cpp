//
// Created by ruben on 5/3/25.
//

#include "Mesh.h"

#include "Seraph/Core/Log.h"
#include "Seraph/Graphics/Material/Material.h"
#include "glm/gtc/type_ptr.hpp"

#include <cstring>

namespace Seraph
{

Mesh::Mesh(const Ref<Material>& material) : m_Material(material)
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

void Mesh::SetVertexLayout(const bgfx::VertexLayout& layout)
{
    m_OwnedLayout = layout;
    m_Layout = &m_OwnedLayout;
    if (m_Layout->getStride() == 0)
        SP_CORE_WARN_TAG("Mesh", "Vertex layout for mesh '{}' has no attributes", m_Name);
}

void Mesh::SetVertexData(const void* data, const uint32_t size)
{
    if (bgfx::isValid(m_VertexBuffer)) {
        bgfx::destroy(m_VertexBuffer);
    }

    if (m_Layout == nullptr) {
        SP_CORE_ERROR_TAG("Mesh", "Vertex data for mesh '{}' set with no layout", m_Name);
        return;
    }

    // Copy so bgfx owns the memory — the caller's buffer need not outlive us.
    m_VertexBuffer =
        bgfx::createVertexBuffer(bgfx::copy(data, size), *m_Layout);
}

void Mesh::SetIndexData(const uint16_t* indices, const size_t size)
{
    if (bgfx::isValid(m_IndexBuffer)) {
        bgfx::destroy(m_IndexBuffer);
    }
    m_IndexBuffer =
        bgfx::createIndexBuffer(bgfx::copy(indices, static_cast<uint32_t>(size)));
}

void Mesh::StageVertexData(const void* data, const uint32_t size)
{
    m_StagedVertices.resize(size);
    if (size > 0 && data != nullptr)
        std::memcpy(m_StagedVertices.data(), data, size);
}

void Mesh::StageIndexData(const uint16_t* indices, const uint32_t count)
{
    m_StagedIndices.assign(indices, indices + count);
}

bool Mesh::Upload()
{
    if (m_Layout == nullptr) {
        SP_CORE_ERROR_TAG("Mesh", "Cannot upload mesh '{}' with no layout", m_Name);
        return false;
    }
    if (m_StagedVertices.empty() || m_StagedIndices.empty()) {
        SP_CORE_ERROR_TAG("Mesh", "Cannot upload mesh '{}' with no staged data", m_Name);
        return false;
    }

    if (bgfx::isValid(m_VertexBuffer))
        bgfx::destroy(m_VertexBuffer);
    if (bgfx::isValid(m_IndexBuffer))
        bgfx::destroy(m_IndexBuffer);

    m_VertexBuffer = bgfx::createVertexBuffer(
        bgfx::copy(m_StagedVertices.data(),
                   static_cast<uint32_t>(m_StagedVertices.size())),
        *m_Layout);
    m_IndexBuffer = bgfx::createIndexBuffer(
        bgfx::copy(m_StagedIndices.data(),
                   static_cast<uint32_t>(m_StagedIndices.size() * sizeof(u16))));

    // Staged CPU data no longer needed once uploaded.
    m_StagedVertices.clear();
    m_StagedVertices.shrink_to_fit();
    m_StagedIndices.clear();
    m_StagedIndices.shrink_to_fit();

    return bgfx::isValid(m_VertexBuffer) && bgfx::isValid(m_IndexBuffer);
}

void Mesh::Submit(u16 viewId, const glm::mat4& transform) const
{
    bgfx::setTransform(glm::value_ptr(transform));

    bgfx::setVertexBuffer(0, VertexBuffer());
    bgfx::setIndexBuffer(IndexBuffer());

    m_Material->Apply(
        viewId, BGFX_DISCARD_INDEX_BUFFER | BGFX_DISCARD_VERTEX_STREAMS);
    bgfx::discard();
}

} // namespace Graphics