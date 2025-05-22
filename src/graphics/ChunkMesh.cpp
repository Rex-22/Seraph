//
// Created by Ruben on 2025/05/22.
//

#include "ChunkMesh.h"

#include "world/Chunk.h"

namespace Graphics
{

using namespace World;

constexpr ChunkMeshFace FRONT_FACE = {1, 1, 1, 0, 1, 1, 0, 0, 1, 1, 0, 1};
constexpr ChunkMeshFace LEFT_FACE = {0, 1, 1, 0, 1, 0, 0, 0, 0, 0, 0, 1};
constexpr ChunkMeshFace BACK_FACE = {0, 1, 0, 1, 1, 0, 1, 0, 0, 0, 0, 0};
constexpr ChunkMeshFace RIGHT_FACE = {1, 1, 0, 1, 1, 1, 1, 0, 1, 1, 0, 0};
constexpr ChunkMeshFace TOP_FACE = {1, 1, 0, 0, 1, 0, 0, 1, 1, 1, 1, 1};
constexpr ChunkMeshFace BOTTOM_FACE = {0, 0, 0, 1, 0, 0, 1, 0, 1, 0, 0, 1};

bgfx::VertexLayout ChunkMesh::ChunkVertex::Layout;

ChunkMesh::~ChunkMesh()
{
    delete m_Mesh;
}

ChunkMesh* ChunkMesh::Create(const Chunk& chunk)
{
    const auto chunkMesh = new ChunkMesh();
    chunkMesh->GenerateMeshData(chunk);
    return chunkMesh;
}

const Mesh* ChunkMesh::GetMesh()
{
    UpdateMesh();
    return m_Mesh;
}

void ChunkMesh::GenerateMeshData(const Chunk& chunk)
{
    m_Dirty = true;
    m_Vertices.clear();
    m_Indices.clear();
    m_IndexCount = 0;
    for (int i = 0; i < ChunkVolume; ++i) {
        const auto blockPos = BlockPosFromIndex(i);
        if (const BlockId currentBlockId = chunk.BlockAt(blockPos);
            currentBlockId == AIR_BLOCK || currentBlockId == INVALID_BLOCK) {
            continue;
        }

        auto bnx = chunk.BlockAt(
            {static_cast<uint16_t>(blockPos.X - 1), blockPos.Y, blockPos.Z});
        auto bpx = chunk.BlockAt(
            {static_cast<uint16_t>(blockPos.X + 1), blockPos.Y, blockPos.Z});
        auto bny = chunk.BlockAt(
            {blockPos.X, static_cast<uint16_t>(blockPos.Y - 1), blockPos.Z});
        auto bpy = chunk.BlockAt(
            {blockPos.X, static_cast<uint16_t>(blockPos.Y + 1), blockPos.Z});
        auto bnz = chunk.BlockAt(
            {blockPos.X, blockPos.Y, static_cast<uint16_t>(blockPos.Z - 1)});
        auto bpz = chunk.BlockAt(
            {blockPos.X, blockPos.Y, static_cast<uint16_t>(blockPos.Z + 1)});

        uint8_t opaqueBitmask = ADJACENT_BITMASK_NONE;

        opaqueBitmask |= (bnx != INVALID_BLOCK && bnx != AIR_BLOCK)
                             ? ADJACENT_BITMASK_NEG_X
                             : 0;
        opaqueBitmask |= (bpx != INVALID_BLOCK && bpx != AIR_BLOCK)
                             ? ADJACENT_BITMASK_POS_X
                             : 0;
        opaqueBitmask |= (bny != INVALID_BLOCK && bny != AIR_BLOCK)
                             ? ADJACENT_BITMASK_NEG_Y
                             : 0;
        opaqueBitmask |= (bpy != INVALID_BLOCK && bpy != AIR_BLOCK)
                             ? ADJACENT_BITMASK_POS_Y
                             : 0;
        opaqueBitmask |= (bnz != INVALID_BLOCK && bnz != AIR_BLOCK)
                             ? ADJACENT_BITMASK_NEG_Z
                             : 0;
        opaqueBitmask |= (bpz != INVALID_BLOCK && bpz != AIR_BLOCK)
                             ? ADJACENT_BITMASK_POS_Z
                             : 0;

        if (!(opaqueBitmask & ADJACENT_BITMASK_NEG_X)) {
            AddFace(LEFT_FACE, blockPos);
        }
        if (!(opaqueBitmask & ADJACENT_BITMASK_POS_X)) {
            AddFace(RIGHT_FACE, blockPos);
        }
        if (!(opaqueBitmask & ADJACENT_BITMASK_NEG_Y)) {
            AddFace(BOTTOM_FACE, blockPos);
        }
        if (!(opaqueBitmask & ADJACENT_BITMASK_POS_Y)) {
            AddFace(TOP_FACE, blockPos);
        }
        if (!(opaqueBitmask & ADJACENT_BITMASK_NEG_Z)) {
            AddFace(BACK_FACE, blockPos);
        }
        if (!(opaqueBitmask & ADJACENT_BITMASK_POS_Z)) {
            AddFace(FRONT_FACE, blockPos);
        }
    }
}

void ChunkMesh::AddFace(ChunkMeshFace face, BlockPos blockPos)
{
    uint16_t index = 0;
    for (int i = 0; i < 4; ++i) {
        uint8_t x = face.Vertices[index++] + blockPos.X;
        uint8_t y = face.Vertices[index++] + blockPos.Y;
        uint8_t z = face.Vertices[index++] + blockPos.Z;

        ChunkVertex vertex = {.Position = {x, y, z}};

        m_Vertices.emplace_back(vertex);
    }
    m_Indices.push_back(m_IndexCount + 0);
    m_Indices.push_back(m_IndexCount + 1);
    m_Indices.push_back(m_IndexCount + 2);
    m_Indices.push_back(m_IndexCount + 2);
    m_Indices.push_back(m_IndexCount + 3);
    m_Indices.push_back(m_IndexCount + 0);

    m_IndexCount += 4;
}

void ChunkMesh::UpdateMesh()
{
    if (!m_Dirty) {
        return;
    }
    ChunkVertex::Setup();
    m_Mesh = new Mesh();
    uint32_t dataSize = m_Vertices.size() * sizeof(ChunkVertex);
    m_Mesh->SetVertexData(m_Vertices.data(), dataSize, ChunkVertex::Layout);
    m_Mesh->SetIndexData(m_Indices.data(), m_Indices.size() * sizeof(uint16_t));
    m_Dirty = false;
}
} // namespace Graphics