//
// Created by ruben on 2026/07/11.
//

#include "MeshFactory.h"

#include <vector>

namespace Seraph
{

// ---- Cube ------------------------------------------------------------------
// 24 vertices (4 per face) so each face can have independent UVs.
// Indices form 12 triangles (2 per face × 6 faces).
// Base positions are the unit-sign corners (±1); CreateCube scales them by the
// requested half-extents.

static const PrimitiveVertex s_CubeVertices[] =
{
    // +Z (front)
    {-1.f,  1.f,  1.f, 0xffffffff, 0.f, 0.f},
    { 1.f,  1.f,  1.f, 0xffffffff, 1.f, 0.f},
    {-1.f, -1.f,  1.f, 0xffffffff, 0.f, 1.f},
    { 1.f, -1.f,  1.f, 0xffffffff, 1.f, 1.f},

    // -Z (back)
    { 1.f,  1.f, -1.f, 0xffffffff, 0.f, 0.f},
    {-1.f,  1.f, -1.f, 0xffffffff, 1.f, 0.f},
    { 1.f, -1.f, -1.f, 0xffffffff, 0.f, 1.f},
    {-1.f, -1.f, -1.f, 0xffffffff, 1.f, 1.f},

    // -X (left)
    {-1.f,  1.f, -1.f, 0xffffffff, 0.f, 0.f},
    {-1.f,  1.f,  1.f, 0xffffffff, 1.f, 0.f},
    {-1.f, -1.f, -1.f, 0xffffffff, 0.f, 1.f},
    {-1.f, -1.f,  1.f, 0xffffffff, 1.f, 1.f},

    // +X (right)
    { 1.f,  1.f,  1.f, 0xffffffff, 0.f, 0.f},
    { 1.f,  1.f, -1.f, 0xffffffff, 1.f, 0.f},
    { 1.f, -1.f,  1.f, 0xffffffff, 0.f, 1.f},
    { 1.f, -1.f, -1.f, 0xffffffff, 1.f, 1.f},

    // +Y (top)
    {-1.f,  1.f, -1.f, 0xffffffff, 0.f, 0.f},
    { 1.f,  1.f, -1.f, 0xffffffff, 1.f, 0.f},
    {-1.f,  1.f,  1.f, 0xffffffff, 0.f, 1.f},
    { 1.f,  1.f,  1.f, 0xffffffff, 1.f, 1.f},

    // -Y (bottom)
    {-1.f, -1.f,  1.f, 0xffffffff, 0.f, 0.f},
    { 1.f, -1.f,  1.f, 0xffffffff, 1.f, 0.f},
    {-1.f, -1.f, -1.f, 0xffffffff, 0.f, 1.f},
    { 1.f, -1.f, -1.f, 0xffffffff, 1.f, 1.f},
};

static const uint16_t s_CubeIndices[] =
{
     2,  1,  0,   2,  3,  1,  // +Z
     6,  5,  4,   6,  7,  5,  // -Z
    10,  9,  8,  10, 11,  9,  // -X
    14, 13, 12,  14, 15, 13,  // +X
    18, 17, 16,  18, 19, 17,  // +Y
    22, 21, 20,  22, 23, 21,  // -Y
};

Ref<Mesh> MeshFactory::CreateCube(const CubeParams& params)
{
    const glm::vec3 e = params.HalfExtents;

    std::vector<PrimitiveVertex> vertices(
        std::begin(s_CubeVertices), std::end(s_CubeVertices));
    for (PrimitiveVertex& v : vertices) {
        v.x *= e.x;
        v.y *= e.y;
        v.z *= e.z;
    }

    auto mesh = Ref<Mesh>::Create();
    mesh->SetName("Cube");
    mesh->SetVertexLayout<PrimitiveVertex>();
    mesh->SetVertexData(
        vertices.data(), static_cast<u32>(vertices.size() * sizeof(PrimitiveVertex)));
    mesh->SetIndexData(s_CubeIndices, sizeof(s_CubeIndices), sizeof(uint16_t));

    constexpr u32 indexCount = sizeof(s_CubeIndices) / sizeof(s_CubeIndices[0]);
    mesh->SetSubmeshes({Mesh::Submesh{0, 0, indexCount, 0}});
    mesh->SetMaterialSlotCount(1);
    return mesh;
}

// ---- Plane -----------------------------------------------------------------
// A quad in the XZ plane (y = 0), centered at origin, facing +Y. Base positions
// are the unit-sign corners; CreatePlane scales X/Z by the requested half-extents.

static const PrimitiveVertex s_PlaneVertices[] =
{
    {-1.f, 0.f, -1.f, 0xffffffff, 0.f, 0.f},
    { 1.f, 0.f, -1.f, 0xffffffff, 1.f, 0.f},
    {-1.f, 0.f,  1.f, 0xffffffff, 0.f, 1.f},
    { 1.f, 0.f,  1.f, 0xffffffff, 1.f, 1.f},
};

static const uint16_t s_PlaneIndices[] =
{
    2, 1, 0,
    2, 3, 1,
};

Ref<Mesh> MeshFactory::CreatePlane(const PlaneParams& params)
{
    const glm::vec2 e = params.HalfExtents;

    std::vector<PrimitiveVertex> vertices(
        std::begin(s_PlaneVertices), std::end(s_PlaneVertices));
    for (PrimitiveVertex& v : vertices) {
        v.x *= e.x;
        v.z *= e.y;
    }

    auto mesh = Ref<Mesh>::Create();
    mesh->SetName("Plane");
    mesh->SetVertexLayout<PrimitiveVertex>();
    mesh->SetVertexData(
        vertices.data(), static_cast<u32>(vertices.size() * sizeof(PrimitiveVertex)));
    mesh->SetIndexData(s_PlaneIndices, sizeof(s_PlaneIndices), sizeof(uint16_t));

    constexpr u32 indexCount = sizeof(s_PlaneIndices) / sizeof(s_PlaneIndices[0]);
    mesh->SetSubmeshes({Mesh::Submesh{0, 0, indexCount, 0}});
    mesh->SetMaterialSlotCount(1);
    return mesh;
}

} // namespace Seraph
