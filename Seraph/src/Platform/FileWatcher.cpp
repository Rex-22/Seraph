#include "FileWatcher.h"

#include "Seraph/Core/Log.h"

#include <mutex>

#ifdef __APPLE__
    #include <CoreServices/CoreServices.h>
    #include <dispatch/dispatch.h>
#endif

namespace Seraph
{

#ifdef __APPLE__

// ---------------------------------------------------------------------------
// macOS backend: FSEvents. The stream posts batches onto a serial dispatch
// queue; the callback appends to a mutex-guarded queue that the main thread
// drains via DrainEvents().
// ---------------------------------------------------------------------------
struct FileWatcher::Impl
{
    FSEventStreamRef Stream = nullptr;
    dispatch_queue_t Queue = nullptr;
    std::mutex Mutex;
    std::vector<FileWatchEvent> Pending;

    static FileWatchEventKind KindFromFlags(FSEventStreamEventFlags flags)
    {
        // A single event can carry several flags; pick the most significant.
        if (flags & kFSEventStreamEventFlagItemRemoved)
            return FileWatchEventKind::Removed;
        if (flags & kFSEventStreamEventFlagItemRenamed)
            return FileWatchEventKind::Renamed;
        if (flags & kFSEventStreamEventFlagItemCreated)
            return FileWatchEventKind::Created;
        return FileWatchEventKind::Modified;
    }

    static void Callback(
        ConstFSEventStreamRef /*streamRef*/, void* clientCallBackInfo,
        size_t numEvents, void* eventPaths,
        const FSEventStreamEventFlags eventFlags[],
        const FSEventStreamEventId /*eventIds*/[])
    {
        auto* self = static_cast<Impl*>(clientCallBackInfo);
        const auto** paths = static_cast<const char**>(eventPaths);

        std::lock_guard<std::mutex> lock(self->Mutex);
        for (size_t i = 0; i < numEvents; ++i) {
            self->Pending.push_back(
                {KindFromFlags(eventFlags[i]), std::filesystem::path(paths[i])});
        }
    }
};

FileWatcher::~FileWatcher()
{
    Stop();
}

void FileWatcher::Start(const std::filesystem::path& root, bool /*recursive*/)
{
    Stop(); // FSEvents watches subtrees; `recursive` is implied on macOS.

    m_Impl = new Impl();

    CFStringRef cfPath = CFStringCreateWithCString(
        nullptr, root.c_str(), kCFStringEncodingUTF8);
    if (cfPath == nullptr) {
        SP_CORE_ERROR_TAG("FileWatcher", "Failed to encode path '{}'", root.string());
        delete m_Impl;
        m_Impl = nullptr;
        return;
    }

    CFArrayRef pathsToWatch = CFArrayCreate(
        nullptr, reinterpret_cast<const void**>(&cfPath), 1, &kCFTypeArrayCallBacks);
    CFRelease(cfPath);

    FSEventStreamContext context = {};
    context.info = m_Impl;

    constexpr CFTimeInterval k_Latency = 0.2; // seconds; coalesces rapid saves
    m_Impl->Stream = FSEventStreamCreate(
        nullptr, &Impl::Callback, &context, pathsToWatch,
        kFSEventStreamEventIdSinceNow, k_Latency,
        kFSEventStreamCreateFlagFileEvents | kFSEventStreamCreateFlagNoDefer);
    CFRelease(pathsToWatch);

    if (m_Impl->Stream == nullptr) {
        SP_CORE_ERROR_TAG("FileWatcher", "FSEventStreamCreate failed for '{}'", root.string());
        delete m_Impl;
        m_Impl = nullptr;
        return;
    }

    m_Impl->Queue = dispatch_queue_create(
        "tech.bitcube.seraph.filewatcher", DISPATCH_QUEUE_SERIAL);
    FSEventStreamSetDispatchQueue(m_Impl->Stream, m_Impl->Queue);
    FSEventStreamStart(m_Impl->Stream);

    SP_CORE_INFO_TAG("FileWatcher", "Watching '{}'", root.string());
}

void FileWatcher::Stop()
{
    if (m_Impl == nullptr)
        return;

    if (m_Impl->Stream != nullptr) {
        FSEventStreamStop(m_Impl->Stream);
        FSEventStreamInvalidate(m_Impl->Stream);
        FSEventStreamRelease(m_Impl->Stream);
        m_Impl->Stream = nullptr;
    }
    if (m_Impl->Queue != nullptr) {
        dispatch_release(m_Impl->Queue);
        m_Impl->Queue = nullptr;
    }

    delete m_Impl;
    m_Impl = nullptr;
}

bool FileWatcher::IsWatching() const
{
    return m_Impl != nullptr && m_Impl->Stream != nullptr;
}

std::vector<FileWatchEvent> FileWatcher::DrainEvents()
{
    if (m_Impl == nullptr)
        return {};

    std::lock_guard<std::mutex> lock(m_Impl->Mutex);
    std::vector<FileWatchEvent> drained;
    drained.swap(m_Impl->Pending);
    return drained;
}

#else

// ---------------------------------------------------------------------------
// No backend on this platform yet — compile a no-op so callers work unchanged.
// ---------------------------------------------------------------------------
struct FileWatcher::Impl
{
};

FileWatcher::~FileWatcher() = default;

void FileWatcher::Start(const std::filesystem::path& root, bool /*recursive*/)
{
    SP_CORE_WARN_TAG(
        "FileWatcher", "No file-watch backend on this platform; '{}' not watched",
        root.string());
}

void FileWatcher::Stop() {}
bool FileWatcher::IsWatching() const { return false; }
std::vector<FileWatchEvent> FileWatcher::DrainEvents() { return {}; }

#endif

} // namespace Seraph
