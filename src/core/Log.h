//
// Created by Ruben on 2025/05/03.
//

#pragma once
#include <memory>
#include <string>
#include <spdlog/spdlog.h>

namespace Core
{
struct Log
{
    static std::shared_ptr<spdlog::logger> core_logger;

    static void Init();
};
}

#define CORE_DEBUG(...) Core::Log::core_logger->debug(__VA_ARGS__);
#define CORE_INFO(...) Core::Log::core_logger->info(__VA_ARGS__);
#define CORE_WARN(...) Core::Log::core_logger->warn(__VA_ARGS__);
#define CORE_ERROR(...) Core::Log::core_logger->error(__VA_ARGS__);
