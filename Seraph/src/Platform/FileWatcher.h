//
// Watches a directory tree for external file changes and buffers them on a
// thread-safe queue. The OS backend (FSEvents on macOS) delivers events on its
// own thread; the editor drains them once per frame on the main thread via
// DrainEvents(), so any follow-up work (asset reload, bgfx uploads) stays on
// the main thread.
//
// Backends: FSEvents (macOS), inotify (Linux), ReadDirectoryChangesW (Windows).
// Any other platform compiles a no-op stub (IsWatching() stays false and
// DrainEvents() returns empty).
//

#pragma once

#include <filesystem>
#include <vector>

namespace Seraph
{

enum class FileWatchEventKind
{
    Created,
    Modified,
    Removed,
    Renamed,
};

struct FileWatchEvent
{
    FileWatchEventKind Kind = FileWatchEventKind::Modified;
    std::filesystem::path Path; // absolute path as reported by the OS
};

class FileWatcher
{
public:
    FileWatcher() = default;
    ~FileWatcher();

    FileWatcher(const FileWatcher&) = delete;
    FileWatcher& operator=(const FileWatcher&) = delete;

    // Begin watching `root` (recursively when requested). Replaces any existing
    // watch. A no-op on platforms without a backend.
    void Start(const std::filesystem::path& root, bool recursive = true);
    void Stop();
    [[nodiscard]] bool IsWatching() const;

    // Main thread: move all buffered events out of the internal queue. Callers
    // should coalesce/dedupe as needed (a single save can produce several
    // events for one path).
    std::vector<FileWatchEvent> DrainEvents();

private:
    struct Impl;
    Impl* m_Impl = nullptr; // platform backend; null when nothing is watched
};

} // namespace Seraph
