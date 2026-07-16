#include "FileWatcher.h"

#include "Seraph/Core/Log.h"

#include <mutex>
#include <system_error>

#ifdef __APPLE__
    #include <CoreServices/CoreServices.h>
    #include <dispatch/dispatch.h>
#elif defined(__linux__)
    #include <atomic>
    #include <cerrno>
    #include <cstring>
    #include <thread>
    #include <unordered_map>

    #include <poll.h>
    #include <sys/eventfd.h>
    #include <sys/inotify.h>
    #include <unistd.h>
#elif defined(_WIN32)
    #include <atomic>
    #include <string>
    #include <thread>

    #include <windows.h>
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

    // FSEvents always watches the whole subtree, so a non-recursive watch is
    // emulated by dropping events whose parent directory is not the watched
    // root. Root is canonicalised to match the resolved paths FSEvents reports
    // (e.g. /tmp -> /private/tmp).
    bool Recursive = true;
    std::filesystem::path Root;

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
            std::filesystem::path path(paths[i]);
            if (!self->Recursive && path.parent_path() != self->Root)
                continue; // deeper than the immediate root; skip in shallow mode
            self->Pending.push_back({KindFromFlags(eventFlags[i]), std::move(path)});
        }
    }
};

FileWatcher::~FileWatcher()
{
    Stop();
}

void FileWatcher::Start(const std::filesystem::path& root, bool recursive)
{
    Stop(); // FSEvents always watches the subtree; shallow mode filters below.

    m_Impl = new Impl();
    m_Impl->Recursive = recursive;
    // Canonicalise so parent_path() comparisons in the callback match the
    // resolved paths FSEvents reports. Fall back to the raw root if the path
    // cannot be resolved (e.g. it does not exist yet).
    std::error_code ec;
    m_Impl->Root = std::filesystem::canonical(root, ec);
    if (ec)
        m_Impl->Root = root;

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

#elif defined(__linux__)

// ---------------------------------------------------------------------------
// Linux backend: inotify. inotify does not watch subtrees, so a recursive watch
// adds one descriptor per directory (and adds more as directories are created).
// A background thread blocks in poll() on the inotify fd plus an eventfd used to
// interrupt it on shutdown, appending events to the mutex-guarded queue that the
// main thread drains via DrainEvents().
// ---------------------------------------------------------------------------
struct FileWatcher::Impl
{
    int Fd = -1;      // inotify instance
    int WakeFd = -1;  // eventfd, written to unblock poll() on Stop()
    bool Recursive = true;
    std::atomic<bool> Running{false};
    std::thread Thread;

    std::mutex Mutex;
    std::vector<FileWatchEvent> Pending;

    // wd -> watched directory. Touched from Start (before the thread runs) and
    // thereafter only from the watch thread, so it needs no separate lock.
    std::unordered_map<int, std::filesystem::path> WatchPaths;

    static constexpr uint32_t k_Mask = IN_CREATE | IN_CLOSE_WRITE | IN_DELETE
        | IN_DELETE_SELF | IN_MOVED_FROM | IN_MOVED_TO | IN_MOVE_SELF;

    static FileWatchEventKind KindFromMask(uint32_t mask)
    {
        if (mask & (IN_MOVED_FROM | IN_MOVED_TO | IN_MOVE_SELF))
            return FileWatchEventKind::Renamed;
        if (mask & (IN_DELETE | IN_DELETE_SELF))
            return FileWatchEventKind::Removed;
        if (mask & IN_CREATE)
            return FileWatchEventKind::Created;
        return FileWatchEventKind::Modified;
    }

    void AddSingle(const std::filesystem::path& dir)
    {
        const int wd = inotify_add_watch(Fd, dir.c_str(), k_Mask);
        if (wd < 0) {
            SP_CORE_WARN_TAG("FileWatcher", "inotify_add_watch failed for '{}': {}",
                dir.string(), std::strerror(errno));
            return;
        }
        WatchPaths[wd] = dir;
    }

    void AddTree(const std::filesystem::path& dir)
    {
        AddSingle(dir);
        if (!Recursive)
            return;

        std::error_code ec;
        auto it = std::filesystem::recursive_directory_iterator(
            dir, std::filesystem::directory_options::skip_permission_denied, ec);
        const std::filesystem::recursive_directory_iterator end;
        for (; !ec && it != end; it.increment(ec)) {
            if (it->is_directory(ec))
                AddSingle(it->path());
        }
    }

    void HandleEvent(const inotify_event& ev, std::vector<FileWatchEvent>& out)
    {
        if (ev.mask & IN_IGNORED) { // watch auto-removed (dir gone)
            WatchPaths.erase(ev.wd);
            return;
        }

        const auto it = WatchPaths.find(ev.wd);
        if (it == WatchPaths.end())
            return;

        std::filesystem::path full = it->second;
        if (ev.len > 0)
            full /= ev.name;

        // A newly-created subdirectory needs its own watch (and may already hold
        // children) for the recursive contract to hold.
        if (Recursive && (ev.mask & IN_ISDIR) && (ev.mask & (IN_CREATE | IN_MOVED_TO)))
            AddTree(full);

        out.push_back({KindFromMask(ev.mask), std::move(full)});
    }

    void Run()
    {
        std::vector<char> buffer(64 * 1024);
        while (Running.load(std::memory_order_relaxed)) {
            pollfd fds[2];
            fds[0] = {Fd, POLLIN, 0};
            fds[1] = {WakeFd, POLLIN, 0};

            const int ret = ::poll(fds, 2, -1);
            if (ret < 0) {
                if (errno == EINTR)
                    continue;
                break;
            }
            if (fds[1].revents & POLLIN) // wake => shutdown requested
                break;
            if (!(fds[0].revents & POLLIN))
                continue;

            const ssize_t len = ::read(Fd, buffer.data(), buffer.size());
            if (len <= 0)
                continue;

            std::vector<FileWatchEvent> batch;
            for (char* p = buffer.data(); p < buffer.data() + len;) {
                auto* ev = reinterpret_cast<inotify_event*>(p);
                HandleEvent(*ev, batch);
                p += sizeof(inotify_event) + ev->len;
            }
            if (!batch.empty()) {
                std::lock_guard<std::mutex> lock(Mutex);
                Pending.insert(Pending.end(), batch.begin(), batch.end());
            }
        }
    }
};

FileWatcher::~FileWatcher()
{
    Stop();
}

void FileWatcher::Start(const std::filesystem::path& root, bool recursive)
{
    Stop();

    m_Impl = new Impl();
    m_Impl->Recursive = recursive;

    m_Impl->Fd = inotify_init1(IN_NONBLOCK | IN_CLOEXEC);
    if (m_Impl->Fd < 0) {
        SP_CORE_ERROR_TAG("FileWatcher", "inotify_init1 failed: {}", std::strerror(errno));
        delete m_Impl;
        m_Impl = nullptr;
        return;
    }

    m_Impl->WakeFd = eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);
    if (m_Impl->WakeFd < 0) {
        SP_CORE_ERROR_TAG("FileWatcher", "eventfd failed: {}", std::strerror(errno));
        ::close(m_Impl->Fd);
        delete m_Impl;
        m_Impl = nullptr;
        return;
    }

    m_Impl->AddTree(root);
    if (m_Impl->WatchPaths.empty()) {
        SP_CORE_ERROR_TAG("FileWatcher", "No watches established for '{}'", root.string());
        ::close(m_Impl->WakeFd);
        ::close(m_Impl->Fd);
        delete m_Impl;
        m_Impl = nullptr;
        return;
    }

    m_Impl->Running.store(true);
    m_Impl->Thread = std::thread([impl = m_Impl] { impl->Run(); });

    SP_CORE_INFO_TAG("FileWatcher", "Watching '{}'", root.string());
}

