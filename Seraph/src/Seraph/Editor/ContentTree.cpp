#include "ContentTree.h"

#include "Seraph/Asset/AssetManager.h"
#include "Seraph/Asset/EditorAssetManager.h"
#include "Seraph/Core/FileSystem.h"

#include <algorithm>
#include <cctype>

namespace Seraph
{

namespace
{

namespace fs = std::filesystem;

bool IsShaderSourceDir(const fs::path& absDir)
{
    std::error_code ec;
    return fs::exists(absDir / "varying.def.sc", ec);
}

// Case-insensitive name compare for stable, human-friendly ordering.
bool NameLess(const std::string& a, const std::string& b)
{
    return std::lexicographical_compare(
        a.begin(), a.end(), b.begin(), b.end(), [](char x, char y) {
            return std::tolower(static_cast<unsigned char>(x)) <
                   std::tolower(static_cast<unsigned char>(y));
        });
}

void BuildFolder(
    ContentFolder& folder, const fs::path& absDir, const fs::path& root,
    EditorAssetManager& ed)
{
    std::error_code ec;
    for (fs::directory_iterator it(absDir, ec), end; it != end; it.increment(ec)) {
        if (ec)
            break;

        const fs::path abs = it->path();
        const std::string name = abs.filename().string();
        if (name.empty() || name[0] == '.') // hidden files / .DS_Store
            continue;

        if (it->is_directory(ec)) {
            if (IsShaderSourceDir(abs))
                continue; // represented by its cooked .sshader in shaders/
            ContentFolder sub;
            sub.Name = name;
            sub.RelativePath = fs::relative(abs, root, ec);
            BuildFolder(sub, abs, root, ed);
            folder.SubFolders.push_back(std::move(sub));
        } else if (it->is_regular_file(ec)) {
            const AssetType type = EditorAssetManager::GetAssetTypeFromPath(abs);
            if (type == AssetType::None)
                continue; // not an asset (source .sc, .srr, etc.)
            ContentEntry file;
            file.Name = name;
            file.RelativePath = fs::relative(abs, root, ec);
            file.Type = type;
            file.Handle = ed.GetAssetHandleFromFilePath(file.RelativePath);
            file.Missing = ed.GetMetadata(file.Handle).IsMissing;
            folder.Files.push_back(std::move(file));
        }
    }

    std::sort(
        folder.SubFolders.begin(), folder.SubFolders.end(),
        [](const ContentFolder& a, const ContentFolder& b) {
            return NameLess(a.Name, b.Name);
        });
    std::sort(
        folder.Files.begin(), folder.Files.end(),
        [](const ContentEntry& a, const ContentEntry& b) {
            return NameLess(a.Name, b.Name);
        });
}

} // namespace

void ContentTree::Clear()
{
    m_Root = ContentFolder{};
}

void ContentTree::Rebuild()
{
    Clear();

    Ref<EditorAssetManager> ed = AssetManager::Get().As<EditorAssetManager>();
    if (!ed || !FileSystem::HasProjectRoot())
        return;

    const fs::path root = FileSystem::ProjectRoot(); // == the active asset root
    std::error_code ec;
    if (!fs::is_directory(root, ec))
        return;

    m_Root.Name = root.filename().string();
    m_Root.RelativePath.clear();
    BuildFolder(m_Root, root, root, *ed);
}

const ContentFolder* ContentTree::FindFolder(const std::filesystem::path& relative) const
{
    const ContentFolder* current = &m_Root;
    if (relative.empty())
        return current;

    for (const auto& part : relative) {
        const std::string component = part.string();
        const ContentFolder* next = nullptr;
        for (const ContentFolder& sub : current->SubFolders) {
            if (sub.Name == component) {
                next = &sub;
                break;
            }
        }
        if (next == nullptr)
            return nullptr;
        current = next;
    }
    return current;
}

} // namespace Seraph
