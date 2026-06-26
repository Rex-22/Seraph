//
// Created by Ruben on 2025/05/03.
//

#include "Log.h"

#include "Base.h"
#include "spdlog/sinks/rotating_file_sink.h"
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>

namespace Seraph
{
std::shared_ptr<spdlog::logger> Log::core_logger;
std::shared_ptr<spdlog::logger> Log::app_logger;

static std::shared_ptr<spdlog::logger> MakeLogger(
  std::string name, const std::vector<spdlog::sink_ptr>& sinks, spdlog::level::level_enum lvl)
{
  auto logger = std::make_shared<spdlog::logger>(std::move(name), begin(sinks), end(sinks));
  spdlog::register_logger(logger);
  logger->set_level(lvl);
  logger->flush_on(spdlog::level::err);
  return logger;
}

void Log::Init()
{
    std::vector<spdlog::sink_ptr> logSinks;
    logSinks.emplace_back(
        std::make_shared<spdlog::sinks::stdout_color_sink_mt>());
    logSinks.emplace_back(
        std::make_shared<spdlog::sinks::rotating_file_sink_mt>(
            "logs/Seraph.log", Megabytes(10), 10));

    logSinks[0]->set_pattern("%^[%T] %n: %v%$");
    logSinks[1]->set_pattern("[%T] [%l] %n: %v");
    spdlog::flush_every(std::chrono::seconds(2));

    core_logger = MakeLogger("SP", logSinks, spdlog::level::trace);
    app_logger = MakeLogger("APP", logSinks, spdlog::level::trace);
}

void Log::Shutdown()
{
    spdlog::shutdown();
}

} // namespace Core