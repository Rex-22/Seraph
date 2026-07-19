#include "MeshSerializer.h"

#include "Seraph/Core/Log.h"
#include "Seraph/Graphics/Mesh.h"

#include <assimp/postprocess.h>
#include <assimp/scene.h>
#include <assimp/Importer.hpp>

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <limits>
#include <vector>

namespace Seraph
{

namespace
{

// ---- .smesh container --------------------------------------------------------
// Native binary mesh format. Same house style as ShaderSerializer: fixed POD
// structs, memcpy'd via a manual cursor, native endianness, reserved fields for
// forward-compat, all section sizes derivable from the header (bounds-checked).
// Section order: header -> attribute directory -> submesh table -> vertex blob
// -> index blob. No material-slot table: slots are binding points, the file
// stores only their count and the submesh->slot mapping.

constexpr char c_MeshMagic[4] = {'S', 'M', 'S', 'H'};
// v1: header + attributes + submeshes + vertex blob + index blob.
// v2: adds a per-slot default-material table (u64 handle * MaterialSlotCount)
//     between the submesh table and the vertex blob.
constexpr u32 c_MeshVersion = 2;

struct MeshFileHeader
{
    char Magic[4];
    u32 Version;
    u32 VertexCount;
    u32 VertexStride;      // bytes per vertex
    u32 IndexCount;
    u32 IndexSize;         // 2 or 4 (bytes per index)
    u32 AttributeCount;
    u32 SubmeshCount;
    u32 MaterialSlotCount; // binding-point count; validates submesh indices
    u32 Reserved[4];       // future: AABB, LOD count, flags, slot-metadata offset
};

struct MeshAttributeEntry
{
    u8 Semantic;   // AttribCode
    u8 Type;       // AttribTypeCode
    u8 Num;        // 1..4
    u8 Normalized; // 0/1
    u8 AsInt;      // 0/1
    u8 Reserved[3];
};

struct MeshSubmeshEntry
{
    u32 BaseVertex;
    u32 BaseIndex;
    u32 IndexCount;
    u32 MaterialSlot;
    u32 Reserved[2]; // future: per-submesh AABB
};

// Our own stable codes for vertex-attribute semantics/types. We translate to and
// from bgfx enums rather than casting their integer values, which are not
// guaranteed stable across bgfx versions.
enum class AttribCode : u8
{
    Position = 0, Normal, Tangent, Bitangent,
    Color0, Color1, Color2, Color3,
    Indices, Weight,
    TexCoord0, TexCoord1, TexCoord2, TexCoord3,
    TexCoord4, TexCoord5, TexCoord6, TexCoord7,
    Unknown = 0xFF,
};

enum class AttribTypeCode : u8
{
    Uint8 = 0, Uint10, Int16, Half, Float,
    Unknown = 0xFF,
};

u8 EncodeAttrib(bgfx::Attrib::Enum a)
{
    switch (a) {
        case bgfx::Attrib::Position:  return static_cast<u8>(AttribCode::Position);
        case bgfx::Attrib::Normal:    return static_cast<u8>(AttribCode::Normal);
        case bgfx::Attrib::Tangent:   return static_cast<u8>(AttribCode::Tangent);
        case bgfx::Attrib::Bitangent: return static_cast<u8>(AttribCode::Bitangent);
        case bgfx::Attrib::Color0:    return static_cast<u8>(AttribCode::Color0);
        case bgfx::Attrib::Color1:    return static_cast<u8>(AttribCode::Color1);
        case bgfx::Attrib::Color2:    return static_cast<u8>(AttribCode::Color2);
        case bgfx::Attrib::Color3:    return static_cast<u8>(AttribCode::Color3);
        case bgfx::Attrib::Indices:   return static_cast<u8>(AttribCode::Indices);
        case bgfx::Attrib::Weight:    return static_cast<u8>(AttribCode::Weight);
        case bgfx::Attrib::TexCoord0: return static_cast<u8>(AttribCode::TexCoord0);
        case bgfx::Attrib::TexCoord1: return static_cast<u8>(AttribCode::TexCoord1);
        case bgfx::Attrib::TexCoord2: return static_cast<u8>(AttribCode::TexCoord2);
        case bgfx::Attrib::TexCoord3: return static_cast<u8>(AttribCode::TexCoord3);
        case bgfx::Attrib::TexCoord4: return static_cast<u8>(AttribCode::TexCoord4);
        case bgfx::Attrib::TexCoord5: return static_cast<u8>(AttribCode::TexCoord5);
        case bgfx::Attrib::TexCoord6: return static_cast<u8>(AttribCode::TexCoord6);
        case bgfx::Attrib::TexCoord7: return static_cast<u8>(AttribCode::TexCoord7);
        default:                      return static_cast<u8>(AttribCode::Unknown);
    }
}

bgfx::Attrib::Enum DecodeAttrib(u8 code)
{
    switch (static_cast<AttribCode>(code)) {
        case AttribCode::Position:  return bgfx::Attrib::Position;
        case AttribCode::Normal:    return bgfx::Attrib::Normal;
        case AttribCode::Tangent:   return bgfx::Attrib::Tangent;
        case AttribCode::Bitangent: return bgfx::Attrib::Bitangent;
        case AttribCode::Color0:    return bgfx::Attrib::Color0;
        case AttribCode::Color1:    return bgfx::Attrib::Color1;
        case AttribCode::Color2:    return bgfx::Attrib::Color2;
        case AttribCode::Color3:    return bgfx::Attrib::Color3;
        case AttribCode::Indices:   return bgfx::Attrib::Indices;
        case AttribCode::Weight:    return bgfx::Attrib::Weight;
        case AttribCode::TexCoord0: return bgfx::Attrib::TexCoord0;
        case AttribCode::TexCoord1: return bgfx::Attrib::TexCoord1;
        case AttribCode::TexCoord2: return bgfx::Attrib::TexCoord2;
        case AttribCode::TexCoord3: return bgfx::Attrib::TexCoord3;
        case AttribCode::TexCoord4: return bgfx::Attrib::TexCoord4;
        case AttribCode::TexCoord5: return bgfx::Attrib::TexCoord5;
        case AttribCode::TexCoord6: return bgfx::Attrib::TexCoord6;
        case AttribCode::TexCoord7: return bgfx::Attrib::TexCoord7;
        default:                    return bgfx::Attrib::Count; // sentinel: skip
    }
}

u8 EncodeAttribType(bgfx::AttribType::Enum t)
{
    switch (t) {
        case bgfx::AttribType::Uint8:  return static_cast<u8>(AttribTypeCode::Uint8);
        case bgfx::AttribType::Uint10: return static_cast<u8>(AttribTypeCode::Uint10);
        case bgfx::AttribType::Int16:  return static_cast<u8>(AttribTypeCode::Int16);
        case bgfx::AttribType::Half:   return static_cast<u8>(AttribTypeCode::Half);
        case bgfx::AttribType::Float:  return static_cast<u8>(AttribTypeCode::Float);
        default:                       return static_cast<u8>(AttribTypeCode::Unknown);
    }
}

bgfx::AttribType::Enum DecodeAttribType(u8 code)
{
    switch (static_cast<AttribTypeCode>(code)) {
        case AttribTypeCode::Uint8:  return bgfx::AttribType::Uint8;
        case AttribTypeCode::Uint10: return bgfx::AttribType::Uint10;
        case AttribTypeCode::Int16:  return bgfx::AttribType::Int16;
        case AttribTypeCode::Half:   return bgfx::AttribType::Half;
        case AttribTypeCode::Float:  return bgfx::AttribType::Float;
        default:                     return bgfx::AttribType::Count; // sentinel: skip
    }
}

// Decompose a bgfx layout into serializable attribute entries, ordered by their
// byte offset so the layout round-trips.
std::vector<MeshAttributeEntry> ExtractAttributes(const bgfx::VertexLayout& layout)
{
    struct OffsetEntry { u16 Offset; MeshAttributeEntry Entry; };
    std::vector<OffsetEntry> ordered;

    for (int i = 0; i < bgfx::Attrib::Count; ++i) {
        const auto attrib = static_cast<bgfx::Attrib::Enum>(i);
        if (!layout.has(attrib))
            continue;

        const u8 semantic = EncodeAttrib(attrib);
        if (semantic == static_cast<u8>(AttribCode::Unknown))
            continue;

        u8 num = 0;
        bgfx::AttribType::Enum type = bgfx::AttribType::Count;
        bool normalized = false;
        bool asInt = false;
        layout.decode(attrib, num, type, normalized, asInt);

        MeshAttributeEntry entry{};
        entry.Semantic = semantic;
        entry.Type = EncodeAttribType(type);
        entry.Num = num;
        entry.Normalized = normalized ? 1 : 0;
        entry.AsInt = asInt ? 1 : 0;
        ordered.push_back({layout.getOffset(attrib), entry});
    }

    std::sort(ordered.begin(), ordered.end(),
              [](const OffsetEntry& a, const OffsetEntry& b) { return a.Offset < b.Offset; });

    std::vector<MeshAttributeEntry> result;
    result.reserve(ordered.size());
    for (const OffsetEntry& e : ordered)
        result.push_back(e.Entry);
    return result;
}

bgfx::VertexLayout BuildLayout(const std::vector<MeshAttributeEntry>& attrs)
{
    bgfx::VertexLayout layout;
    layout.begin();
    for (const MeshAttributeEntry& a : attrs) {
        const bgfx::Attrib::Enum attrib = DecodeAttrib(a.Semantic);
        const bgfx::AttribType::Enum type = DecodeAttribType(a.Type);
        if (attrib == bgfx::Attrib::Count || type == bgfx::AttribType::Count)
            continue;
        layout.add(attrib, a.Num, type, a.Normalized != 0, a.AsInt != 0);
    }
    layout.end();
    return layout;
}

Ref<Asset> ParseSMesh(const Buffer& bytes)
{
    if (bytes.Size() < sizeof(MeshFileHeader)) {
        SP_CORE_ERROR_TAG("Mesh", ".smesh too small for header");
        return nullptr;
    }

    MeshFileHeader header{};
    std::memcpy(&header, bytes.Data(), sizeof(header));

    if (std::memcmp(header.Magic, c_MeshMagic, sizeof(c_MeshMagic)) != 0) {
        SP_CORE_ERROR_TAG("Mesh", ".smesh has bad magic");
        return nullptr;
    }
    if (header.Version == 0 || header.Version > c_MeshVersion) {
        SP_CORE_ERROR_TAG(
            "Mesh", ".smesh version {} unsupported (max {})", header.Version, c_MeshVersion);
        return nullptr;
    }
    if (header.IndexSize != 2 && header.IndexSize != 4) {
        SP_CORE_ERROR_TAG("Mesh", ".smesh invalid index size {}", header.IndexSize);
        return nullptr;
    }

    const u32 slotCount = header.MaterialSlotCount == 0 ? 1 : header.MaterialSlotCount;
    const u64 attrBytes = static_cast<u64>(header.AttributeCount) * sizeof(MeshAttributeEntry);
    const u64 submeshBytes = static_cast<u64>(header.SubmeshCount) * sizeof(MeshSubmeshEntry);
    // v2+ stores a u64 default-material handle per slot after the submesh table.
    const u64 slotBytes =
        header.Version >= 2 ? static_cast<u64>(slotCount) * sizeof(u64) : 0;
    const u64 vertexBytes = static_cast<u64>(header.VertexCount) * header.VertexStride;
    const u64 indexBytes = static_cast<u64>(header.IndexCount) * header.IndexSize;
    const u64 required = sizeof(header) + attrBytes + submeshBytes + slotBytes +
        vertexBytes + indexBytes;
    if (bytes.Size() < required) {
        SP_CORE_ERROR_TAG("Mesh", ".smesh is truncated");
        return nullptr;
    }

    const u8* cursor = bytes.Data() + sizeof(header);

    std::vector<MeshAttributeEntry> attrs(header.AttributeCount);
    if (header.AttributeCount > 0)
        std::memcpy(attrs.data(), cursor, attrBytes);
    cursor += attrBytes;

    std::vector<MeshSubmeshEntry> submeshEntries(header.SubmeshCount);
    if (header.SubmeshCount > 0)
        std::memcpy(submeshEntries.data(), cursor, submeshBytes);
    cursor += submeshBytes;

    std::vector<AssetHandle> slotDefaults;
    if (slotBytes > 0) {
        std::vector<u64> raw(slotCount);
        std::memcpy(raw.data(), cursor, slotBytes);
        slotDefaults.assign(raw.begin(), raw.end());
        cursor += slotBytes;
    }

    const u8* vertexData = cursor;
    cursor += vertexBytes;
    const u8* indexData = cursor;

    auto mesh = Ref<Mesh>::Create();
    bgfx::VertexLayout layout = BuildLayout(attrs);
    if (layout.getStride() != header.VertexStride)
        SP_CORE_WARN_TAG(
            "Mesh", ".smesh stride {} != reconstructed {}; layout may not match",
            header.VertexStride, layout.getStride());
    mesh->SetVertexLayout(layout);
    mesh->StageVertexData(vertexData, static_cast<u32>(vertexBytes));
    mesh->StageIndexData(indexData, static_cast<u32>(indexBytes), header.IndexSize);

    std::vector<Mesh::Submesh> submeshes;
    submeshes.reserve(submeshEntries.size());
    for (const MeshSubmeshEntry& e : submeshEntries)
        submeshes.push_back({e.BaseVertex, e.BaseIndex, e.IndexCount, e.MaterialSlot});
    mesh->SetSubmeshes(std::move(submeshes));
    mesh->SetMaterialSlotCount(slotCount);
    if (!slotDefaults.empty())
        mesh->SetMaterialSlotDefaults(std::move(slotDefaults));

    return mesh; // Upload happens in Finalize (main thread)
}

// ---- Assimp import -----------------------------------------------------------

// Matches the procedural PrimitiveVertex layout (position/color/texcoord/normal/
// tangent), so imported meshes carry the data lit/PBR shaders need. The tangent's
// w component holds handedness for bitangent reconstruction.
struct MeshVertex
{
    float x, y, z;
    u32 abgr;
    float u, v;
    float nx, ny, nz;
    float tx, ty, tz, tw;
};

bgfx::VertexLayout BuildMeshLayout()
{
    bgfx::VertexLayout layout;
    layout.begin()
        .add(bgfx::Attrib::Position, 3, bgfx::AttribType::Float)
        .add(bgfx::Attrib::Color0, 4, bgfx::AttribType::Uint8, true)
        .add(bgfx::Attrib::TexCoord0, 2, bgfx::AttribType::Float)
        .add(bgfx::Attrib::Normal, 3, bgfx::AttribType::Float)
        .add(bgfx::Attrib::Tangent, 4, bgfx::AttribType::Float)
        .end();
    return layout;
}

Ref<Asset> LoadFromAssimp(const AssetMetadata& metadata, const Buffer& bytes)
{
    Assimp::Importer importer;
    constexpr unsigned int flags = aiProcess_Triangulate | aiProcess_GenSmoothNormals |
        aiProcess_CalcTangentSpace | aiProcess_JoinIdenticalVertices |
        aiProcess_FlipUVs | aiProcess_PreTransformVertices;

    // Hint Assimp with the file extension when we have one (loose editor files);
    // packed meshes are .smesh and never reach here.
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
    std::vector<Mesh::Submesh> submeshes;

    for (unsigned int m = 0; m < scene->mNumMeshes; ++m) {
        const aiMesh* mesh = scene->mMeshes[m];
        const auto baseVertex = static_cast<u32>(vertices.size());
        const auto baseIndex = static_cast<u32>(indices.size());

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
            if (mesh->HasNormals()) {
                vertex.nx = mesh->mNormals[v].x;
                vertex.ny = mesh->mNormals[v].y;
                vertex.nz = mesh->mNormals[v].z;
            }
            if (mesh->HasTangentsAndBitangents()) {
                vertex.tx = mesh->mTangents[v].x;
                vertex.ty = mesh->mTangents[v].y;
                vertex.tz = mesh->mTangents[v].z;
                // Handedness: sign of (N × T) · B, matching how the shader
                // reconstructs the bitangent as cross(N, T) * w.
                const aiVector3D& n = mesh->mNormals[v];
                const aiVector3D& t = mesh->mTangents[v];
                const aiVector3D& b = mesh->mBitangents[v];
                const aiVector3D nxt(
                    n.y * t.z - n.z * t.y, n.z * t.x - n.x * t.z, n.x * t.y - n.y * t.x);
                vertex.tw = (nxt.x * b.x + nxt.y * b.y + nxt.z * b.z) < 0.0f ? -1.0f : 1.0f;
            } else {
                vertex.tw = 1.0f;
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

        const auto indexCount = static_cast<u32>(indices.size()) - baseIndex;
        if (indexCount > 0)
            submeshes.push_back(
                {baseVertex, baseIndex, indexCount, mesh->mMaterialIndex});
    }

    if (vertices.empty() || indices.empty()) {
        SP_CORE_ERROR_TAG(
            "Assimp", "'{}' produced no renderable geometry",
            metadata.FilePath.string());
        return nullptr;
    }

    Ref<Mesh> outMesh = Ref<Mesh>::Create();
    outMesh->SetName(metadata.FilePath.filename().string());
    outMesh->SetVertexLayout(BuildMeshLayout());
    outMesh->StageVertexData(
        vertices.data(), static_cast<u32>(vertices.size() * sizeof(MeshVertex)));
    outMesh->StageIndexData(
        indices.data(), static_cast<u32>(indices.size() * sizeof(u16)), sizeof(u16));
    outMesh->SetSubmeshes(std::move(submeshes));
    outMesh->SetMaterialSlotCount(scene->mNumMaterials > 0 ? scene->mNumMaterials : 1);

    SP_CORE_INFO_TAG(
        "Assimp", "Imported '{}': {} vertices, {} indices, {} submeshes",
        metadata.FilePath.string(), vertices.size(), indices.size(),
        outMesh->Submeshes().size());
    return outMesh;
}

} // namespace

MeshSerializer::~MeshSerializer() = default;

Ref<Asset> MeshSerializer::LoadData(const AssetMetadata& metadata, const Buffer& bytes)
{
    if (!bytes)
        return nullptr;

    // Self-describing native format takes priority; fall back to Assimp import.
    if (bytes.Size() >= sizeof(c_MeshMagic) &&
        std::memcmp(bytes.Data(), c_MeshMagic, sizeof(c_MeshMagic)) == 0)
        return ParseSMesh(bytes);

    return LoadFromAssimp(metadata, bytes);
}

void MeshSerializer::Finalize(const Ref<Asset>& asset)
{
    if (Ref<Mesh> mesh = asset.As<Mesh>())
        mesh->Upload();
}

bool MeshSerializer::Serialize(
    const AssetMetadata& /*metadata*/, const Ref<Asset>& asset, Buffer& out)
{
    Ref<Mesh> mesh = asset.As<Mesh>();
    if (!mesh)
        return false;

    const bgfx::VertexLayout* layout = mesh->Layout();
    if (layout == nullptr) {
        SP_CORE_ERROR_TAG("Mesh", "Cannot serialize mesh '{}': no vertex layout", mesh->Name());
        return false;
    }

    const std::vector<u8>& vertexData = mesh->VertexData();
    const std::vector<u8>& indexData = mesh->IndexData();
    if (vertexData.empty() || indexData.empty()) {
        SP_CORE_ERROR_TAG(
            "Mesh", "Cannot serialize mesh '{}': no retained CPU geometry", mesh->Name());
        return false;
    }

    const std::vector<MeshAttributeEntry> attrs = ExtractAttributes(*layout);
    if (attrs.empty()) {
        SP_CORE_ERROR_TAG("Mesh", "Cannot serialize mesh '{}': empty layout", mesh->Name());
        return false;
    }

    // Submeshes default to a single full-range submesh if none were assigned.
    std::vector<Mesh::Submesh> submeshes = mesh->Submeshes();
    if (submeshes.empty())
        submeshes.push_back({0, 0, mesh->IndexCount(), 0});

    std::vector<MeshSubmeshEntry> submeshEntries;
    submeshEntries.reserve(submeshes.size());
    for (const Mesh::Submesh& s : submeshes) {
        MeshSubmeshEntry e{};
        e.BaseVertex = s.BaseVertex;
        e.BaseIndex = s.BaseIndex;
        e.IndexCount = s.IndexCount;
        e.MaterialSlot = s.MaterialSlot;
        submeshEntries.push_back(e);
    }

    MeshFileHeader header{};
    std::memcpy(header.Magic, c_MeshMagic, sizeof(c_MeshMagic));
    header.Version = c_MeshVersion;
    header.VertexCount = mesh->VertexCount();
    header.VertexStride = layout->getStride();
    header.IndexCount = mesh->IndexCount();
    header.IndexSize = mesh->IndexSize();
    header.AttributeCount = static_cast<u32>(attrs.size());
    header.SubmeshCount = static_cast<u32>(submeshEntries.size());
    const u32 slotCount = mesh->MaterialSlotCount() == 0 ? 1 : mesh->MaterialSlotCount();
    header.MaterialSlotCount = slotCount;

    // Per-slot default material handles (v2). Missing entries serialize as null.
    std::vector<u64> slotDefaults(slotCount, c_NullAssetHandle);
    const std::vector<AssetHandle>& meshSlots = mesh->MaterialSlotDefaults();
    for (u32 i = 0; i < slotCount && i < meshSlots.size(); ++i)
        slotDefaults[i] = static_cast<u64>(meshSlots[i]);

    const u64 attrBytes = attrs.size() * sizeof(MeshAttributeEntry);
    const u64 submeshBytes = submeshEntries.size() * sizeof(MeshSubmeshEntry);
    const u64 slotBytes = slotDefaults.size() * sizeof(u64);
    const u64 total = sizeof(header) + attrBytes + submeshBytes + slotBytes +
        vertexData.size() + indexData.size();

    out.Allocate(total);
    if (!out)
        return false;

    u8* cursor = out.Data();
    std::memcpy(cursor, &header, sizeof(header));
    cursor += sizeof(header);
    std::memcpy(cursor, attrs.data(), attrBytes);
    cursor += attrBytes;
    std::memcpy(cursor, submeshEntries.data(), submeshBytes);
    cursor += submeshBytes;
    std::memcpy(cursor, slotDefaults.data(), slotBytes);
    cursor += slotBytes;
    std::memcpy(cursor, vertexData.data(), vertexData.size());
    cursor += vertexData.size();
    std::memcpy(cursor, indexData.data(), indexData.size());

    return true;
}

} // namespace Seraph
