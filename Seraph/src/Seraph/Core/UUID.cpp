//
// Created by ruben on 2026/06/27.
//

#include "UUID.h"

#include <mutex>
#include <random>

#include <unordered_map>

namespace Seraph
{
static std::random_device s_RandomDevice;
static std::mt19937_64 s_Engine(s_RandomDevice());
static std::uniform_int_distribution<u64> s_UniformDistribution;
// The generator statics above are shared mutable state; the asset system mints
// handles from worker threads, so guard generation to avoid a data race.
static std::mutex s_UUIDMutex;

UUID::UUID()
{
    std::scoped_lock<std::mutex> lock(s_UUIDMutex);
    m_UUID = s_UniformDistribution(s_Engine);
}

UUID::UUID(u64 uuid)
    : m_UUID(uuid)
{
}

}