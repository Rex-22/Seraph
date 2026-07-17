//
// Loads a project's compiled native-script module (libGame.<dylib/so/dll>) at
// runtime via SDL's shared-object loader, so the editor stays portable — no
// recompile to switch projects. The module's reflection registrations (generated
// by SeraphHeaderTool from each script's SCLASS/SP_REFLECT) run on load, tagged
// k_GameModule, so its scripts appear in the shared reflection registry (which
// lives in libSeraph — module and host share one) and are dropped on unload. One
// module is loaded at a time. See ScriptTypes for the reflection-backed lookup.
//

#pragma once

#include "Seraph/Core/Base.h"

#include <filesystem>
#include <string>

struct SDL_SharedObject; // SDL3 opaque handle; full type only in the .cpp

namespace Seraph
{

class ScriptLibrary
{
public:
    // Load the module at `gameLib` (any previously-loaded module is unloaded
    // first). Returns false if the file is missing or the load fails.
    static bool Load(const std::filesystem::path& gameLib);

    // Unload the current module (if any) and drop its registrations.
    static void Unload();

    // Unload → reload. Use after a rebuild. The caller MUST ensure no script
    // instances are live (i.e. not playing): a live instance's vtable lives in
    // the module being unloaded.
    static bool Reload(const std::filesystem::path& gameLib);

    static bool IsLoaded();

    // Platform module filename: "libGame.dylib" / "libGame.so" / "Game.dll".
    static std::string LibraryFileName();

private:
    static SDL_SharedObject* s_Handle;
    static std::filesystem::path s_LoadedCopy; // the unique temp actually loaded
    static u32 s_Counter;
};

} // namespace Seraph
