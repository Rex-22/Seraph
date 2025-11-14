//
// Created by Claude Code on 2025/11/14.
//

#include "AnimationParser.h"

#include "core/Log.h"

#include <filesystem>
#include <fstream>
#include <nlohmann/json.hpp>

namespace fs = std::filesystem;
using json = nlohmann::json;

namespace Resources
{

bool AnimationParser::ParseMcmeta(const std::string& mcmetaPath, AnimationMetadata& outMetadata)
{
    try {
        // Read JSON file
        std::ifstream file(mcmetaPath);
        if (!file.is_open()) {
            CORE_ERROR("Failed to open .mcmeta file: {}", mcmetaPath);
            return false;
        }

        json root = json::parse(file);

        // Check if animation data exists
        if (!root.contains("animation")) {
            CORE_WARN(".mcmeta file has no animation data: {}", mcmetaPath);
            return false;
        }

        json anim = root["animation"];

        // Parse interpolate flag
        outMetadata.interpolate = anim.value("interpolate", false);

        // Parse frametime (default 1 tick)
        outMetadata.frametime = anim.value("frametime", 1);

        // Parse frames array (if present)
        if (anim.contains("frames") && anim["frames"].is_array()) {
            for (const auto& frameEntry : anim["frames"]) {
                if (frameEntry.is_number_integer()) {
                    // Simple frame index
                    outMetadata.frames.push_back(frameEntry.get<int>());
                    outMetadata.frameTimes.push_back(outMetadata.frametime); // Use default
                } else if (frameEntry.is_object()) {
                    // Frame with custom time
                    int index = frameEntry.value("index", 0);
                    int time = frameEntry.value("time", outMetadata.frametime);
                    outMetadata.frames.push_back(index);
                    outMetadata.frameTimes.push_back(time);
                }
            }
        }

        CORE_INFO(
            "Parsed .mcmeta: {} frames, frametime {} ticks, interpolate {}",
            outMetadata.frames.size(), outMetadata.frametime, outMetadata.interpolate);

        return true;

    } catch (const std::exception& e) {
        CORE_ERROR("Exception parsing .mcmeta file {}: {}", mcmetaPath, e.what());
        return false;
    }
}

std::string AnimationParser::FindMcmetaFile(const std::string& texturePath)
{
    // .mcmeta file has the same name as the texture with .mcmeta appended
    std::string mcmetaPath = texturePath + ".mcmeta";

    if (fs::exists(mcmetaPath)) {
        return mcmetaPath;
    }

    return ""; // Not found
}

} // namespace Resources
