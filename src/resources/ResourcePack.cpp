//
// Created by Claude Code on 2025/11/14.
//

#include "ResourcePack.h"

#include "core/Log.h"

#include <filesystem>
#include <fstream>
#include <nlohmann/json.hpp>

namespace fs = std::filesystem;
using json = nlohmann::json;

namespace Resources
{

bool ResourcePack::LoadPackInfo(const std::string& packPath, ResourcePackInfo& outInfo)
{
    try {
        std::string mcmetaPath = packPath + "/pack.mcmeta";

        if (!fs::exists(mcmetaPath)) {
            CORE_ERROR("pack.mcmeta not found in: {}", packPath);
            return false;
        }

        // Read JSON file
        std::ifstream file(mcmetaPath);
        if (!file.is_open()) {
            CORE_ERROR("Failed to open pack.mcmeta: {}", mcmetaPath);
            return false;
        }

        json root = json::parse(file);

        // Parse pack metadata
        if (!root.contains("pack")) {
            CORE_ERROR("pack.mcmeta missing 'pack' object");
            return false;
        }

        json pack = root["pack"];

        outInfo.packFormat = pack.value("pack_format", 0);
        outInfo.description = pack.value("description", "");
        outInfo.path = packPath;

        // Extract pack name from directory
        fs::path packFsPath(packPath);
        outInfo.name = packFsPath.filename().string();

        CORE_INFO(
            "Loaded resource pack: {} (format {}, description: \"{}\")", outInfo.name,
            outInfo.packFormat, outInfo.description);

        return true;

    } catch (const std::exception& e) {
        CORE_ERROR("Exception loading pack.mcmeta from {}: {}", packPath, e.what());
        return false;
    }
}

bool ResourcePack::IsValidResourcePack(const std::string& packPath)
{
    // Check if pack.mcmeta exists
    std::string mcmetaPath = packPath + "/pack.mcmeta";
    if (!fs::exists(mcmetaPath)) {
        return false;
    }

    // Check if assets directory exists
    std::string assetsPath = packPath + "/assets";
    if (!fs::exists(assetsPath)) {
        return false;
    }

    return true;
}

} // namespace Resources
