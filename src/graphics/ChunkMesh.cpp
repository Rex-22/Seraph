//
// Created by Ruben on 2025/05/22.
//

#include "ChunkMesh.h"

#include "TextureAtlas.h"
#include "core/Application.h"
#include "core/Log.h"
#include "resources/BakedModel.h"
#include "resources/BlockStateLoader.h"
#include "world/BlockState.h"
#include "world/Blocks.h"
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

    uint32_t atlasWidth = Core::Application::GetInstance()->TextureAtlas()->Width();
    uint32_t atlasHeight = Core::Application::GetInstance()->TextureAtlas()->Height();
    uint32_t textureSize = Core::Application::GetInstance()->TextureAtlas()->SpriteSize();

    const float numSubTextureWidth = static_cast<float>(atlasWidth) / static_cast<float>(textureSize);
    const float numSubTextureHeight = static_cast<float>(atlasHeight) / static_cast<float>(textureSize);

    glm::vec2 uvSize = glm::vec2(1.0f / numSubTextureWidth, 1.0f / numSubTextureHeight);

    // Use new BakedModel system!
    CORE_INFO("ChunkMesh: Using JSON block model system");

    for (uint32_t i = 0; i < ChunkVolume; ++i) {
        const auto blockPos = BlockPosFromIndex(i);

        // Get BlockState from chunk
        BlockStateId stateId = chunk.BlockStateIdAt(blockPos);
        BlockState* state = Blocks::GetStateById(stateId);
        if (!state) {
            continue; // Skip if state not found
        }

        // Get BakedModel from state
        Resources::BakedModel* bakedModel = state->GetBakedModel();
        if (!bakedModel || bakedModel->IsEmpty()) {
            continue; // Skip if no model (e.g., air)
        }

        // Get adjacent blocks for culling
        auto adjacentStateNX = chunk.BlockStateIdAt(
            {static_cast<uint16_t>(blockPos.X - 1), blockPos.Y, blockPos.Z});
        auto adjacentStatePX = chunk.BlockStateIdAt(
            {static_cast<uint16_t>(blockPos.X + 1), blockPos.Y, blockPos.Z});
        auto adjacentStateNY = chunk.BlockStateIdAt(
            {blockPos.X, static_cast<uint16_t>(blockPos.Y - 1), blockPos.Z});
        auto adjacentStatePY = chunk.BlockStateIdAt(
            {blockPos.X, static_cast<uint16_t>(blockPos.Y + 1), blockPos.Z});
        auto adjacentStateNZ = chunk.BlockStateIdAt(
            {blockPos.X, blockPos.Y, static_cast<uint16_t>(blockPos.Z - 1)});
        auto adjacentStatePZ = chunk.BlockStateIdAt(
            {blockPos.X, blockPos.Y, static_cast<uint16_t>(blockPos.Z + 1)});

        // Render all quads from the baked model
        for (const auto& quad : bakedModel->GetQuads()) {
            // Check if face should be culled
            bool shouldCull = false;
            if (!quad.cullface.empty()) {
                // Get adjacent block in cullface direction
                BlockStateId adjacentStateId = 0;
                if (quad.cullface == "west") adjacentStateId = adjacentStateNX;
                else if (quad.cullface == "east") adjacentStateId = adjacentStatePX;
                else if (quad.cullface == "down") adjacentStateId = adjacentStateNY;
                else if (quad.cullface == "up") adjacentStateId = adjacentStatePY;
                else if (quad.cullface == "north") adjacentStateId = adjacentStateNZ;
                else if (quad.cullface == "south") adjacentStateId = adjacentStatePZ;

                BlockState* adjacentState = Blocks::GetStateById(adjacentStateId);
                if (adjacentState) {
                    const Block* adjacentBlock = adjacentState->GetBlock();
                    // Cull if adjacent block is opaque
                    shouldCull = adjacentBlock && adjacentBlock->IsOpaque();
                }
            }

            if (!shouldCull) {
                AddBakedQuad(quad, blockPos);
            }
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
            .AO = 1.0f,  // Fully lit (AO calculation will be added later)
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

void ChunkMesh::AddBakedQuad(const Resources::BakedQuad& quad, const BlockPos blockPos)
{
    // Transform quad vertices from model space [0-1] to chunk space
    // quad.vertices are in model space (0-1), need to add block position
    glm::vec3 blockPosVec(blockPos.X, blockPos.Y, blockPos.Z);

    for (int i = 0; i < 4; ++i) {
        ChunkVertex vertex = {
            .Position = quad.vertices[i] + blockPosVec,
            .UV = quad.uvs[i],
            .AO = quad.aoWeights[i],  // Use pre-calculated AO from baked model
        };

        m_Vertices.emplace_back(vertex);
    }

    // Add indices for two triangles (quad = 2 triangles)
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