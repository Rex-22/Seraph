//
// AutoCVar deferred-registration queue. AutoCVars constructed during static init
// can't register into Settings yet (the reflection registry that resolves their
// value type, and Settings::Init, run later), so they queue here and Console::Init
// drains the queue via FlushPendingCVarRegistrations(). Anything constructed after
// the flush registers immediately.
//

#include "Seraph/Console/AutoCVar.h"

namespace Seraph
{

namespace
{

struct PendingQueue
{
    std::vector<std::function<void()>> Fns;
    bool Flushed = false;
};

PendingQueue& Queue()
{
    static PendingQueue s;
    return s;
}

} // namespace

namespace Detail
{

void EnqueueOrRunCVarRegistration(std::function<void()> fn)
{
    PendingQueue& q = Queue();
    if (q.Flushed)
        fn(); // registry is live already (late/hot-reloaded CVar) — run now
    else
        q.Fns.push_back(std::move(fn));
}

} // namespace Detail

void FlushPendingCVarRegistrations()
{
    PendingQueue& q = Queue();
    // Run in registration order. New entries could be appended re-entrantly
    // (unlikely), so drain by index rather than caching size.
    for (std::size_t i = 0; i < q.Fns.size(); ++i)
        q.Fns[i]();
    q.Fns.clear();
    q.Flushed = true;
}

} // namespace Seraph
