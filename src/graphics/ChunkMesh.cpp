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

bgfx::VertexLayout ChunkMesh::ChunkVertex::Layout;

ChunkMesh::~ChunkMesh()
{
    delete m_Mesh;
    delete m_TransparentMesh;
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
    m_OpaqueVertices.clear();
    m_OpaqueIndices.clear();
    m_OpaqueIndexCount = 0;
    m_TransparentVertices.clear();
    m_TransparentIndices.clear();
    m_TransparentIndexCount = 0;

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
                AddBakedQuad(quad, blockPos, chunk);
            }
        }
    }
}

bool ChunkMesh::IsBlockOpaqueAt(const Chunk& chunk, const BlockPos& pos) const
{
    BlockStateId stateId = chunk.BlockStateIdAt(pos);
    BlockState* state = Blocks::GetStateById(stateId);
    if (!state) {
        return false;  // Air or invalid
    }

    const Block* block = state->GetBlock();
    return block && block->IsOpaque();
}

float ChunkMesh::CalculateVertexAO(
    const Chunk& chunk, const BlockPos& blockPos,
    const glm::vec3& normal, const glm::vec3& vertexOffset)
{
    // Minecraft-style AO: Check 3 adjacent blocks per vertex
    // The blocks are checked OUTSIDE the current face, not inside the block

    // First, offset to the face position (add normal direction)
    int baseX = static_cast<int>(blockPos.X);
    int baseY = static_cast<int>(blockPos.Y);
    int baseZ = static_cast<int>(blockPos.Z);

    // Offset by face normal (check blocks on the other side of the face)
    int normalOffsetX = normal.x > 0.5f ? 1 : (normal.x < -0.5f ? -1 : 0);
    int normalOffsetY = normal.y > 0.5f ? 1 : (normal.y < -0.5f ? -1 : 0);
    int normalOffsetZ = normal.z > 0.5f ? 1 : (normal.z < -0.5f ? -1 : 0);

    baseX += normalOffsetX;
    baseY += normalOffsetY;
    baseZ += normalOffsetZ;

    // Determine perpendicular offsets based on vertex position
    int perpX = 0, perpY = 0, perpZ = 0;

    if (std::abs(normal.y) > 0.5f) {
        // Top or bottom face - check X and Z perpendicular
        perpX = (vertexOffset.x > 0.5f) ? 1 : -1;
        perpZ = (vertexOffset.z > 0.5f) ? 1 : -1;
    } else if (std::abs(normal.x) > 0.5f) {
        // Left or right face - check Y and Z perpendicular
        perpY = (vertexOffset.y > 0.5f) ? 1 : -1;
        perpZ = (vertexOffset.z > 0.5f) ? 1 : -1;
    } else {
        // Front or back face - check X and Y perpendicular
        perpX = (vertexOffset.x > 0.5f) ? 1 : -1;
        perpY = (vertexOffset.y > 0.5f) ? 1 : -1;
    }

    // Calculate the 3 neighbor positions (on the outside of the face)
    BlockPos side1 = {
        static_cast<uint16_t>(baseX + (std::abs(normal.y) > 0.5f ? perpX : (std::abs(normal.x) > 0.5f ? 0 : perpX))),
        static_cast<uint16_t>(baseY + (std::abs(normal.x) > 0.5f ? perpY : (std::abs(normal.z) > 0.5f ? perpY : 0))),
        static_cast<uint16_t>(baseZ + (std::abs(normal.z) > 0.5f ? 0 : perpZ))
    };

    BlockPos side2 = {
        static_cast<uint16_t>(baseX + (std::abs(normal.y) > 0.5f ? 0 : (std::abs(normal.z) > 0.5f ? perpX : 0))),
        static_cast<uint16_t>(baseY + (std::abs(normal.x) > 0.5f ? 0 : (std::abs(normal.z) > 0.5f ? perpY : 0))),
        static_cast<uint16_t>(baseZ + (std::abs(normal.y) > 0.5f ? perpZ : (std::abs(normal.x) > 0.5f ? perpZ : 0)))
    };

    BlockPos corner = {
        static_cast<uint16_t>(baseX + (std::abs(normal.x) > 0.5f ? 0 : perpX)),
        static_cast<uint16_t>(baseY + (std::abs(normal.y) > 0.5f ? 0 : perpY)),
        static_cast<uint16_t>(baseZ + (std::abs(normal.z) > 0.5f ? 0 : perpZ))
    };

    // Check if blocks are opaque
    bool side1Opaque = IsBlockOpaqueAt(chunk, side1);
    bool side2Opaque = IsBlockOpaqueAt(chunk, side2);
    bool cornerOpaque = IsBlockOpaqueAt(chunk, corner);

    // Minecraft AO formula
    if (side1Opaque && side2Opaque) {
        return 0.0f;  // Maximum occlusion (both sides blocked)
    }

    int occludedCount = (side1Opaque ? 1 : 0) + (side2Opaque ? 1 : 0) + (cornerOpaque ? 1 : 0);
    return (3.0f - static_cast<float>(occludedCount)) / 3.0f;
    // Returns: 1.0 (no occlusion), 0.66 (one block), 0.33 (two blocks), 0.0 (three blocks)
}

