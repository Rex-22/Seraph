//
// ConsoleSink — an spdlog sink that captures formatted log records into a
// thread-safe ring buffer the in-editor console panel renders. Log records arrive
// on any thread; the panel reads on the main thread each frame, so the buffer is
// mutex-guarded and exposes a Version() the panel polls to avoid re-copying every
// frame.
//
// Wired into the loggers in Core/Log.cpp (SP_HAS_CONSOLE builds only), so both the
// dedicated console logger (SP_CONSOLE_LOG_*) and the tagged engine/app loggers
// (SP_CORE_INFO_TAG / SP_INFO ...) surface in the panel.
//

#pragma once

#include "Seraph/Core/Base.h"

#include <spdlog/sinks/base_sink.h>

#include <mutex>
#include <string>
#include <vector>

namespace Seraph
{

// One captured line: the spdlog level (spdlog::level::level_enum as int, so this
// header needn't drag the enum into consumers) and the message text.
struct ConsoleLine
{
    int Level = 0;
    std::string Text;
};

// The shared ring buffer behind the sink. Static facade — the sink writes, the
// panel reads, both under one lock.
class ConsoleOutput
{
public:
    ConsoleOutput() = delete;

    static void Push(int level, std::string text);
    // Replace `out` with a snapshot of the current buffer (oldest first).
    static void Read(std::vector<ConsoleLine>& out);
    static void Clear();
    // Bumped on every Push/Clear; the panel re-reads only when this changes.
    [[nodiscard]] static u64 Version();
};

// The spdlog sink. base_sink<std::mutex> serializes sink_it_ calls; we forward the
// raw payload + level to ConsoleOutput (ignoring the text formatter — the panel
// styles by level itself).
class ConsoleSink : public spdlog::sinks::base_sink<std::mutex>
{
protected:
    void sink_it_(const spdlog::details::log_msg& msg) override
    {
        ConsoleOutput::Push(static_cast<int>(msg.level),
                            std::string(msg.payload.data(), msg.payload.size()));
    }
    void flush_() override {}
};

} // namespace Seraph
