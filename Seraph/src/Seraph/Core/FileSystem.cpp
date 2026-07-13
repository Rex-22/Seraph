#include "FileSystem.h"

#include "Seraph/Core/Log.h"

#include <SDL3/SDL_filesystem.h>
#include <SDL3/SDL_stdinc.h>
#include <config.h>

#include <fstream>
#include <system_error>
#include <utility>

namespace Seraph
{
namespace
{

std::filesystem::path s_ProjectRoot;
std::filesystem::path s_EngineRoot;
std::filesystem::path s_UserConfigDir;

const std::filesystem::path& RootPath(Root root)
{
    switch (root) {
        case Root::Project: return s_ProjectRoot;
        case Root::Engine:  return s_EngineRoot;
        case Root::User:    return s_UserConfigDir;
        case Root::Absolute: break;
    }
    static const std::filesystem::path k_Empty;
    return k_Empty;
}

} // namespace

void FileSystem::Init()
{
    if (const char* base = SDL_GetBasePath()) // owned by SDL; do not free
        s_EngineRoot = base;

    if (char* pref = SDL_GetPrefPath("BitCube", "Seraph")) {
        s_UserConfigDir = pref;
        SDL_free(pref);
    }

    // Default the project root to the compile-time sample assets dir, so a bare
    // run still resolves assets until a project is opened over it.
    s_ProjectRoot = ASSET_PATH;

    SP_CORE_INFO_TAG(
        "FileSystem", "engine='{}' user='{}' project='{}'",
        s_EngineRoot.string(), s_UserConfigDir.string(), s_ProjectRoot.string());
}

void FileSystem::Shutdown()
{
    s_ProjectRoot.clear();
    s_EngineRoot.clear();
    s_UserConfigDir.clear();
}

void FileSystem::SetProjectRoot(std::filesystem::path root)
{
    s_ProjectRoot = std::move(root);
}

bool FileSystem::HasProjectRoot() { return !s_ProjectRoot.empty(); }
const std::filesystem::path& FileSystem::ProjectRoot() { return s_ProjectRoot; }
const std::filesystem::path& FileSystem::EngineRoot() { return s_EngineRoot; }
const std::filesystem::path& FileSystem::UserConfigDir() { return s_UserConfigDir; }

std::filesystem::path FileSystem::Resolve(
    Root root, const std::filesystem::path& relative)
{
    if (root == Root::Absolute || relative.is_absolute())
        return relative;
    return RootPath(root) / relative;
}

bool FileSystem::Read(Root root, const std::filesystem::path& relative, Buffer& out)
{
    const std::filesystem::path full = Resolve(root, relative);
    std::ifstream in(full, std::ios::binary | std::ios::ate);
    if (!in) {
        SP_CORE_ERROR_TAG("FileSystem", "Read failed to open: {}", full.string());
        return false;
    }

    const std::streamsize size = in.tellg();
    if (size < 0) {
        SP_CORE_ERROR_TAG("FileSystem", "Read failed to size: {}", full.string());
        return false;
    }
    in.seekg(0, std::ios::beg);

    Buffer buffer(static_cast<u64>(size));
    if (size > 0 &&
        !in.read(reinterpret_cast<char*>(buffer.Data()), size)) {
        SP_CORE_ERROR_TAG("FileSystem", "Read failed: {}", full.string());
        return false;
    }

    out = std::move(buffer);
    return true;
}

bool FileSystem::Write(
    Root root, const std::filesystem::path& relative, const Buffer& bytes)
{
    const std::filesystem::path full = Resolve(root, relative);

    std::error_code ec;
    if (full.has_parent_path())
        std::filesystem::create_directories(full.parent_path(), ec);

    std::ofstream out(full, std::ios::binary | std::ios::trunc);
    if (!out) {
        SP_CORE_ERROR_TAG(
            "FileSystem", "Write failed to open: {}", full.string());
        return false;
    }
    if (bytes.Size() > 0)
        out.write(
            reinterpret_cast<const char*>(bytes.Data()),
            static_cast<std::streamsize>(bytes.Size()));
    if (!out) {
        SP_CORE_ERROR_TAG("FileSystem", "Write failed: {}", full.string());
        return false;
    }
    return true;
}

bool FileSystem::Exists(Root root, const std::filesystem::path& relative)
{
    std::error_code ec;
    return std::filesystem::exists(Resolve(root, relative), ec);
}

bool FileSystem::CreateDirectories(Root root, const std::filesystem::path& relative)
{
    std::error_code ec;
    std::filesystem::create_directories(Resolve(root, relative), ec);
    return !ec;
}

std::vector<std::filesystem::path> FileSystem::List(
    Root root, const std::filesystem::path& relative, std::string_view extension)
{
    std::vector<std::filesystem::path> result;
    const std::filesystem::path dir = Resolve(root, relative);

    std::error_code ec;
    if (!std::filesystem::is_directory(dir, ec))
        return result;

    for (const auto& entry : std::filesystem::directory_iterator(dir, ec)) {
        if (!extension.empty() && entry.path().extension() != extension)
            continue;
        result.push_back(entry.path());
    }
    return result;
}

} // namespace Seraph
