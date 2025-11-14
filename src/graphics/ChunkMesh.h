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

namespace Resources
{
struct BakedQuad;
}

namespace Graphics
{

class ChunkMesh
{
public:
    struct ChunkVertex
    {
        glm::vec3 Position;
        glm::vec2 UV;
        glm::vec2 OverlayUV;  // Overlay texture coordinates for multi-layer rendering
        float AO;             // Ambient occlusion weight [0-1]

        static void Setup()
        {
            static bool HasSetup = false;
            if (HasSetup) {
                return;
            }

            Layout.begin()
                .add(bgfx::Attrib::Position, 3, bgfx::AttribType::Float)
                .add(bgfx::Attrib::TexCoord0, 2, bgfx::AttribType::Float)  // Base UV
                .add(bgfx::Attrib::TexCoord1, 2, bgfx::AttribType::Float)  // Overlay UV
                .add(bgfx::Attrib::TexCoord2, 1, bgfx::AttribType::Float)  // AO
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
    void AddBakedQuad(const Resources::BakedQuad& quad, World::BlockPos blockPos, const World::Chunk& chunk);
    void UpdateMesh();

    // Ambient occlusion calculation
    float CalculateVertexAO(
        const World::Chunk& chunk,
        const World::BlockPos& blockPos,
        const glm::vec3& normal,
        const glm::vec3& vertexOffset);

    bool IsBlockOpaqueAt(const World::Chunk& chunk, const World::BlockPos& pos) const;

private:
    Mesh* m_Mesh = nullptr;
    Mesh* m_TransparentMesh = nullptr;

    // Opaque geometry (rendered first with Z-write)
    std::vector<ChunkVertex> m_OpaqueVertices;
    std::vector<uint16_t> m_OpaqueIndices;
    uint16_t m_OpaqueIndexCount = 0;

    // Transparent geometry (rendered second with blending)
    std::vector<ChunkVertex> m_TransparentVertices;
    std::vector<uint16_t> m_TransparentIndices;
    uint16_t m_TransparentIndexCount = 0;

    bool m_Dirty = true;

public:
    const Mesh* GetTransparentMesh();

private:
};

} // namespace Graphics
