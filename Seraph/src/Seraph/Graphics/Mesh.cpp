//
// Created by ruben on 5/3/25.
//

#include "Mesh.h"

#include "Seraph/Asset/AssetRef.h"
#include "Seraph/Core/Log.h"

#include <cstring>

namespace Seraph
{

namespace
{
// Register TAssetRef<Mesh> as a reference type filtered to AssetType::Mesh, so a
// field declared TAssetRef<Mesh> draws a mesh-only asset picker. Done here where
// Mesh is a complete type (RegisterAssetRefType<Mesh>() reads Mesh::GetStaticType()).
const bool k_MeshAssetRefReflected = [] {
    RegisterAssetRefType<Mesh>();
    return true;
}();
} // namespace

Mesh::~Mesh()
{
    if (bgfx::isValid(m_VertexBuffer))
        bgfx::destroy(m_VertexBuffer);
    if (bgfx::isValid(m_IndexBuffer))
        bgfx::destroy(m_IndexBuffer);
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

void Mesh::SetVertexData(const void* data, const u32 byteSize)
{
    m_Vertices.resize(byteSize);
    if (byteSize > 0 && data != nullptr)
        std::memcpy(m_Vertices.data(), data, byteSize);

    if (bgfx::isValid(m_VertexBuffer)) {
        bgfx::destroy(m_VertexBuffer);
        m_VertexBuffer = BGFX_INVALID_HANDLE;
    }

    if (m_Layout == nullptr) {
        SP_CORE_ERROR_TAG("Mesh", "Vertex data for mesh '{}' set with no layout", m_Name);
        return;
    }

    // Copy so bgfx owns the GPU-side memory; our CPU copy in m_Vertices persists
    // for serialization.
    m_VertexBuffer =
        bgfx::createVertexBuffer(bgfx::copy(m_Vertices.data(), byteSize), *m_Layout);
}

void Mesh::SetIndexData(const void* data, const u32 byteSize, const u32 indexSize)
{
    m_IndexSize = indexSize;
    m_Indices.resize(byteSize);
    if (byteSize > 0 && data != nullptr)
        std::memcpy(m_Indices.data(), data, byteSize);

    if (bgfx::isValid(m_IndexBuffer)) {
        bgfx::destroy(m_IndexBuffer);
        m_IndexBuffer = BGFX_INVALID_HANDLE;
    }

    const u16 flags =
        indexSize == sizeof(u32) ? BGFX_BUFFER_INDEX32 : BGFX_BUFFER_NONE;
    m_IndexBuffer =
        bgfx::createIndexBuffer(bgfx::copy(m_Indices.data(), byteSize), flags);
}

void Mesh::StageVertexData(const void* data, const u32 byteSize)
{
    m_Vertices.resize(byteSize);
    if (byteSize > 0 && data != nullptr)
        std::memcpy(m_Vertices.data(), data, byteSize);
}

void Mesh::StageIndexData(const void* data, const u32 byteSize, const u32 indexSize)
{
    m_IndexSize = indexSize;
    m_Indices.resize(byteSize);
    if (byteSize > 0 && data != nullptr)
        std::memcpy(m_Indices.data(), data, byteSize);
}

bool Mesh::Upload()
{
    if (m_Layout == nullptr) {
        SP_CORE_ERROR_TAG("Mesh", "Cannot upload mesh '{}' with no layout", m_Name);
        return false;
    }
    if (m_Vertices.empty() || m_Indices.empty()) {
        SP_CORE_ERROR_TAG("Mesh", "Cannot upload mesh '{}' with no staged data", m_Name);
        return false;
    }
    return CreateBuffers();
}

bool Mesh::CreateBuffers()
{
    if (bgfx::isValid(m_VertexBuffer))
        bgfx::destroy(m_VertexBuffer);
    if (bgfx::isValid(m_IndexBuffer))
        bgfx::destroy(m_IndexBuffer);

    m_VertexBuffer = bgfx::createVertexBuffer(
        bgfx::copy(m_Vertices.data(), static_cast<u32>(m_Vertices.size())), *m_Layout);

    const u16 flags =
        m_IndexSize == sizeof(u32) ? BGFX_BUFFER_INDEX32 : BGFX_BUFFER_NONE;
    m_IndexBuffer = bgfx::createIndexBuffer(
        bgfx::copy(m_Indices.data(), static_cast<u32>(m_Indices.size())), flags);

    return bgfx::isValid(m_VertexBuffer) && bgfx::isValid(m_IndexBuffer);
}

} // namespace Seraph
