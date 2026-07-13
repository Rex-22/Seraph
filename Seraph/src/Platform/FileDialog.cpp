#include "FileDialog.h"

#include "Platform/Window.h"
#include "Seraph/Core/Application.h"

#include <SDL3/SDL_dialog.h>

#include <mutex>
#include <string>

namespace Seraph
{
namespace
{

std::mutex s_Mutex;
bool s_Open = false;      // a dialog is currently showing
bool s_HasResult = false; // a result is waiting to be polled
int s_Id = 0;
std::filesystem::path s_Path; // empty == cancelled / error

// SDL requires the filter (and its strings) to stay valid until the callback
// fires. Only one dialog runs at a time, so static storage is safe.
std::string s_FilterName;
std::string s_FilterPattern;
SDL_DialogFileFilter s_Filter;

void SDLCALL OnPicked(void* /*userdata*/, const char* const* filelist, int /*filter*/)
{
    std::scoped_lock lock(s_Mutex);
    s_Open = false;
    s_HasResult = true;
    s_Path.clear();
    if (filelist != nullptr && filelist[0] != nullptr)
        s_Path = filelist[0];
}

SDL_Window* NativeWindow()
{
    return Application::Instance().Window().Handle();
}

} // namespace

void FileDialog::OpenFile(int id, const char* filterName, const char* filterPattern)
{
    {
        std::scoped_lock lock(s_Mutex);
        if (s_Open)
            return;
        s_Open = true;
        s_HasResult = false;
        s_Id = id;
        s_FilterName = filterName != nullptr ? filterName : "";
        s_FilterPattern = filterPattern != nullptr ? filterPattern : "*";
        s_Filter.name = s_FilterName.c_str();
        s_Filter.pattern = s_FilterPattern.c_str();
    }
    // Call SDL outside the lock: the callback could (per its contract) run on
    // another thread and take the same lock.
    SDL_ShowOpenFileDialog(
        OnPicked, nullptr, NativeWindow(), &s_Filter, 1, nullptr, false);
}

void FileDialog::OpenFolder(int id)
{
    {
        std::scoped_lock lock(s_Mutex);
        if (s_Open)
            return;
        s_Open = true;
        s_HasResult = false;
        s_Id = id;
    }
    SDL_ShowOpenFolderDialog(OnPicked, nullptr, NativeWindow(), nullptr, false);
}

std::optional<FileDialog::Result> FileDialog::Poll()
{
    std::scoped_lock lock(s_Mutex);
    if (!s_HasResult)
        return std::nullopt;

    s_HasResult = false;
    if (s_Path.empty())
        return std::nullopt; // cancelled / error

    return Result{ s_Id, s_Path };
}

} // namespace Seraph
