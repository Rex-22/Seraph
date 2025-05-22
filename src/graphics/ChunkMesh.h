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

class ChunkMesh
{
public:
    struct ChunkVertex
    {
        glm::vec3 Position;
        static void Setup()
        {
            static bool HasSetup = false;
            if (HasSetup) {
                return;
            }

            Layout.begin()
                .add(bgfx::Attrib::Position, 3, bgfx::AttribType::Float)
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
    void AddFace(ChunkMeshFace face, World::BlockPos blockPos);
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
