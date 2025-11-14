//
// Created by Claude Code on 2025/11/11.
//

#pragma once

#include <glm/glm.hpp>
#include <string>
#include <vector>

namespace Resources
{

/**
 * A single quad (4 vertices) compiled from a BlockElement face.
 * Contains all data needed for rendering, including pre-calculated UVs and AO.
 */
struct BakedQuad
{
    /// Vertex positions in model space [0-1] (one block unit)
    /// Layout: [v0, v1, v2, v3] forming a counter-clockwise quad
    glm::vec3 vertices[4];

    /// Texture UV coordinates in atlas space [0-1]
    /// Correspond to vertices array
    glm::vec2 uvs[4];

    /// Overlay texture UV coordinates in atlas space [0-1]
    /// Used for multi-layer rendering (e.g., grass block sides with tintable overlay)
    /// Only used if hasOverlay is true
    glm::vec2 overlayUVs[4];

    /// Whether this quad has an overlay texture
    /// If true, overlayUVs contains valid texture coordinates
    /// If false, overlayUVs should be ignored
    bool hasOverlay = false;

    /// Face normal vector (for lighting calculations)
    glm::vec3 normal;

    /// Direction to cull against ("down", "up", "north", "south", "west", "east")
    /// Empty string means no culling
    std::string cullface;

    /// Per-vertex ambient occlusion weights [0-1]
    /// 0 = fully occluded (dark), 1 = fully lit
    /// Only used if the model has AO enabled
    float aoWeights[4] = {1.0f, 1.0f, 1.0f, 1.0f};

    /// Apply diffuse shading to this quad
    /// False for emissive blocks (lamps, etc.)
    bool shade = true;

    /// Tint layer index
    /// -1: No tint
    /// 0+: Apply biome/block-specific tint (e.g., grass color)
    int tintindex = -1;

    BakedQuad() : normal(0, 1, 0), cullface("") {}
};

/**
 * A complete baked block model ready for rendering.
 * Contains pre-compiled quads with resolved UVs, transforms applied, and AO calculated.
 */
class BakedModel
{
public:
    BakedModel() = default;

    /**
     * Add a baked quad to this model.
     */
    void AddQuad(const BakedQuad& quad) { m_Quads.push_back(quad); }

    /**
     * Get all quads in this model.
     */
    const std::vector<BakedQuad>& GetQuads() const { return m_Quads; }

    /**
     * Get mutable quads for modification.
     */
    std::vector<BakedQuad>& GetQuads() { return m_Quads; }

    /**
     * Check if this model has any transparent quads.
     */
    bool HasTransparency() const { return m_HasTransparency; }

    /**
     * Set transparency flag.
     */
    void SetHasTransparency(bool transparent) { m_HasTransparency = transparent; }

    /**
     * Check if ambient occlusion is enabled for this model.
     */
    bool IsAmbientOcclusion() const { return m_IsAmbientOcclusion; }

    /**
     * Set ambient occlusion flag.
     */
    void SetAmbientOcclusion(bool enabled) { m_IsAmbientOcclusion = enabled; }

    /**
     * Get the number of quads in this model.
     */
    size_t GetQuadCount() const { return m_Quads.size(); }

    /**
     * Check if this model is empty (has no quads).
     */
    bool IsEmpty() const { return m_Quads.empty(); }

    /**
     * Clear all quads from this model.
     */
    void Clear()
    {
        m_Quads.clear();
        m_HasTransparency = false;
        m_IsAmbientOcclusion = true;
    }

private:
    /// Pre-compiled quads ready for rendering
    std::vector<BakedQuad> m_Quads;

    /// True if any quad has transparency (affects render order)
    bool m_HasTransparency = false;

    /// True if ambient occlusion should be calculated for this model
    bool m_IsAmbientOcclusion = true;
};

} // namespace Resources