void ChunkMesh::AddBakedQuad(const Resources::BakedQuad& quad, const BlockPos blockPos, const Chunk& chunk)
{
    // Determine if block is transparent
    BlockStateId stateId = chunk.BlockStateIdAt(blockPos);
    BlockState* state = Blocks::GetStateById(stateId);
    bool isTransparent = false;

    if (state) {
        const Block* block = state->GetBlock();
        isTransparent = block && block->GetTransparencyType() != TransparencyType::Opaque;
    }

    // Transform quad vertices from model space [0-1] to chunk space
    glm::vec3 blockPosVec(blockPos.X, blockPos.Y, blockPos.Z);

    // Choose which vertex/index lists to use
    auto& vertices = isTransparent ? m_TransparentVertices : m_OpaqueVertices;
    auto& indices = isTransparent ? m_TransparentIndices : m_OpaqueIndices;
    uint16_t& indexCount = isTransparent ? m_TransparentIndexCount : m_OpaqueIndexCount;

    for (int i = 0; i < 4; ++i) {
        // Calculate dynamic per-vertex AO
        float aoValue = 1.0f;  // Default: fully lit

        if (state) {
            const Block* block = state->GetBlock();
            Resources::BakedModel* model = state->GetBakedModel();

            if (block && model &&
                block->HasAmbientOcclusion() &&
                model->IsAmbientOcclusion() &&
                quad.shade) {
                // Calculate dynamic AO based on adjacent blocks
                aoValue = CalculateVertexAO(chunk, blockPos, quad.normal, quad.vertices[i]);
            }
        }

        ChunkVertex vertex = {
            .Position = quad.vertices[i] + blockPosVec,
            .UV = quad.uvs[i],
            .AO = aoValue,  // Dynamic AO calculation!
        };

        vertices.emplace_back(vertex);
    }

    // Add indices for two triangles (quad = 2 triangles)
    indices.push_back(indexCount + 0);
    indices.push_back(indexCount + 1);
    indices.push_back(indexCount + 2);
    indices.push_back(indexCount + 2);
    indices.push_back(indexCount + 3);
    indices.push_back(indexCount + 0);

    indexCount += 4;
}

void ChunkMesh::UpdateMesh()
{
    if (!m_Dirty) {
        return;
    }
    ChunkVertex::Setup();

    // Build opaque mesh
    delete m_Mesh;
    m_Mesh = new Mesh();
    if (!m_OpaqueVertices.empty()) {
        uint32_t dataSize = m_OpaqueVertices.size() * sizeof(ChunkVertex);
        m_Mesh->SetVertexData(m_OpaqueVertices.data(), dataSize, ChunkVertex::Layout);
        m_Mesh->SetIndexData(m_OpaqueIndices.data(), m_OpaqueIndices.size() * sizeof(uint16_t));
    }

    // Build transparent mesh
    delete m_TransparentMesh;
    m_TransparentMesh = new Mesh();
    if (!m_TransparentVertices.empty()) {
        uint32_t dataSize = m_TransparentVertices.size() * sizeof(ChunkVertex);
        m_TransparentMesh->SetVertexData(m_TransparentVertices.data(), dataSize, ChunkVertex::Layout);
        m_TransparentMesh->SetIndexData(m_TransparentIndices.data(), m_TransparentIndices.size() * sizeof(uint16_t));
    }

    m_Dirty = false;

    CORE_INFO("ChunkMesh: Built {} opaque vertices, {} transparent vertices",
        m_OpaqueVertices.size(), m_TransparentVertices.size());
}

const Mesh* ChunkMesh::GetTransparentMesh()
{
    UpdateMesh();
    return m_TransparentMesh;
}

} // namespace Graphics