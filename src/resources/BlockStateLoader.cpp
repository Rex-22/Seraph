//
// Created by Claude Code on 2025/11/11.
//

#include "BlockStateLoader.h"

#include "BlockModelLoader.h"
#include "ModelBakery.h"
#include "core/Log.h"
#include "world/Block.h"
#include "world/BlockState.h"

#include <fstream>
#include <nlohmann/json.hpp>
#include <sstream>

using json = nlohmann::json;

namespace Resources
{

BlockStateLoader::BlockStateLoader(
    BlockModelLoader* modelLoader, ModelBakery* bakery,
    const Graphics::TextureAtlas* textureAtlas)
    : m_ModelLoader(modelLoader)
    , m_Bakery(bakery)
    , m_TextureAtlas(textureAtlas)
{
}

std::vector<World::BlockState*> BlockStateLoader::LoadBlockState(
    const std::string& blockstatePath, World::Block* block)
{
    // Check cache first
    auto cacheIt = m_BlockStateCache.find(blockstatePath);
    if (cacheIt != m_BlockStateCache.end()) {
        // Return raw pointers from cached unique_ptrs
        std::vector<World::BlockState*> states;
        for (const auto& statePtr : cacheIt->second) {
            states.push_back(statePtr.get());
        }
        return states;
    }

    // Construct full file path
    std::string jsonPath = ASSET_PATH + m_BlockStatesBasePath + blockstatePath + ".json";

    // Parse JSON
    std::unordered_map<std::string, std::vector<BlockStateVariant>> variants;
    if (!ParseBlockStateJson(jsonPath, variants)) {
        CORE_ERROR("Failed to parse blockstate: {}", jsonPath);
        return {};
    }

    // Create BlockState objects for each variant
    std::vector<std::unique_ptr<World::BlockState>> states;

    for (const auto& [propertyString, variantList] : variants) {
        // For now, use the first variant (TODO: implement random selection with weights)
        if (!variantList.empty()) {
            const BlockStateVariant& variant = variantList[0];
            auto state =
                std::unique_ptr<World::BlockState>(CreateBlockState(
                    block, m_NextStateId++, propertyString, variant));
            if (state) {
                states.push_back(std::move(state));
            }
        }
    }

    // Store in cache
    std::vector<World::BlockState*> statePointers;
    for (const auto& state : states) {
        statePointers.push_back(state.get());
    }
    m_BlockStateCache[blockstatePath] = std::move(states);

    CORE_INFO(
        "Loaded blockstate {}: {} variants", blockstatePath, statePointers.size());

    return statePointers;
}

void BlockStateLoader::ClearCache()
{
    m_BlockStateCache.clear();
    m_NextStateId = 0;
}

bool BlockStateLoader::ParseBlockStateJson(
    const std::string& jsonPath,
    std::unordered_map<std::string, std::vector<BlockStateVariant>>& outVariants)
{
    try {
        // Open file
        std::ifstream file(jsonPath);
        if (!file.is_open()) {
            CORE_ERROR("Failed to open blockstate file: {}", jsonPath);
            return false;
        }

        // Parse JSON
        json j;
        file >> j;

        // Parse variants
        if (!j.contains("variants")) {
            CORE_ERROR("Blockstate missing 'variants' field: {}", jsonPath);
            return false;
        }

        for (auto& [propertyString, variantJson] : j["variants"].items()) {
            std::vector<BlockStateVariant> variantList;

            // Check if variant is an array (multiple random variants) or single object
            if (variantJson.is_array()) {
                // Multiple variants with random selection
                for (auto& varJson : variantJson) {
                    BlockStateVariant variant;
                    variant.model = varJson.value("model", "");
                    variant.rotationX = varJson.value("x", 0);
                    variant.rotationY = varJson.value("y", 0);
                    variant.uvlock = varJson.value("uvlock", false);
                    variant.weight = varJson.value("weight", 1);
                    variantList.push_back(variant);
                }
            } else {
                // Single variant
                BlockStateVariant variant;
                variant.model = variantJson.value("model", "");
                variant.rotationX = variantJson.value("x", 0);
                variant.rotationY = variantJson.value("y", 0);
                variant.uvlock = variantJson.value("uvlock", false);
                variant.weight = variantJson.value("weight", 1);
                variantList.push_back(variant);
            }

            outVariants[propertyString] = variantList;
        }

        return true;

    } catch (const std::exception& e) {
        CORE_ERROR("Exception parsing blockstate JSON {}: {}", jsonPath, e.what());
        return false;
    }
}

World::BlockState* BlockStateLoader::CreateBlockState(
    World::Block* block, uint16_t stateId, const std::string& propertyString,
    const BlockStateVariant& variant)
{
    // Create block state
    auto* state = new World::BlockState(block, stateId);

    // Parse and set properties
    if (!propertyString.empty()) {
        std::unordered_map<std::string, std::string> properties;
        ParsePropertyString(propertyString, properties);
        for (const auto& [key, value] : properties) {
            state->SetProperty(key, value);
        }
    }

    // Load and bake model
    BlockModel* model = m_ModelLoader->LoadModel(variant.model);
    if (!model) {
        CORE_ERROR("Failed to load model for variant: {}", variant.model);
        delete state;
        return nullptr;
    }

    BakedModel* bakedModel = m_Bakery->BakeModel(*model, m_TextureAtlas);
    if (!bakedModel) {
        CORE_ERROR("Failed to bake model: {}", variant.model);
        delete state;
        return nullptr;
    }

    // TODO: Apply rotations (x, y) to baked model if needed
    // For now, we just use the baked model as-is

    state->SetBakedModel(bakedModel);

    return state;
}

void BlockStateLoader::ParsePropertyString(
    const std::string& propertyString,
    std::unordered_map<std::string, std::string>& outProperties)
{
    if (propertyString.empty()) {
        return;
    }

    // Split by comma
    std::istringstream iss(propertyString);
    std::string pair;

    while (std::getline(iss, pair, ',')) {
        // Split by equals
        size_t equalsPos = pair.find('=');
        if (equalsPos != std::string::npos) {
            std::string key = pair.substr(0, equalsPos);
            std::string value = pair.substr(equalsPos + 1);
            outProperties[key] = value;
        }
    }
}

} // namespace Resources
