//
// Created by ruben on 2026/07/11.
//

#include "MeshFactory.h"

namespace Seraph
{

// ---- Cube ------------------------------------------------------------------
// 24 vertices (4 per face) so each face can have independent UVs.
// Indices form 12 triangles (2 per face × 6 faces).

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

Ref<Mesh> MeshFactory::CreateCube(const Material& material)
{
    auto mesh = Ref<Mesh>::Create(material);
    mesh->SetName("Cube");
    mesh->SetVertexLayout<PrimitiveVertex>();
    mesh->SetVertexData(s_CubeVertices, sizeof(s_CubeVertices));
    mesh->SetIndexData(s_CubeIndices, sizeof(s_CubeIndices));
    return mesh;
}

// ---- Plane -----------------------------------------------------------------
// A unit quad in the XZ plane (y = 0), centered at origin, facing +Y.

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

Ref<Mesh> MeshFactory::CreatePlane(const Material& material)
{
    auto mesh = Ref<Mesh>::Create(material);
    mesh->SetName("Plane");
    mesh->SetVertexLayout<PrimitiveVertex>();
    mesh->SetVertexData(s_PlaneVertices, sizeof(s_PlaneVertices));
    mesh->SetIndexData(s_PlaneIndices, sizeof(s_PlaneIndices));
    return mesh;
}

} // namespace Seraph
