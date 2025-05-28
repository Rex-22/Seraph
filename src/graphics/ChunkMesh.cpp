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

    constexpr float atlasSize = 32.0f;
    constexpr float textureSize = 16.0f;

    const float numSubTextureWidth = atlasSize/textureSize;  // Atlas is 2 sub-textures wide (32px / 16px)
    const float numSubTextureHeight = atlasSize/textureSize; // Atlas is 2 sub-textures high (32px / 16px)

    glm::vec2 uvSize = glm::vec2(1.0f / numSubTextureWidth, 1.0f / numSubTextureHeight); // Should be (0.5, 0.5)

    for (uint32_t i = 0; i < ChunkVolume; ++i) {
        const auto blockPos = BlockPosFromIndex(i);
        const Block* b = chunk.BlockAt(blockPos);

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

        uint16_t opaqueBitmask = ADJACENT_BITMASK_NONE;

        opaqueBitmask |= bnx != nullptr && (bnx->IsOpaque() || (bnx->CullsSelf() && b==bnx)) ? ADJACENT_BITMASK_NEG_X : 0;
        opaqueBitmask |= bpx != nullptr && (bpx->IsOpaque() || (bpx->CullsSelf() && b==bpx)) ? ADJACENT_BITMASK_POS_X : 0;
        opaqueBitmask |= bny != nullptr && (bny->IsOpaque() || (bny->CullsSelf() && b==bny)) ? ADJACENT_BITMASK_NEG_Y : 0;
        opaqueBitmask |= bpy != nullptr && (bpy->IsOpaque() || (bpy->CullsSelf() && b==bpy)) ? ADJACENT_BITMASK_POS_Y : 0;
        opaqueBitmask |= bnz != nullptr && (bnz->IsOpaque() || (bnz->CullsSelf() && b==bnz)) ? ADJACENT_BITMASK_NEG_Z : 0;
        opaqueBitmask |= bpz != nullptr && (bpz->IsOpaque() || (bpz->CullsSelf() && b==bpz)) ? ADJACENT_BITMASK_POS_Z : 0;


        if ((opaqueBitmask & ADJACENT_BITMASK_NEG_X) == 0) {
            glm::vec2 textureRegion = b->TextureRegion(Direction::Left); // Assuming (0,0) is currently hardcoded
            glm::vec2 uvOffset;
            uvOffset.x = textureRegion.x * uvSize.x;
            uvOffset.y = (numSubTextureHeight - 1.0f - textureRegion.y) * uvSize.y;

            AddFace(LEFT_FACE, blockPos, uvOffset, uvSize);
        }
        if ((opaqueBitmask & ADJACENT_BITMASK_POS_X) == 0) {
            glm::vec2 textureRegion = b->TextureRegion(Direction::Right); // Assuming (0,0) is currently hardcoded
            glm::vec2 uvOffset;
            uvOffset.x = textureRegion.x * uvSize.x;
            uvOffset.y = (numSubTextureHeight - 1.0f - textureRegion.y) * uvSize.y;

            AddFace(RIGHT_FACE, blockPos, uvOffset, uvSize);
        }
        if ((opaqueBitmask & ADJACENT_BITMASK_NEG_Y) == 0) {
            glm::vec2 textureRegion = b->TextureRegion(Direction::Bottom); // Assuming (0,0) is currently hardcoded
            glm::vec2 uvOffset;
            uvOffset.x = textureRegion.x * uvSize.x;
            uvOffset.y = (numSubTextureHeight - 1.0f - textureRegion.y) * uvSize.y;

            AddFace(BOTTOM_FACE, blockPos, uvOffset, uvSize);
        }
        if ((opaqueBitmask & ADJACENT_BITMASK_POS_Y) == 0) {
            glm::vec2 textureRegion = b->TextureRegion(Direction::Top); // Assuming (0,0) is currently hardcoded
            glm::vec2 uvOffset;
            uvOffset.x = textureRegion.x * uvSize.x;
            uvOffset.y = (numSubTextureHeight - 1.0f - textureRegion.y) * uvSize.y;

            AddFace(TOP_FACE, blockPos, uvOffset, uvSize);
        }
        if ((opaqueBitmask & ADJACENT_BITMASK_NEG_Z) == 0) {
            glm::vec2 textureRegion = b->TextureRegion(Direction::Back); // Assuming (0,0) is currently hardcoded
            glm::vec2 uvOffset;
            uvOffset.x = textureRegion.x * uvSize.x;
            uvOffset.y = (numSubTextureHeight - 1.0f - textureRegion.y) * uvSize.y;

            AddFace(BACK_FACE, blockPos, uvOffset, uvSize);
        }
        if ((opaqueBitmask & ADJACENT_BITMASK_POS_Z) == 0) {
            glm::vec2 textureRegion = b->TextureRegion(Direction::Front); // Assuming (0,0) is currently hardcoded
            glm::vec2 uvOffset;
            uvOffset.x = textureRegion.x * uvSize.x;
            uvOffset.y = (numSubTextureHeight - 1.0f - textureRegion.y) * uvSize.y;

            AddFace(FRONT_FACE, blockPos, uvOffset, uvSize);
        }
    }
}

constexpr glm::vec2 Uvs[4] = {
    glm::vec2(0, 0),
   glm::vec2(1, 0),
   glm::vec2(1, 1),
   glm::vec2(0, 1),
};

void ChunkMesh::AddFace(const ChunkMeshFace face, const BlockPos blockPos, glm::vec2 uvOffset, glm::vec2 uvSize)
{
    uint16_t index = 0;
    for (uint8_t i = 0; i < 4; ++i) {
        uint8_t x = face.Vertices[index++] + blockPos.X;
        uint8_t y = face.Vertices[index++] + blockPos.Y;
        uint8_t z = face.Vertices[index++] + blockPos.Z;

        ChunkVertex vertex = {
            .Position = {x, y, z},
            .UV = uvOffset + Uvs[i] * uvSize,
        };

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