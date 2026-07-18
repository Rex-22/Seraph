#include "Seraph/Console/ConsoleSink.h"

#include <deque>

namespace Seraph
{

namespace
{

constexpr std::size_t k_MaxLines = 8192; // ring cap; oldest lines drop off

struct Store
{
    std::mutex Mutex;
    std::deque<ConsoleLine> Lines;
    u64 Version = 0;
};

Store& Get()
{
    static Store s;
    return s;
}

} // namespace

void ConsoleOutput::Push(int level, std::string text)
{
    Store& s = Get();
    std::lock_guard<std::mutex> lock(s.Mutex);
    s.Lines.push_back({level, std::move(text)});
    if (s.Lines.size() > k_MaxLines)
        s.Lines.pop_front();
    ++s.Version;
}

void ConsoleOutput::Read(std::vector<ConsoleLine>& out)
{
    Store& s = Get();
    std::lock_guard<std::mutex> lock(s.Mutex);
    out.assign(s.Lines.begin(), s.Lines.end());
}

void ConsoleOutput::Clear()
{
    Store& s = Get();
    std::lock_guard<std::mutex> lock(s.Mutex);
    s.Lines.clear();
    ++s.Version;
}

u64 ConsoleOutput::Version()
{
    Store& s = Get();
    std::lock_guard<std::mutex> lock(s.Mutex);
    return s.Version;
}

} // namespace Seraph
