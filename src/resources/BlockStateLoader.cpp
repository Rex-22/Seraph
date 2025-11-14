//
// Created by Claude Code on 2025/11/11.
//

#include "BlockStateLoader.h"

#include "BakedModel.h"
#include "BlockModelLoader.h"
#include "ModelBakery.h"
#include "core/Log.h"
#include "world/Block.h"
#include "world/BlockState.h"

#include <cstdlib>
#include <fstream>
#include <glm/gtc/matrix_transform.hpp>
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
        if (variantList.empty()) {
            continue;
        }

        // Select variant based on weights (Minecraft-style)
        const BlockStateVariant* selectedVariant = nullptr;

        if (variantList.size() == 1) {
            // Single variant - use it directly
            selectedVariant = &variantList[0];
        } else {
            // Multiple variants - weighted random selection
            // Calculate total weight
            int totalWeight = 0;
            for (const auto& v : variantList) {
                totalWeight += v.weight;
            }

            // For blockstates with multiple variants, we need to handle selection during chunk generation
            // Each block placement position will use its coordinates to deterministically select a variant
            // For now during loading, xwe create all variants and store them for later selection
            //
            // Current approach: Select based on a simple distribution for the first state
            // In a full implementation, we would create multiple BlockStates (one per variant)
            // and select during chunk generation based on world position hash
            //
            // Simple weighted selection for demonstration (will be improved in Phase 2 completion)
            int randomValue = std::rand() % totalWeight;
            int cumulative = 0;
            selectedVariant = &variantList[0];

            for (const auto& v : variantList) {
                cumulative += v.weight;
                if (randomValue < cumulative) {
                    selectedVariant = &v;
                    break;
                }
            }

            CORE_INFO("Blockstate has {} variants (total weight: {}), selected variant with weight {}",
                variantList.size(), totalWeight, selectedVariant->weight);
        }

        if (selectedVariant) {
            auto state =
                std::unique_ptr<World::BlockState>(CreateBlockState(
                    block, m_NextStateId++, propertyString, *selectedVariant));
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

    // Apply blockstate rotations if specified
    if (variant.rotationX != 0 || variant.rotationY != 0) {
        BakedModel* rotatedModel = ApplyBlockstateRotation(bakedModel, variant);
        if (rotatedModel) {
            bakedModel = rotatedModel;
        }
    }

    state->SetBakedModel(bakedModel);

    return state;
}

Resources::BakedModel* BlockStateLoader::ApplyBlockstateRotation(
    const Resources::BakedModel* source, const BlockStateVariant& variant)
{
    // Create a rotated copy of the baked model
    auto* rotated = new Resources::BakedModel();
    rotated->SetAmbientOcclusion(source->IsAmbientOcclusion());
    rotated->SetHasTransparency(source->HasTransparency());

    // Calculate rotation matrices
    glm::mat4 rotMatrix(1.0f);

    // Apply Y rotation first (around vertical axis)
    if (variant.rotationY != 0) {
        float angleY = glm::radians(static_cast<float>(variant.rotationY));
        rotMatrix = glm::rotate(rotMatrix, angleY, glm::vec3(0, 1, 0));
    }

    // Apply X rotation (around horizontal axis)
    if (variant.rotationX != 0) {
        float angleX = glm::radians(static_cast<float>(variant.rotationX));
        rotMatrix = glm::rotate(rotMatrix, angleX, glm::vec3(1, 0, 0));
    }

    // Rotate each quad
    for (const auto& quad : source->GetQuads()) {
        Resources::BakedQuad rotatedQuad = quad;

        // Rotate vertices around block center (0.5, 0.5, 0.5)
        glm::vec3 center(0.5f, 0.5f, 0.5f);
        for (int i = 0; i < 4; ++i) {
            glm::vec3 vertex = quad.vertices[i] - center;
            glm::vec4 rotatedVertex = rotMatrix * glm::vec4(vertex, 1.0f);
            rotatedQuad.vertices[i] = glm::vec3(rotatedVertex) + center;
        }

        // Rotate normal
        glm::vec4 rotatedNormal = rotMatrix * glm::vec4(quad.normal, 0.0f);
        rotatedQuad.normal = glm::normalize(glm::vec3(rotatedNormal));

        // Update cullface direction after rotation
        // TODO: Implement cullface rotation mapping if needed
        // For now, keep original cullface (may cause incorrect culling on rotated blocks)

        // Handle UVLock: don't rotate UVs if uvlock is true
        if (!variant.uvlock) {
            // UVs stay as-is (default Minecraft behavior)
        }
        // Note: UV lock implementation would counter-rotate UVs to keep them upright

        rotated->AddQuad(rotatedQuad);
    }

    return rotated;
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
