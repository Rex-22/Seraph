//
// Minimal fixed-size thread pool. The asset system uses it for off-thread byte
// reads + CPU parsing; GPU finalize stays on the main thread, so a single
// worker is the sensible default (raise the count to parallelize IO/parse).
//

#pragma once

#include "Seraph/Core/Base.h"

#include <atomic>
#include <condition_variable>
#include <functional>
#include <mutex>
#include <queue>
#include <string>
#include <thread>
#include <vector>

namespace Seraph
{

class ThreadPool
{
public:
    using Job = std::function<void()>;

    explicit ThreadPool(u32 threadCount = 1, std::string name = "Worker");
    ~ThreadPool();

    ThreadPool(const ThreadPool&) = delete;
    ThreadPool& operator=(const ThreadPool&) = delete;

    void Enqueue(Job job);

    // Block until the queue is empty and no job is running.
    void Drain();

    [[nodiscard]] u32 PendingCount() const;
    [[nodiscard]] bool IsIdle() const;

private:
    void WorkerLoop();

    std::vector<std::thread> m_Threads;
    std::queue<Job> m_Jobs;

    mutable std::mutex m_Mutex;
    std::condition_variable m_JobAvailable;
    std::condition_variable m_JobFinished;

    std::atomic<bool> m_Running{true};
    u32 m_ActiveJobs = 0;
    std::string m_Name;
};

} // namespace Seraph
