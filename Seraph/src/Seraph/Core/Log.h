//
// Created by Ruben on 2025/05/03.
//

#pragma once
#include <memory>
#include <string>
#include <spdlog/spdlog.h>

namespace Seraph
{
struct Log
{
    static std::shared_ptr<spdlog::logger> core_logger;
    static std::shared_ptr<spdlog::logger> app_logger;

    static void Init();
    static void Shutdown();
};
}

#define CORE_TRACE(...) Seraph::Log::core_logger->trace(__VA_ARGS__)
#define CORE_DEBUG(...) Seraph::Log::core_logger->debug(__VA_ARGS__)
#define CORE_INFO(...) Seraph::Log::core_logger->info(__VA_ARGS__)
#define CORE_WARN(...) Seraph::Log::core_logger->warn(__VA_ARGS__)
#define CORE_ERROR(...) Seraph::Log::core_logger->error(__VA_ARGS__)

#define APP_TRACE(...) Seraph::Log::app_logger->trace(__VA_ARGS__)
#define APP_DEBUG(...) Seraph::Log::app_logger->debug(__VA_ARGS__)
#define APP_INFO(...) Seraph::Log::app_logger->info(__VA_ARGS__)
#define APP_WARN(...) Seraph::Log::app_logger->warn(__VA_ARGS__)
#define APP_ERROR(...) Seraph::Log::app_logger->error(__VA_ARGS__)
