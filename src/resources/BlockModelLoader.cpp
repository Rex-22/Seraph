//
// Created by Claude Code on 2025/11/11.
//

#include "BlockModelLoader.h"

#include "core/Log.h"

#include <config.h>
#include <fstream>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

namespace Resources
{

BlockModel* BlockModelLoader::LoadModel(const std::string& modelPath)
{
    // Check cache first
    auto cacheIt = m_ModelCache.find(modelPath);
    if (cacheIt != m_ModelCache.end()) {
        return cacheIt->second.get();
    }

    // Load model from file
    BlockModel* model = LoadModelRaw(modelPath);
    if (!model) {
        CORE_ERROR("Failed to load model: {}", modelPath);
        return GetFallbackModel();
    }

    // Resolve parent model if present
    if (model->HasParent()) {
        ResolveParent(*model);
    }

    // Resolve texture variables
    ResolveTextures(*model);

    // Mark as resolved
    model->SetResolved(true);

    return model;
}

void BlockModelLoader::ClearCache()
{
    m_ModelCache.clear();
    m_FallbackModel.reset();
}

bool BlockModelLoader::IsCached(const std::string& modelPath) const
{
    return m_ModelCache.find(modelPath) != m_ModelCache.end();
}

BlockModel* BlockModelLoader::GetFallbackModel()
{
    if (!m_FallbackModel) {
        CreateFallbackModel();
    }
    return m_FallbackModel.get();
}

BlockModel* BlockModelLoader::LoadModelRaw(const std::string& modelPath)
{
    // Construct full file path
    std::string jsonPath = ASSET_PATH + m_ModelsBasePath + modelPath + ".json";

    // Create new model
    auto model = std::make_unique<BlockModel>();

    // Parse JSON
    if (!ParseModelJson(jsonPath, *model)) {
        CORE_ERROR("Failed to parse model JSON: {}", jsonPath);
        return nullptr;
    }

    // Store in cache
    BlockModel* modelPtr = model.get();
    m_ModelCache[modelPath] = std::move(model);

    return modelPtr;
}

bool BlockModelLoader::ParseModelJson(const std::string& jsonPath, BlockModel& outModel)
{
    try {
        // Open file
        std::ifstream file(jsonPath);
        if (!file.is_open()) {
            CORE_ERROR("Failed to open file: {}", jsonPath);
            return false;
        }

        // Parse JSON
        json j;
        file >> j;

        // Parse parent
        if (j.contains("parent")) {
            outModel.SetParent(j["parent"].get<std::string>());
        }

        // Parse ambient occlusion
        if (j.contains("ambientocclusion")) {
            outModel.SetAmbientOcclusion(j["ambientocclusion"].get<bool>());
        }

        // Parse textures
        if (j.contains("textures")) {
            for (auto& [key, value] : j["textures"].items()) {
                outModel.AddTexture(key, value.get<std::string>());
            }
        }

        // Parse elements
        if (j.contains("elements")) {
            for (auto& elemJson : j["elements"]) {
                BlockElement element;

                // Parse from position
                if (elemJson.contains("from")) {
                    auto from = elemJson["from"];
                    element.from =
                        glm::vec3(from[0].get<float>(), from[1].get<float>(), from[2].get<float>());
                }

                // Parse to position
                if (elemJson.contains("to")) {
                    auto to = elemJson["to"];
                    element.to =
                        glm::vec3(to[0].get<float>(), to[1].get<float>(), to[2].get<float>());
                }

                // Parse rotation (optional)
                if (elemJson.contains("rotation")) {
                    auto rotJson = elemJson["rotation"];
                    BlockElement::Rotation rotation;

                    if (rotJson.contains("origin")) {
                        auto origin = rotJson["origin"];
                        rotation.origin = glm::vec3(
                            origin[0].get<float>(), origin[1].get<float>(),
                            origin[2].get<float>());
                    }

                    if (rotJson.contains("axis")) {
                        rotation.axis = rotJson["axis"].get<std::string>();
                    }

                    if (rotJson.contains("angle")) {
                        rotation.angle = rotJson["angle"].get<float>();
                    }

                    if (rotJson.contains("rescale")) {
                        rotation.rescale = rotJson["rescale"].get<bool>();
                    }

                    element.rotation = rotation;
                }

                // Parse shade
                if (elemJson.contains("shade")) {
                    element.shade = elemJson["shade"].get<bool>();
                }

                // Parse faces
                if (elemJson.contains("faces")) {
                    for (auto& [direction, faceJson] : elemJson["faces"].items()) {
                        BlockFace face;

                        // Parse texture (required)
                        if (faceJson.contains("texture")) {
                            face.texture = faceJson["texture"].get<std::string>();
                        }

                        // Parse UV (optional)
                        if (faceJson.contains("uv")) {
                            auto uv = faceJson["uv"];
                            face.uv = glm::vec4(
                                uv[0].get<float>(), uv[1].get<float>(), uv[2].get<float>(),
                                uv[3].get<float>());
                        }

                        // Parse cullface (optional)
                        if (faceJson.contains("cullface")) {
                            face.cullface = faceJson["cullface"].get<std::string>();
                        }

                        // Parse rotation (optional)
                        if (faceJson.contains("rotation")) {
                            face.rotation = faceJson["rotation"].get<int>();
                        }

                        // Parse tintindex (optional)
                        if (faceJson.contains("tintindex")) {
                            face.tintindex = faceJson["tintindex"].get<int>();
                        }

                        element.AddFace(direction, face);
                    }
                }

                outModel.AddElement(element);
            }
        }

        return true;

    } catch (const std::exception& e) {
        CORE_ERROR("Exception while parsing JSON {}: {}", jsonPath, e.what());
        return false;
    }
}

void BlockModelLoader::ResolveParent(BlockModel& model)
{
    // Get parent model
    std::string parentPath = model.GetParent();
    if (parentPath.empty()) {
        return;
    }

    // Load parent model (recursively if needed)
    BlockModel* parent = LoadModel(parentPath);
    if (!parent) {
        CORE_ERROR("Failed to load parent model: {}", parentPath);
        return;
    }

    // Merge textures (child overrides parent)
    for (const auto& [key, value] : parent->GetTextures()) {
        if (!model.HasTexture(key)) {
            model.AddTexture(key, value);
        }
    }

    // Copy elements if child has none
    if (model.GetElements().empty() && !parent->GetElements().empty()) {
        for (const auto& element : parent->GetElements()) {
            model.AddElement(element);
        }
    }

    // Inherit ambient occlusion if not explicitly set
    // Note: This is a simplification; in Minecraft, AO is always explicitly set or defaulted
    // Here we'll just keep the child's value
}

void BlockModelLoader::ResolveTextures(BlockModel& model)
{
    std::unordered_map<std::string, bool> visited;

    // Resolve all texture variables
    auto& textures = model.GetTextures();
    std::unordered_map<std::string, std::string> resolvedTextures;

    for (auto& [key, value] : textures) {
        visited.clear();
        std::string resolved = ResolveTextureVariable(value, model, visited);
        if (!resolved.empty()) {
            resolvedTextures[key] = resolved;
        } else {
            resolvedTextures[key] = value; // Keep original if can't resolve
        }
    }

    // Update model with resolved textures
    for (const auto& [key, value] : resolvedTextures) {
        model.AddTexture(key, value);
    }

    // Resolve texture references in elements
    for (auto& element : model.GetElements()) {
        for (auto& [direction, face] : element.faces) {
            if (face.texture.empty()) {
                continue;
            }

            visited.clear();
            std::string resolved = ResolveTextureVariable(face.texture, model, visited);
            if (!resolved.empty()) {
                face.texture = resolved;
            }
        }
    }
}

std::string BlockModelLoader::ResolveTextureVariable(
    const std::string& variable, const BlockModel& model,
    std::unordered_map<std::string, bool>& visited)
{
    // If not a variable reference, return as-is
    if (variable.empty() || variable[0] != '#') {
        return variable;
    }

    // Check for circular reference
    if (visited.find(variable) != visited.end()) {
        CORE_ERROR("Circular texture variable reference detected: {}", variable);
        return "";
    }
    visited[variable] = true;

    // Remove # prefix
    std::string varName = variable.substr(1);

    // Look up in model's textures
    std::string value = model.GetTexture(varName);
    if (value.empty()) {
        CORE_WARN("Unresolved texture variable: {}", variable);
        return "";
    }

    // Recursively resolve if it's another variable
    return ResolveTextureVariable(value, model, visited);
}

void BlockModelLoader::CreateFallbackModel()
{
    m_FallbackModel = std::make_unique<BlockModel>();

    // Set magenta texture for all sides
    m_FallbackModel->AddTexture("all", "missing");

    // Create a full cube element
    BlockElement element(glm::vec3(0, 0, 0), glm::vec3(16, 16, 16));

    // Add faces for all directions
    const std::vector<std::string> directions = {"down",  "up",   "north",
                                                  "south", "west", "east"};
    for (const auto& dir : directions) {
        BlockFace face;
        face.texture = "#all";
        face.cullface = dir;
        element.AddFace(dir, face);
    }

    m_FallbackModel->AddElement(element);
    m_FallbackModel->SetResolved(true);
}

} // namespace Resources
