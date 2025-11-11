///
// Created by Claude Code on 2025/11/11.
//

#pragma once

#include <config.h>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace World
{
class Block;
class BlockState;
} // namespace World

namespace Resources
{
class BlockModelLoader;
class ModelBakery;
class BakedModel;
} // namespace Resources

namespace Graphics
{
class TextureAtlas;
}

namespace Resources
{

/**
 * Variant definition from blockstate JSON.
 * Maps property combinations to model references with transformations.
 */
struct BlockStateVariant
{
    /// Model path (e.g., "block/dirt")
    std::string model;

    /// X-axis rotation (0, 90, 180, 270)
    int rotationX = 0;

    /// Y-axis rotation (0, 90, 180, 270)
    int rotationY = 0;

    /// Lock UVs when rotating
    bool uvlock = false;

    /// Selection weight for random variants (default: 1)
    int weight = 1;
};

/**
 * Loads blockstate JSON files and creates BlockState objects.
 * Parses variants and maps them to baked models.
 */
class BlockStateLoader
{
public:
    BlockStateLoader(
        BlockModelLoader* modelLoader, ModelBakery* bakery,
        const Graphics::TextureAtlas* textureAtlas);

    ~BlockStateLoader() = default;

    /**
     * Load a blockstate from JSON file.
     * Creates BlockState objects for all variants.
     *
     * @param blockstatePath Path relative to assets/blockstates/ (e.g., "dirt")
     * @param block Base block to associate with states
     * @return Vector of block states (one per variant)
     */
    std::vector<World::BlockState*> LoadBlockState(
        const std::string& blockstatePath, World::Block* block);

    /**
     * Clear all loaded block states.
     */
    void ClearCache();

    /**
     * Get the number of loaded block states.
     */
    size_t GetCacheSize() const { return m_BlockStateCache.size(); }

private:
    /**
     * Parse blockstate JSON file.
     * Returns map of property strings to variant definitions.
     */
    bool ParseBlockStateJson(
        const std::string& jsonPath,
        std::unordered_map<std::string, std::vector<BlockStateVariant>>& outVariants);

    /**
     * Create a BlockState from a variant definition.
     */
    World::BlockState* CreateBlockState(
        World::Block* block, uint16_t stateId, const std::string& propertyString,
        const BlockStateVariant& variant);

    /**
     * Parse property string into key-value map.
     * Example: "facing=north,open=true" -> {"facing": "north", "open": "true"}
     */
    void ParsePropertyString(
        const std::string& propertyString,
        std::unordered_map<std::string, std::string>& outProperties);

private:
    /// Block model loader
    BlockModelLoader* m_ModelLoader;

    /// Model bakery
    ModelBakery* m_Bakery;

    /// Texture atlas for baking
    const Graphics::TextureAtlas* m_TextureAtlas;

    /// Cache of loaded block states
    /// Key: blockstate path (e.g., "dirt")
    /// Value: vector of states (one per variant)
    std::unordered_map<std::string, std::vector<std::unique_ptr<World::BlockState>>>
        m_BlockStateCache;

    /// Next available state ID
    uint16_t m_NextStateId = 0;

    /// Base path for blockstates directory
    std::string m_BlockStatesBasePath = "blockstates/";
};

} // namespace Resources
