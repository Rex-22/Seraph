//
// Created by Claude Code on 2025/11/11.
//

#pragma once

#include "BlockModel.h"

#include <memory>
#include <string>
#include <unordered_map>

namespace Resources
{

/**
 * Loads and parses Minecraft-compatible block model JSON files.
 * Handles parent model inheritance and texture variable resolution.
 * Caches loaded models to avoid re-parsing.
 */
class BlockModelLoader
{
public:
    BlockModelLoader() = default;
    ~BlockModelLoader() = default;

    /**
     * Load a block model from a JSON file.
     * Resolves parent models and texture variables.
     *
     * @param modelPath Model path relative to assets/models/ (e.g., "block/dirt")
     * @return Pointer to loaded model, or nullptr if loading failed
     *
     * The returned pointer is owned by the loader and remains valid until
     * the loader is destroyed or ClearCache() is called.
     */
    BlockModel* LoadModel(const std::string& modelPath);

    /**
     * Clear the model cache.
     * All previously returned model pointers become invalid.
     */
    void ClearCache();

    /**
     * Get the number of cached models.
     */
    size_t GetCacheSize() const { return m_ModelCache.size(); }

    /**
     * Check if a model is cached.
     */
    bool IsCached(const std::string& modelPath) const;

    /**
     * Get the fallback model used when loading fails.
     * Returns a simple magenta cube.
     */
    BlockModel* GetFallbackModel();

private:
    /**
     * Load a model from file without resolving parent or textures.
     * @return Pointer to unresolved model
     */
    BlockModel* LoadModelRaw(const std::string& modelPath);

    /**
     * Parse a JSON file into a BlockModel.
     * Does not resolve parent or texture variables.
     */
    bool ParseModelJson(const std::string& jsonPath, BlockModel& outModel);

    /**
     * Resolve parent model inheritance.
     * Merges textures and copies elements from parent if not defined in child.
     */
    void ResolveParent(BlockModel& model);

    /**
     * Resolve texture variable references.
     * Converts all #variable references to concrete texture paths.
     */
    void ResolveTextures(BlockModel& model);

    /**
     * Resolve a single texture variable, following the chain of references.
     * @param variable Variable to resolve (e.g., "#side")
     * @param model Model containing texture definitions
     * @param visited Set of visited variables to detect circular references
     * @return Resolved texture path, or empty string if unresolved
     */
    std::string ResolveTextureVariable(
        const std::string& variable, const BlockModel& model,
        std::unordered_map<std::string, bool>& visited);

    /**
     * Create the fallback model (magenta cube).
     */
    void CreateFallbackModel();

private:
    /// Cache of loaded models
    /// Key: model path (e.g., "block/dirt")
    /// Value: unique pointer to loaded model
    std::unordered_map<std::string, std::unique_ptr<BlockModel>> m_ModelCache;

    /// Fallback model used when loading fails
    std::unique_ptr<BlockModel> m_FallbackModel;

    /// Base path for models directory
    std::string m_ModelsBasePath = "models/";
};

} // namespace Resources
