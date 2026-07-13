#include "Project.h"

#include "Seraph/Core/Log.h"

#include <yaml-cpp/yaml.h>

#include <fstream>

namespace Seraph
{

bool Project::Save(const Project& project, const std::filesystem::path& path)
{
    YAML::Emitter out;
    out << YAML::BeginMap;
    out << YAML::Key << "Project" << YAML::Value << project.Name;
    out << YAML::Key << "AssetDirectory" << YAML::Value
        << project.AssetDirectory.generic_string();
    out << YAML::Key << "StartupScene" << YAML::Value
        << static_cast<u64>(project.StartupScene);
    out << YAML::Key << "Packs" << YAML::Value << YAML::BeginSeq;
    for (const auto& pack : project.Packs)
        out << pack.generic_string();
    out << YAML::EndSeq;
    out << YAML::Key << "Window" << YAML::Value << YAML::BeginMap;
    out << YAML::Key << "Width" << YAML::Value << project.WindowWidth;
    out << YAML::Key << "Height" << YAML::Value << project.WindowHeight;
    out << YAML::Key << "Title" << YAML::Value << project.WindowTitle;
    out << YAML::Key << "VSync" << YAML::Value << project.WindowVSync;
    out << YAML::EndMap;
    out << YAML::EndMap;

    std::ofstream fout(path);
    if (!fout) {
        SP_CORE_ERROR_TAG(
            "Project", "Could not write project to '{}'", path.string());
        return false;
    }
    fout << out.c_str();
    return true;
}

std::optional<Project> Project::Load(const std::filesystem::path& path)
{
    if (!std::filesystem::exists(path)) {
        SP_CORE_ERROR_TAG("Project", "Project file not found: '{}'", path.string());
        return std::nullopt;
    }

    YAML::Node data;
    try {
        data = YAML::LoadFile(path.string());
    } catch (const std::exception& e) {
        SP_CORE_ERROR_TAG(
            "Project", "Failed to parse project '{}': {}", path.string(), e.what());
        return std::nullopt;
    }

    Project project;
    if (data["Project"])
        project.Name = data["Project"].as<std::string>();
    if (data["AssetDirectory"])
        project.AssetDirectory = data["AssetDirectory"].as<std::string>();
    if (data["StartupScene"])
        project.StartupScene = data["StartupScene"].as<u64>();
    if (const YAML::Node packs = data["Packs"]; packs && packs.IsSequence())
        for (const auto& pack : packs)
            project.Packs.emplace_back(pack.as<std::string>());
    if (const YAML::Node window = data["Window"]) {
        project.WindowWidth = window["Width"].as<int>(project.WindowWidth);
        project.WindowHeight = window["Height"].as<int>(project.WindowHeight);
        project.WindowTitle = window["Title"].as<std::string>(project.WindowTitle);
        project.WindowVSync = window["VSync"].as<bool>(project.WindowVSync);
    }

    SP_CORE_INFO_TAG(
        "Project", "Loaded project '{}' (assets='{}', startupScene={}, packs={})",
        project.Name, project.AssetDirectory.generic_string(),
        static_cast<u64>(project.StartupScene), project.Packs.size());
    return project;
}

} // namespace Seraph
