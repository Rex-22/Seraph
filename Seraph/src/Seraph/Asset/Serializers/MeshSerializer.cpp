#include "MeshSerializer.h"

#include "Seraph/Core/Log.h"
#include "Seraph/Graphics/Material/ColorProperty.h"
#include "Seraph/Graphics/Material/Material.h"
#include "Seraph/Graphics/Material/TextureProperty.h"
#include "Seraph/Graphics/Mesh.h"
#include "Seraph/Graphics/ShaderManager.h"
#include "Seraph/Graphics/Texture2D.h"

#include <assimp/postprocess.h>
#include <assimp/scene.h>
#include <assimp/Importer.hpp>

#include <cstdint>
#include <limits>
#include <vector>

namespace Seraph
{

namespace
{

// Matches the layout the built-in "simple" shader expects (same as the
// procedural cube), so imported meshes render with the default material.
struct MeshVertex
{
    float x, y, z;
    u32 abgr;
    float u, v;
};

bgfx::VertexLayout BuildMeshLayout()
{
    bgfx::VertexLayout layout;
    layout.begin()
        .add(bgfx::Attrib::Position, 3, bgfx::AttribType::Float)
        .add(bgfx::Attrib::Color0, 4, bgfx::AttribType::Uint8, true)
        .add(bgfx::Attrib::TexCoord0, 2, bgfx::AttribType::Float)
        .end();
    return layout;
}

} // namespace

MeshSerializer::~MeshSerializer() = default;

Ref<Asset> MeshSerializer::LoadData(const AssetMetadata& metadata, const Buffer& bytes)
{
    if (!bytes)
        return nullptr;

    Assimp::Importer importer;
    constexpr unsigned int flags = aiProcess_Triangulate | aiProcess_GenNormals |
        aiProcess_JoinIdenticalVertices | aiProcess_FlipUVs |
        aiProcess_PreTransformVertices;

    // Hint Assimp with the file extension so it can pick the right importer.
    const std::string hint = metadata.FilePath.extension().string();
    const aiScene* scene = importer.ReadFileFromMemory(
        bytes.Data(), bytes.Size(), flags,
        hint.empty() ? nullptr : hint.c_str() + (hint[0] == '.' ? 1 : 0));

    if (scene == nullptr || (scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE) != 0 ||
        scene->mRootNode == nullptr) {
        SP_CORE_ERROR_TAG(
            "Assimp", "Failed to import '{}': {}", metadata.FilePath.string(),
            importer.GetErrorString());
        return nullptr;
    }

    std::vector<MeshVertex> vertices;
    std::vector<u16> indices;

    for (unsigned int m = 0; m < scene->mNumMeshes; ++m) {
        const aiMesh* mesh = scene->mMeshes[m];
        const auto baseVertex = static_cast<u32>(vertices.size());

        if (baseVertex + mesh->mNumVertices > std::numeric_limits<u16>::max()) {
            SP_CORE_ERROR_TAG(
                "Assimp",
                "'{}' exceeds the 65535-vertex limit for 16-bit indices; "
                "truncating import.",
                metadata.FilePath.string());
            break;
        }

        for (unsigned int v = 0; v < mesh->mNumVertices; ++v) {
            MeshVertex vertex{};
            vertex.x = mesh->mVertices[v].x;
            vertex.y = mesh->mVertices[v].y;
            vertex.z = mesh->mVertices[v].z;
            vertex.abgr = 0xffffffff;
            if (mesh->HasTextureCoords(0)) {
                vertex.u = mesh->mTextureCoords[0][v].x;
                vertex.v = mesh->mTextureCoords[0][v].y;
            }
            vertices.push_back(vertex);
        }

        for (unsigned int f = 0; f < mesh->mNumFaces; ++f) {
            const aiFace& face = mesh->mFaces[f];
            if (face.mNumIndices != 3)
                continue; // triangulated above; skip anything degenerate
            for (unsigned int i = 0; i < 3; ++i)
                indices.push_back(static_cast<u16>(baseVertex + face.mIndices[i]));
        }
    }

    if (vertices.empty() || indices.empty()) {
        SP_CORE_ERROR_TAG(
            "Assimp", "'{}' produced no renderable geometry",
            metadata.FilePath.string());
        return nullptr;
    }

    // Material assigned during Finalize (main thread). No GPU work here.
    Ref<Mesh> outMesh = Ref<Mesh>::Create(Ref<Material>(nullptr));
    outMesh->SetName(metadata.FilePath.filename().string());
    outMesh->SetVertexLayout(BuildMeshLayout());
    outMesh->StageVertexData(
        vertices.data(), static_cast<u32>(vertices.size() * sizeof(MeshVertex)));
    outMesh->StageIndexData(indices.data(), static_cast<u32>(indices.size()));

    SP_CORE_INFO_TAG(
        "Assimp", "Imported '{}': {} vertices, {} indices",
        metadata.FilePath.string(), vertices.size(), indices.size());
    return outMesh;
}

void MeshSerializer::Finalize(const Ref<Asset>& asset)
{
    Ref<Mesh> mesh = asset.As<Mesh>();
    if (!mesh)
        return;

    mesh->SetMaterial(GetOrCreateDefaultMaterial());
    mesh->Upload();
}

Ref<Material> MeshSerializer::GetOrCreateDefaultMaterial()
{
    if (m_DefaultMaterial)
        return m_DefaultMaterial;

    Ref<Material> material = Ref<Material>::Create(ShaderManager::Get("simple"));

    u32 white = 0xffffffff;
    m_DefaultTexture = Texture2D::Create("MeshDefaultTexture", &white, 1, 1);

    material->AddProperty<ColorProperty>("s_color", glm::vec4(1.0f));
    material->AddProperty<TextureProperty>("s_texColor", m_DefaultTexture, 0);

    m_DefaultMaterial = material;
    return m_DefaultMaterial;
}

} // namespace Seraph
