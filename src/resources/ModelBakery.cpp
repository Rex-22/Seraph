//
// Created by Claude Code on 2025/11/11.
//

#include "ModelBakery.h"

#include "TextureManager.h"
#include "core/Log.h"
#include "graphics/TextureAtlas.h"

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/transform.hpp>

namespace Resources
{

BakedModel* ModelBakery::BakeModel(
    const BlockModel& model, const Graphics::TextureAtlas* textureAtlas)
{
    if (!model.IsResolved()) {
        CORE_ERROR("Attempting to bake unresolved model");
        return nullptr;
    }

    // Generate cache key
    std::string cacheKey = GenerateCacheKey(model);

    // Check cache
    auto cacheIt = m_BakedModelCache.find(cacheKey);
    if (cacheIt != m_BakedModelCache.end()) {
        return cacheIt->second.get();
    }

    // Create new baked model
    auto bakedModel = std::make_unique<BakedModel>();
    bakedModel->SetAmbientOcclusion(model.IsAmbientOcclusion());

    // Bake all elements
    std::vector<BakedQuad> quads;
    for (const auto& element : model.GetElements()) {
        BakeElement(element, textureAtlas, quads);
    }

    // Add quads to baked model
    for (const auto& quad : quads) {
        bakedModel->AddQuad(quad);
    }

    // Store in cache
    BakedModel* bakedModelPtr = bakedModel.get();
    m_BakedModelCache[cacheKey] = std::move(bakedModel);

    return bakedModelPtr;
}

void ModelBakery::ClearCache() { m_BakedModelCache.clear(); }

void ModelBakery::BakeElement(
    const BlockElement& element, const Graphics::TextureAtlas* textureAtlas,
    std::vector<BakedQuad>& outQuads)
{
    // Bake each face of the element
    const std::vector<std::string> directions = {"down",  "up",   "north",
                                                  "south", "west", "east"};

    for (const auto& direction : directions) {
        const BlockFace* face = element.GetFace(direction);
        if (!face) {
            continue; // Face not defined
        }

        BakedQuad quad = BakeFace(element, direction, *face, textureAtlas);
        outQuads.push_back(quad);
    }
}

BakedQuad ModelBakery::BakeFace(
    const BlockElement& element, const std::string& direction, const BlockFace& face,
    const Graphics::TextureAtlas* textureAtlas)
{
    BakedQuad quad;

    // Get face vertices in model space (0-16)
    glm::vec3 modelVertices[4];
    GetFaceVertices(direction, element.from, element.to, modelVertices);

    // Convert to world space (0-1) and apply rotation if present
    for (int i = 0; i < 4; i++) {
        // Convert from model space (0-16) to world space (0-1)
        glm::vec3 vertex = modelVertices[i] / 16.0f;

        // Apply element rotation if present
        if (element.rotation.has_value()) {
            vertex = ApplyRotation(vertex, element.rotation.value());
        }

        quad.vertices[i] = vertex;
    }

    // Get face normal
    quad.normal = GetFaceNormal(direction);

    // Apply element rotation to normal if present
    if (element.rotation.has_value()) {
        quad.normal = ApplyRotation(quad.normal, element.rotation.value());
    }

    // Resolve texture UVs
    ResolveTextureUVs(face.texture, face.uv, textureAtlas, quad.uvs);

    // Apply UV rotation if specified
    if (face.rotation != 0) {
        ApplyUVRotation(face.rotation, quad.uvs);
    }

    // Set other properties
    quad.cullface = face.cullface;
    quad.shade = element.shade;
    quad.tintindex = face.tintindex;

    return quad;
}

glm::vec3 ModelBakery::ApplyRotation(
    const glm::vec3& vertex, const BlockElement::Rotation& rotation)
{
    // Translate to rotation origin
    glm::vec3 translated = vertex - rotation.origin / 16.0f;

    // Create rotation matrix based on axis
    glm::mat4 rotMatrix(1.0f);
    float angleRad = glm::radians(rotation.angle);

    if (rotation.axis == "x") {
        rotMatrix = glm::rotate(angleRad, glm::vec3(1, 0, 0));
    } else if (rotation.axis == "y") {
        rotMatrix = glm::rotate(angleRad, glm::vec3(0, 1, 0));
    } else if (rotation.axis == "z") {
        rotMatrix = glm::rotate(angleRad, glm::vec3(0, 0, 1));
    }

    // Apply rotation
    glm::vec4 rotated = rotMatrix * glm::vec4(translated, 1.0f);

    // Apply rescale if enabled
    if (rotation.rescale) {
        // Calculate scale factor to restore original size
        // For 45-degree rotations: scale = sqrt(2)
        // For 22.5-degree rotations: scale = sqrt(2 - sqrt(2))
        float scale = 1.0f;
        float absAngle = std::abs(rotation.angle);
        if (absAngle == 45.0f) {
            scale = std::sqrt(2.0f);
        } else if (absAngle == 22.5f) {
            scale = std::sqrt(2.0f - std::sqrt(2.0f));
        }
        rotated *= scale;
    }

    // Translate back
    return glm::vec3(rotated) + rotation.origin / 16.0f;
}

void ModelBakery::GetFaceVertices(
    const std::string& direction, const glm::vec3& from, const glm::vec3& to,
    glm::vec3 outVertices[4])
{
    // Define vertices for each face direction (counter-clockwise winding)
    // Coordinates in model space (0-16)

    if (direction == "down") { // -Y (viewed from below, looking up)
        outVertices[0] = glm::vec3(from.x, from.y, from.z); // 0: back-left
        outVertices[1] = glm::vec3(to.x, from.y, from.z);   // 1: back-right
        outVertices[2] = glm::vec3(to.x, from.y, to.z);     // 2: front-right
        outVertices[3] = glm::vec3(from.x, from.y, to.z);   // 3: front-left
    } else if (direction == "up") { // +Y (viewed from above, looking down)
        outVertices[0] = glm::vec3(from.x, to.y, to.z);     // 0: front-left
        outVertices[1] = glm::vec3(to.x, to.y, to.z);       // 1: front-right
        outVertices[2] = glm::vec3(to.x, to.y, from.z);     // 2: back-right
        outVertices[3] = glm::vec3(from.x, to.y, from.z);   // 3: back-left
    } else if (direction == "north") { // -Z
        outVertices[0] = glm::vec3(to.x, from.y, from.z);   // 0: bottom-right
        outVertices[1] = glm::vec3(from.x, from.y, from.z); // 1: bottom-left
        outVertices[2] = glm::vec3(from.x, to.y, from.z);   // 2: top-left
        outVertices[3] = glm::vec3(to.x, to.y, from.z);     // 3: top-right
    } else if (direction == "south") { // +Z
        outVertices[0] = glm::vec3(from.x, from.y, to.z);   // 0: bottom-left
        outVertices[1] = glm::vec3(to.x, from.y, to.z);     // 1: bottom-right
        outVertices[2] = glm::vec3(to.x, to.y, to.z);       // 2: top-right
        outVertices[3] = glm::vec3(from.x, to.y, to.z);     // 3: top-left
    } else if (direction == "west") { // -X
        outVertices[0] = glm::vec3(from.x, from.y, from.z); // 0: bottom-back
        outVertices[1] = glm::vec3(from.x, from.y, to.z);   // 1: bottom-front
        outVertices[2] = glm::vec3(from.x, to.y, to.z);     // 2: top-front
        outVertices[3] = glm::vec3(from.x, to.y, from.z);   // 3: top-back
    } else if (direction == "east") { // +X
        outVertices[0] = glm::vec3(to.x, from.y, to.z);     // 0: bottom-front
        outVertices[1] = glm::vec3(to.x, from.y, from.z);   // 1: bottom-back
        outVertices[2] = glm::vec3(to.x, to.y, from.z);     // 2: top-back
        outVertices[3] = glm::vec3(to.x, to.y, to.z);       // 3: top-front
    }
}

glm::vec3 ModelBakery::GetFaceNormal(const std::string& direction) const
{
    if (direction == "down")
        return glm::vec3(0, -1, 0);
    if (direction == "up")
        return glm::vec3(0, 1, 0);
    if (direction == "north")
        return glm::vec3(0, 0, -1);
    if (direction == "south")
        return glm::vec3(0, 0, 1);
    if (direction == "west")
        return glm::vec3(-1, 0, 0);
    if (direction == "east")
        return glm::vec3(1, 0, 0);

    return glm::vec3(0, 1, 0); // Default up
}

void ModelBakery::ResolveTextureUVs(
    const std::string& texturePath, const glm::vec4& elementUV,
    const Graphics::TextureAtlas* textureAtlas, glm::vec2 outUVs[4])
{
    if (!textureAtlas) {
        CORE_ERROR("TextureAtlas is null in ModelBakery::ResolveTextureUVs");
        // Set default UVs
        outUVs[0] = glm::vec2(0, 0);
        outUVs[1] = glm::vec2(1, 0);
        outUVs[2] = glm::vec2(1, 1);
        outUVs[3] = glm::vec2(0, 1);
        return;
    }

    // Calculate texture grid coordinates from elementUV
    // elementUV is in texture space [0-16], convert to normalized [0-1]
    // Note: Minecraft UVs have origin at top-left, OpenGL has origin at bottom-left
    // So we flip the V coordinates
    float u0 = elementUV.x / 16.0f;
    float v0 = 1.0f - (elementUV.y / 16.0f);  // Flip V
    float u1 = elementUV.z / 16.0f;
    float v1 = 1.0f - (elementUV.w / 16.0f);  // Flip V

    // Get atlas dimensions
    float atlasWidth = static_cast<float>(textureAtlas->Width());
    float atlasHeight = static_cast<float>(textureAtlas->Height());
    float spriteSize = static_cast<float>(textureAtlas->SpriteSize());

    // Calculate number of sprites in atlas
    float numSpritesWidth = atlasWidth / spriteSize;
    float numSpritesHeight = atlasHeight / spriteSize;

    // Calculate size of one sprite in UV space
    float uvSpriteWidth = 1.0f / numSpritesWidth;
    float uvSpriteHeight = 1.0f / numSpritesHeight;

    // Look up texture position in atlas using TextureManager
    float baseU = 0.0f;
    float baseV = 0.0f;

    if (m_TextureManager && m_TextureManager->HasTexture(texturePath)) {
        TextureInfo texInfo = m_TextureManager->GetTextureInfo(texturePath);
        baseU = texInfo.uvOffset.x;
        baseV = texInfo.uvOffset.y;

        CORE_INFO("Resolved texture '{}' to UV offset ({}, {})", texturePath, baseU, baseV);
    } else {
        CORE_WARN("Texture '{}' not found in atlas, using (0,0)", texturePath);
    }

    // Map element UVs [0-1] to atlas UVs using texture's position
    // Element UV is normalized [0-1] within the sprite
    outUVs[0] = glm::vec2(baseU + u0 * uvSpriteWidth, baseV + v0 * uvSpriteHeight);
    outUVs[1] = glm::vec2(baseU + u1 * uvSpriteWidth, baseV + v0 * uvSpriteHeight);
    outUVs[2] = glm::vec2(baseU + u1 * uvSpriteWidth, baseV + v1 * uvSpriteHeight);
    outUVs[3] = glm::vec2(baseU + u0 * uvSpriteWidth, baseV + v1 * uvSpriteHeight);
}

void ModelBakery::ApplyUVRotation(int rotation, glm::vec2 uvs[4])
{
    if (rotation == 0) {
        return; // No rotation
    }

    // Rotate UVs counter-clockwise by specified degrees
    glm::vec2 temp[4];
    for (int i = 0; i < 4; i++) {
        temp[i] = uvs[i];
    }

    int steps = (rotation / 90) % 4;
    for (int i = 0; i < 4; i++) {
        uvs[i] = temp[(i + steps) % 4];
    }
}

std::string ModelBakery::GenerateCacheKey(const BlockModel& model)
{
    // Simple cache key based on element count and texture count
    // TODO: Implement proper hashing for better cache key generation
    return std::to_string(model.GetElements().size()) + "_" +
           std::to_string(model.GetTextures().size());
}

} // namespace Resources
