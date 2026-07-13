//
// The engine's file access. All reads and writes go through here rather than
// touching std streams / bx readers directly, so file location is decided in one
// place. Paths resolve against a named root (mount):
//
//   Project  - the active project's asset dir (the per-project asset root).
//   Engine   - built-in engine/editor resources shipped next to the executable
//              (read-only). Reserved for editor assets; never packed into a game.
//   User     - a writable per-user config dir (editor recents, etc.).
//   Absolute - the path is used as-is (already absolute).
//
// Bytes come back as a Buffer; nothing here hands out a bx::FileReader, because
// every serializer already consumes a Buffer.
//

#pragma once

#include "Seraph/Core/Buffer.h"

#include <filesystem>
#include <string_view>
#include <vector>

namespace Seraph
{

enum class Root
{
    Project,
    Engine,
    User,
    Absolute,
};

class FileSystem
{
public:
    // Establishes the Engine (executable dir) and User (per-user config) roots.
    // Call after SDL is initialised. The Project root starts unset.
    static void Init();
    static void Shutdown();

    // The active project's asset root. Empty until a project is opened.
    static void SetProjectRoot(std::filesystem::path root);
    static bool HasProjectRoot();
    static const std::filesystem::path& ProjectRoot();
    static const std::filesystem::path& EngineRoot();
    static const std::filesystem::path& UserConfigDir();

    // Absolute path for (root, relative). If `relative` is itself absolute it is
    // returned unchanged regardless of root.
    static std::filesystem::path Resolve(Root root, const std::filesystem::path& relative);

    static bool Read(Root root, const std::filesystem::path& relative, Buffer& out);
    static bool Write(Root root, const std::filesystem::path& relative, const Buffer& bytes);
    static bool Exists(Root root, const std::filesystem::path& relative);
    static bool CreateDirectories(Root root, const std::filesystem::path& relative);

    // Entries directly under (root, relative), optionally filtered by extension
    // (e.g. ".sproj"). Absolute paths.
    static std::vector<std::filesystem::path> List(
        Root root, const std::filesystem::path& relative,
        std::string_view extension = {});
};

} // namespace Seraph
