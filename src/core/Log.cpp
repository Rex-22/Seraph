//
// Created by Ruben on 2025/05/03.
//

#include "Log.h"
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks-inl.h>
#include <spdlog/spdlog.h>

namespace Core
{
std::shared_ptr<spdlog::logger> Log::core_logger;

void Log::Init()
{
    std::vector<spdlog::sink_ptr> logSinks;
    logSinks.emplace_back(
        std::make_shared<spdlog::sinks::stdout_color_sink_mt>());
    logSinks.emplace_back(
        std::make_shared<spdlog::sinks::basic_file_sink_mt>(
            "Seraph.log", true));

    logSinks[0]->set_pattern("%^[%T] %n: %v%$");
    logSinks[1]->set_pattern("[%T] [%l] %n: %v");

    core_logger =
        std::make_shared<spdlog::logger>("SP", begin(logSinks), end(logSinks));
    register_logger(Log::core_logger);
    core_logger->set_level(spdlog::level::trace);
    core_logger->flush_on(spdlog::level::trace);
}

} // namespace Core