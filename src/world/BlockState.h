//
// Created by Claude Code on 2025/11/11.
//

#pragma once

#include <cstdint>
#include <string>
#include <unordered_map>

namespace World
{
class Block;

// BlockStateId is used for efficient chunk storage (like Minecraft)
typedef uint16_t BlockStateId;
}

namespace Resources
{
class BakedModel;
}

namespace World
{

/**
 * Represents a specific state of a block with properties.
 * Examples:
 * - dirt: no properties (default state)
 * - furnace[facing=north]: one property
 * - door[facing=north,open=true]: multiple properties
 */
class BlockState
{
public:
    BlockState() = default;

    /**
     * Create a block state with a base block and state ID.
     */
    BlockState(const Block* block, uint16_t stateId);

    /**
     * Get the base block type.
     */
    const Block* GetBlock() const { return m_Block; }

    /**
     * Get the unique state ID.
     * Used for efficient storage in chunks.
     */
    uint16_t GetStateId() const { return m_StateId; }

    /**
     * Set a state property.
     * @param key Property name (e.g., "facing")
     * @param value Property value (e.g., "north")
     */
    void SetProperty(const std::string& key, const std::string& value);

    /**
     * Get a state property value.
     * Returns empty string if property doesn't exist.
     */
    std::string GetProperty(const std::string& key) const;

    /**
     * Check if a property exists.
     */
    bool HasProperty(const std::string& key) const;

    /**
     * Get all properties.
     */
    const std::unordered_map<std::string, std::string>& GetProperties() const
    {
        return m_Properties;
    }

    /**
     * Get the baked model for this state.
     * The model is selected based on state properties.
     */
    Resources::BakedModel* GetBakedModel() const { return m_BakedModel; }

    /**
     * Set the baked model for this state.
     */
    void SetBakedModel(Resources::BakedModel* model) { m_BakedModel = model; }

    /**
     * Build property string for lookup (e.g., "facing=north,open=true").
     * Properties are sorted alphabetically for consistent keys.
     */
    std::string BuildPropertyString() const;

    /**
     * Check if this is the default state (no properties).
     */
    bool IsDefaultState() const { return m_Properties.empty(); }

private:
    /// Base block type
    const Block* m_Block = nullptr;

    /// Unique state ID (for chunk storage)
    uint16_t m_StateId = 0;

    /// State properties (e.g., "facing": "north")
    std::unordered_map<std::string, std::string> m_Properties;

    /// Cached baked model for this state
    Resources::BakedModel* m_BakedModel = nullptr;
};

} // namespace World
