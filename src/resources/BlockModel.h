//
// Created by Claude Code on 2025/11/11.
//

#pragma once

#include "BlockElement.h"

#include <string>
#include <unordered_map>
#include <vector>

namespace Resources
{

/**
 * Represents a complete block model loaded from JSON.
 * Supports parent model inheritance and texture variable resolution.
 */
class BlockModel
{
public:
    BlockModel() = default;

    /**
     * Set the parent model path.
     * Parent models provide default textures and elements.
     */
    void SetParent(const std::string& parentPath) { m_Parent = parentPath; }

    /**
     * Get the parent model path.
     */
    const std::string& GetParent() const { return m_Parent; }

    /**
     * Check if this model has a parent.
     */
    bool HasParent() const { return !m_Parent.empty(); }

    /**
     * Enable or disable ambient occlusion for this model.
     */
    void SetAmbientOcclusion(bool enabled) { m_AmbientOcclusion = enabled; }

    /**
     * Check if ambient occlusion is enabled.
     */
    bool IsAmbientOcclusion() const { return m_AmbientOcclusion; }

    /**
     * Add a texture variable definition.
     * @param variable Variable name (without '#', e.g., "side")
     * @param texture Texture path or another variable reference
     */
    void AddTexture(const std::string& variable, const std::string& texture)
    {
        m_Textures[variable] = texture;
    }

    /**
     * Get texture for a variable.
     * Returns empty string if variable not found.
     */
    std::string GetTexture(const std::string& variable) const
    {
        auto it = m_Textures.find(variable);
        return it != m_Textures.end() ? it->second : "";
    }

    /**
     * Check if texture variable exists.
     */
    bool HasTexture(const std::string& variable) const
    {
        return m_Textures.find(variable) != m_Textures.end();
    }

    /**
     * Get all texture variables.
     */
    const std::unordered_map<std::string, std::string>& GetTextures() const
    {
        return m_Textures;
    }

    /**
     * Add an element to this model.
     */
    void AddElement(const BlockElement& element) { m_Elements.push_back(element); }

    /**
     * Get all elements in this model.
     */
    const std::vector<BlockElement>& GetElements() const { return m_Elements; }

    /**
     * Get mutable elements for modification.
     */
    std::vector<BlockElement>& GetElements() { return m_Elements; }

    /**
     * Check if this model has been resolved (parent inheritance complete).
     */
    bool IsResolved() const { return m_IsResolved; }

    /**
     * Mark this model as resolved.
     * Should be called after parent inheritance and texture resolution.
     */
    void SetResolved(bool resolved) { m_IsResolved = resolved; }

    /**
     * Clear all data in this model.
     */
    void Clear()
    {
        m_Parent.clear();
        m_Textures.clear();
        m_Elements.clear();
        m_AmbientOcclusion = true;
        m_IsResolved = false;
    }

private:
    /// Parent model path (e.g., "block/cube")
    std::string m_Parent;

    /// Texture variable definitions
    /// Key: variable name (e.g., "side")
    /// Value: texture path or variable reference (e.g., "block/dirt" or "#all")
    std::unordered_map<std::string, std::string> m_Textures;

    /// Cuboid elements that make up this model
    std::vector<BlockElement> m_Elements;

    /// Enable ambient occlusion calculation
    bool m_AmbientOcclusion = true;

    /// True if parent inheritance and texture resolution is complete
    bool m_IsResolved = false;
};

} // namespace Resources
