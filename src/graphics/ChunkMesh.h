//
// Created by Ruben on 2025/05/22.
//
#pragma once
#include "Mesh.h"
#include "glm/glm.hpp"
#include "world/Block.h"

namespace World
{
class Chunk;
}

namespace Graphics
{

struct ChunkMeshFace
{
    int8_t Vertices[12];
};

constexpr uint16_t ADJACENT_BITMASK_NONE = 0; // 000000
constexpr uint16_t ADJACENT_BITMASK_NEG_X = 1 << 0; // 1  (000001) - Left
constexpr uint16_t ADJACENT_BITMASK_POS_X = 1 << 1; // 2  (000010) - Right
constexpr uint16_t ADJACENT_BITMASK_NEG_Y = 1 << 2; // 4  (000100) - Bottom
constexpr uint16_t ADJACENT_BITMASK_POS_Y = 1 << 3; // 8  (001000) - Top
constexpr uint16_t ADJACENT_BITMASK_NEG_Z = 1 << 4; // 16 (010000) - Back
constexpr uint16_t ADJACENT_BITMASK_POS_Z = 1 << 5; // 32 (100000) - Front

constexpr uint16_t ALL_ADJACENT_BITMASKS =
    ADJACENT_BITMASK_NEG_X | ADJACENT_BITMASK_POS_X | ADJACENT_BITMASK_NEG_Y |
    ADJACENT_BITMASK_POS_Y | ADJACENT_BITMASK_NEG_Z | ADJACENT_BITMASK_POS_Z;

class ChunkMesh
{
public:
    struct ChunkVertex
    {
        glm::vec3 Position;
        glm::vec2 UV;
        static void Setup()
        {
            static bool HasSetup = false;
            if (HasSetup) {
                return;
            }

            Layout.begin()
                .add(bgfx::Attrib::Position, 3, bgfx::AttribType::Float)
                .add(bgfx::Attrib::TexCoord0, 2, bgfx::AttribType::Float)
                .end();
            HasSetup = true;
        }
        static bgfx::VertexLayout Layout;
    };

public:
    ~ChunkMesh();

    static ChunkMesh* Create(const World::Chunk& chunk);

    const Mesh* GetMesh();

private:
    void GenerateMeshData(const World::Chunk& chunk);
    void AddFace(ChunkMeshFace face, World::BlockPos blockPos, glm::vec2 uvOffset, glm::vec2 uvSize);
    void UpdateMesh();

private:
    Mesh* m_Mesh = nullptr;

    std::vector<ChunkVertex> m_Vertices;
    std::vector<uint16_t> m_Indices;
    uint16_t m_IndexCount = 0;

    bool m_Dirty = true;

private:
};

} // namespace Graphics
