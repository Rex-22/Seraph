//
// Thin wrapper over SDL's native file/folder dialogs. The dialogs are
// asynchronous — start one with OpenFile/OpenFolder, then call Poll() once per
// frame on the main thread to pick up the chosen path (SDL may invoke its
// callback on another thread, so the result is delivered through a poll).
//
// Only one dialog is shown at a time; requests while one is open are ignored.
//

#pragma once

#include <filesystem>
#include <optional>

namespace Seraph
{

class FileDialog
{
public:
    // Show a file-open dialog filtered by a single extension (pattern is the
    // extension without a dot, e.g. "sproj"). `id` is echoed back in the result.
    static void OpenFile(int id, const char* filterName, const char* filterPattern);

    // Show a folder-select dialog.
    static void OpenFolder(int id);

    struct Result
    {
        int id = 0;
        std::filesystem::path path;
    };

    // Main thread: returns the chosen path once (then clears). nullopt while no
    // dialog result is ready, or when the user cancelled.
    static std::optional<Result> Poll();
};

} // namespace Seraph
