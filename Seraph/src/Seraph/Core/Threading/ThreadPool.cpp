#include "ThreadPool.h"

#include <utility>

namespace Seraph
{

ThreadPool::ThreadPool(u32 threadCount, std::string name)
    : m_Name(std::move(name))
{
    if (threadCount == 0)
        threadCount = 1;
    m_Threads.reserve(threadCount);
    for (u32 i = 0; i < threadCount; ++i)
        m_Threads.emplace_back([this] { WorkerLoop(); });
}

ThreadPool::~ThreadPool()
{
    {
        std::scoped_lock lock(m_Mutex);
        m_Running = false;
    }
    m_JobAvailable.notify_all();
    for (std::thread& thread : m_Threads)
        if (thread.joinable())
            thread.join();
}

void ThreadPool::Enqueue(Job job)
{
    {
        std::scoped_lock lock(m_Mutex);
        m_Jobs.push(std::move(job));
    }
    m_JobAvailable.notify_one();
}

void ThreadPool::Drain()
{
    std::unique_lock lock(m_Mutex);
    m_JobFinished.wait(lock, [this] {
        return m_Jobs.empty() && m_ActiveJobs == 0;
    });
}

u32 ThreadPool::PendingCount() const
{
    std::scoped_lock lock(m_Mutex);
    return static_cast<u32>(m_Jobs.size()) + m_ActiveJobs;
}

bool ThreadPool::IsIdle() const
{
    std::scoped_lock lock(m_Mutex);
    return m_Jobs.empty() && m_ActiveJobs == 0;
}

void ThreadPool::WorkerLoop()
{
    while (true) {
        Job job;
        {
            std::unique_lock lock(m_Mutex);
            m_JobAvailable.wait(lock, [this] {
                return !m_Running || !m_Jobs.empty();
            });
            if (!m_Running && m_Jobs.empty())
                return;
            job = std::move(m_Jobs.front());
            m_Jobs.pop();
            ++m_ActiveJobs;
        }

        job();

        {
            std::scoped_lock lock(m_Mutex);
            --m_ActiveJobs;
        }
        m_JobFinished.notify_all();
    }
}

} // namespace Seraph
