//
// See ScriptLibrary.h.
//

#include "ScriptLibrary.h"

#include "ScriptRegistry.h"
#include "Seraph/Core/Log.h"
#include "Seraph/Reflection/Reflection.h"
#include "Seraph/Settings/Settings.h"

#include <SDL3/SDL_loadso.h>

#include <system_error>

namespace Seraph
{

SDL_SharedObject* ScriptLibrary::s_Handle = nullptr;
std::filesystem::path ScriptLibrary::s_LoadedCopy;
u32 ScriptLibrary::s_Counter = 0;

std::string ScriptLibrary::LibraryFileName()
{
#if defined(_WIN32)
    return "Game.dll";
#elif defined(__APPLE__)
    return "libGame.dylib";
#else
    return "libGame.so";
#endif
}

bool ScriptLibrary::Load(const std::filesystem::path& gameLib)
{
    if (s_Handle)
        Unload();

    std::error_code ec;
    if (!std::filesystem::exists(gameLib, ec)) {
        SP_CORE_WARN_TAG("Scripting", "No script module at '{}'", gameLib.string());
        return false;
    }

    // Loaders cache a library by path, so a rebuilt file at the same path may
    // not re-run its static initializers. Load a unique per-load copy so a
    // reload always registers fresh.
    const std::filesystem::path dir = gameLib.parent_path();
    const std::string stem = gameLib.stem().string();
    const std::string ext = gameLib.extension().string();
    const std::filesystem::path copy =
        dir / ("." + stem + "_" + std::to_string(s_Counter++) + ext);

    std::filesystem::copy_file(
        gameLib, copy, std::filesystem::copy_options::overwrite_existing, ec);
    if (ec) {
        SP_CORE_ERROR_TAG(
            "Scripting", "Failed to stage script module: {}", ec.message());
        return false;
    }

    // Tag reflection registrations that run during load as the Game module, so
    // ClearModule can drop them (and their Property thunks) before the dylib
    // unmaps. The scope restores the previous (engine) module on exit.
    SDL_SharedObject* handle = nullptr;
    {
        ReflectionModuleScope reflScope(k_GameModule);
        handle = SDL_LoadObject(copy.string().c_str());
    }
    if (!handle) {
        SP_CORE_ERROR_TAG("Scripting", "SDL_LoadObject failed: {}", SDL_GetError());
        std::filesystem::remove(copy, ec);
        return false;
    }

    s_Handle = handle;
    s_LoadedCopy = copy;
    SP_CORE_INFO_TAG(
        "Scripting",
        "Loaded script module '{}' ({} scripts, {} reflected types registered)",
        gameLib.filename().string(), ScriptRegistry::GetAll().size(),
        Reflection::All().size());
    return true;
}

void ScriptLibrary::Unload()
{
    if (!s_Handle)
        return;

    // Drop factories, reflected types, and game.* settings (their lambdas /
    // Get-Set thunks / registration all live in the module) before unmapping it —
    // otherwise those function pointers dangle.
    ScriptRegistry::Clear();
    Reflection::ClearModule(k_GameModule);
    Settings::PurgeByPrefix("game.");
    SDL_UnloadObject(s_Handle);
    s_Handle = nullptr;

    std::error_code ec;
    if (!s_LoadedCopy.empty())
        std::filesystem::remove(s_LoadedCopy, ec);
    s_LoadedCopy.clear();
}

bool ScriptLibrary::Reload(const std::filesystem::path& gameLib)
{
    Unload();
    return Load(gameLib);
}

bool ScriptLibrary::IsLoaded()
{
    return s_Handle != nullptr;
}

} // namespace Seraph