void FileWatcher::Stop()
{
    if (m_Impl == nullptr)
        return;

    if (m_Impl->Running.exchange(false)) {
        const uint64_t one = 1;
        if (::write(m_Impl->WakeFd, &one, sizeof(one)) < 0) {
            // best-effort wake; the thread also re-checks Running each loop
        }
        if (m_Impl->Thread.joinable())
            m_Impl->Thread.join();
    }

    if (m_Impl->Fd >= 0)
        ::close(m_Impl->Fd);
    if (m_Impl->WakeFd >= 0)
        ::close(m_Impl->WakeFd);

    delete m_Impl;
    m_Impl = nullptr;
}

bool FileWatcher::IsWatching() const
{
    return m_Impl != nullptr && m_Impl->Running.load();
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

#elif defined(_WIN32)

// ---------------------------------------------------------------------------
// Windows backend: ReadDirectoryChangesW with overlapped I/O. A background
// thread issues the async read, then waits on the completion event plus a stop
// event; on completion it parses the FILE_NOTIFY_INFORMATION records into the
// mutex-guarded queue drained on the main thread. bWatchSubtree honours the
// `recursive` flag natively.
// ---------------------------------------------------------------------------
struct FileWatcher::Impl
{
    HANDLE Dir = INVALID_HANDLE_VALUE;
    HANDLE StopEvent = nullptr; // manual-reset; set on Stop()
    HANDLE IoEvent = nullptr;   // auto-reset; overlapped completion
    OVERLAPPED Overlapped{};
    bool Recursive = true;
    std::atomic<bool> Running{false};
    std::thread Thread;
    std::filesystem::path Root;

    std::mutex Mutex;
    std::vector<FileWatchEvent> Pending;

    alignas(DWORD) unsigned char Buffer[64 * 1024];

    static FileWatchEventKind KindFromAction(DWORD action)
    {
        switch (action) {
        case FILE_ACTION_ADDED:
            return FileWatchEventKind::Created;
        case FILE_ACTION_REMOVED:
            return FileWatchEventKind::Removed;
        case FILE_ACTION_RENAMED_OLD_NAME:
        case FILE_ACTION_RENAMED_NEW_NAME:
            return FileWatchEventKind::Renamed;
        default:
            return FileWatchEventKind::Modified;
        }
    }

    void Run()
    {
        constexpr DWORD k_Filter = FILE_NOTIFY_CHANGE_FILE_NAME
            | FILE_NOTIFY_CHANGE_DIR_NAME | FILE_NOTIFY_CHANGE_LAST_WRITE
            | FILE_NOTIFY_CHANGE_SIZE;

        while (Running.load(std::memory_order_relaxed)) {
            Overlapped = {};
            Overlapped.hEvent = IoEvent;

            if (!ReadDirectoryChangesW(Dir, Buffer, sizeof(Buffer),
                    Recursive ? TRUE : FALSE, k_Filter, nullptr, &Overlapped,
                    nullptr))
                break;

            HANDLE handles[2] = {IoEvent, StopEvent};
            const DWORD wait = WaitForMultipleObjects(2, handles, FALSE, INFINITE);
            if (wait != WAIT_OBJECT_0) // stop signalled or wait failed
                break;

            DWORD bytes = 0;
            if (!GetOverlappedResult(Dir, &Overlapped, &bytes, TRUE) || bytes == 0)
                continue;

            std::vector<FileWatchEvent> batch;
            for (DWORD offset = 0; offset < bytes;) {
                auto* info =
                    reinterpret_cast<FILE_NOTIFY_INFORMATION*>(Buffer + offset);
                const std::wstring name(
                    info->FileName, info->FileNameLength / sizeof(WCHAR));
                batch.push_back({KindFromAction(info->Action), Root / name});
                if (info->NextEntryOffset == 0)
                    break;
                offset += info->NextEntryOffset;
            }
            if (!batch.empty()) {
                std::lock_guard<std::mutex> lock(Mutex);
                Pending.insert(Pending.end(), batch.begin(), batch.end());
            }
        }
    }
};

FileWatcher::~FileWatcher()
{
    Stop();
}

void FileWatcher::Start(const std::filesystem::path& root, bool recursive)
{
    Stop();

    m_Impl = new Impl();
    m_Impl->Recursive = recursive;
    m_Impl->Root = root;

    m_Impl->Dir = CreateFileW(root.wstring().c_str(), FILE_LIST_DIRECTORY,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr,
        OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OVERLAPPED, nullptr);
    if (m_Impl->Dir == INVALID_HANDLE_VALUE) {
        SP_CORE_ERROR_TAG("FileWatcher", "CreateFileW failed for '{}'", root.string());
        delete m_Impl;
        m_Impl = nullptr;
        return;
    }

    m_Impl->StopEvent = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    m_Impl->IoEvent = CreateEventW(nullptr, FALSE, FALSE, nullptr);
    if (m_Impl->StopEvent == nullptr || m_Impl->IoEvent == nullptr) {
        SP_CORE_ERROR_TAG("FileWatcher", "CreateEventW failed for '{}'", root.string());
        if (m_Impl->StopEvent)
            CloseHandle(m_Impl->StopEvent);
        if (m_Impl->IoEvent)
            CloseHandle(m_Impl->IoEvent);
        CloseHandle(m_Impl->Dir);
        delete m_Impl;
        m_Impl = nullptr;
        return;
    }

    m_Impl->Running.store(true);
    m_Impl->Thread = std::thread([impl = m_Impl] { impl->Run(); });

    SP_CORE_INFO_TAG("FileWatcher", "Watching '{}'", root.string());
}

void FileWatcher::Stop()
{
    if (m_Impl == nullptr)
        return;

    if (m_Impl->Running.exchange(false)) {
        if (m_Impl->StopEvent)
            SetEvent(m_Impl->StopEvent);
        // Cancel any in-flight read so the kernel stops writing into Buffer
        // before we join and free the Impl.
        CancelIoEx(m_Impl->Dir, &m_Impl->Overlapped);
        if (m_Impl->Thread.joinable())
            m_Impl->Thread.join();
    }

    if (m_Impl->Dir != INVALID_HANDLE_VALUE)
        CloseHandle(m_Impl->Dir);
    if (m_Impl->StopEvent)
        CloseHandle(m_Impl->StopEvent);
    if (m_Impl->IoEvent)
        CloseHandle(m_Impl->IoEvent);

    delete m_Impl;
    m_Impl = nullptr;
}

bool FileWatcher::IsWatching() const
{
    return m_Impl != nullptr && m_Impl->Running.load();
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
