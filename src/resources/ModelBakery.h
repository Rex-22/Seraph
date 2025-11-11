//
// Created by Claude Code on 2025/11/11.
//

#pragma once

#include "BakedModel.h"
#include "BlockModel.h"

#include <memory>
#include <string>
#include <unordered_map>

namespace Graphics
{
class TextureAtlas;
}

namespace Resources
{
class TextureManager;

/**
 * Compiles BlockModels into BakedModels for efficient rendering.
 * Applies transformations, resolves texture UVs, and calculates ambient occlusion.
 */
class ModelBakery
{
public:
    ModelBakery() = default;
    ~ModelBakery() = default;

    /**
     * Set the TextureManager for texture UV lookups.
     * Must be called before baking models.
     */
    void SetTextureManager(const TextureManager* textureManager) {
        m_TextureManager = textureManager;
    }

    /**
     * Bake a block model into a renderable format.
     * Applies all transformations, resolves texture UVs, and calculates AO.
     *
     * @param model Source block model (must be resolved)
     * @param textureAtlas Texture atlas for UV resolution
     * @return Pointer to baked model, or nullptr if baking failed
     *
     * The returned pointer is owned by the bakery and remains valid until
     * the bakery is destroyed or ClearCache() is called.
     */
    BakedModel* BakeModel(const BlockModel& model, const Graphics::TextureAtlas* textureAtlas);

    /**
     * Clear the baked model cache.
     * All previously returned model pointers become invalid.
     */
    void ClearCache();

    /**
     * Get the number of cached baked models.
     */
    size_t GetCacheSize() const { return m_BakedModelCache.size(); }

private:
    /**
     * Bake a single element into quads.
     */
    void BakeElement(
        const BlockElement& element, const Graphics::TextureAtlas* textureAtlas,
        std::vector<BakedQuad>& outQuads);

    /**
     * Bake a single face of an element into a quad.
     */
    BakedQuad BakeFace(
        const BlockElement& element, const std::string& direction, const BlockFace& face,
        const Graphics::TextureAtlas* textureAtlas);

    /**
     * Apply rotation to a vertex position.
     */
    glm::vec3 ApplyRotation(
        const glm::vec3& vertex, const BlockElement::Rotation& rotation);

    /**
     * Get the 4 corner positions for a face direction.
     * Returns positions in model space (0-16).
     */
    void GetFaceVertices(
        const std::string& direction, const glm::vec3& from, const glm::vec3& to,
        glm::vec3 outVertices[4]);

    /**
     * Get the normal vector for a face direction.
     */
    glm::vec3 GetFaceNormal(const std::string& direction) const;

    /**
     * Resolve texture path to atlas UV coordinates.
     * @param texturePath Texture path (e.g., "block/dirt")
     * @param elementUV UV coordinates in texture space [0-16]
     * @param outUVs Output UV coordinates in atlas space [0-1] for 4 corners
     */
    void ResolveTextureUVs(
        const std::string& texturePath, const glm::vec4& elementUV,
        const Graphics::TextureAtlas* textureAtlas, glm::vec2 outUVs[4]);

    /**
     * Apply UV rotation (0, 90, 180, 270 degrees).
     */
    void ApplyUVRotation(int rotation, glm::vec2 uvs[4]);

    /**
     * Generate cache key for a model (for caching baked models).
     */
    std::string GenerateCacheKey(const BlockModel& model);

private:
    /// Cache of baked models
    /// Key: cache key (model hash)
    /// Value: unique pointer to baked model
    std::unordered_map<std::string, std::unique_ptr<BakedModel>> m_BakedModelCache;

    /// TextureManager for looking up texture positions in atlas
    const TextureManager* m_TextureManager = nullptr;
};

} // namespace Resources
