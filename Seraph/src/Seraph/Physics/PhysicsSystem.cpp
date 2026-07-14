//
// Created for Jolt Physics integration (Physics 1).
//

#include "PhysicsSystem.h"

#include "PhysicsSettings.h"
#include "Seraph/Core/Log.h"
#include "Seraph/Physics/JoltPhysics/JoltScene.h"

#include <Jolt/Jolt.h>

#include <Jolt/Core/Factory.h>
#include <Jolt/Core/JobSystemThreadPool.h>
#include <Jolt/Core/TempAllocator.h>
#include <Jolt/RegisterTypes.h>

#include <cstdarg>
#include <cstdio>
#include <memory>
#include <thread>

namespace Seraph
{
namespace
{
    // Long-lived singletons handed to JPH::PhysicsSystem::Update every step.
    std::unique_ptr<JPH::TempAllocatorImpl> s_TempAllocator;
    std::unique_ptr<JPH::JobSystemThreadPool> s_JobSystem;

    PhysicsSettings s_Settings;

    // Recommended Jolt limits (see Jolt HelloWorld). Job/barrier pool sizes.
    constexpr uint32_t k_MaxPhysicsJobs = 2048;
    constexpr uint32_t k_MaxPhysicsBarriers = 8;
    constexpr uint32_t k_TempAllocatorBytes = 32u * 1024 * 1024; // 32 MB

    // Routes Jolt's printf-style trace through the engine's "Physics" log tag.
    void TraceImpl(const char* fmt, ...)
    {
        va_list args;
        va_start(args, fmt);
        char buffer[1024];
        std::vsnprintf(buffer, sizeof(buffer), fmt, args);
        va_end(args);
        SP_CORE_TRACE_TAG("Physics", "{}", buffer);
    }

#ifdef JPH_ENABLE_ASSERTS
    bool AssertFailedImpl(
        const char* expression, const char* message, const char* file, JPH::uint line)
    {
        SP_CORE_ERROR_TAG(
            "Physics", "Assert failed {}:{}: ({}) {}", file, line, expression,
            message != nullptr ? message : "");
        return true; // break into the debugger
    }
#endif
} // namespace

void PhysicsSystem::Init()
{
    // Collision-layer matrix is process-global; set up the defaults once.
    PhysicsLayerManager::InitDefaults();

    // Install Jolt's default malloc/free before anything allocates.
    JPH::RegisterDefaultAllocator();

    JPH::Trace = TraceImpl;
    JPH_IF_ENABLE_ASSERTS(JPH::AssertFailed = AssertFailedImpl;)

    JPH::Factory::sInstance = new JPH::Factory();
    JPH::RegisterTypes();

    s_TempAllocator = std::make_unique<JPH::TempAllocatorImpl>(k_TempAllocatorBytes);

    const unsigned int hw = std::thread::hardware_concurrency();
    const int workerThreads = static_cast<int>(hw > 1 ? hw - 1 : 1);
    s_JobSystem = std::make_unique<JPH::JobSystemThreadPool>(
        k_MaxPhysicsJobs, k_MaxPhysicsBarriers, workerThreads);

    SP_CORE_INFO_TAG(
        "Physics", "Jolt Physics initialized ({} worker threads)", workerThreads);
}

void PhysicsSystem::Shutdown()
{
    s_JobSystem.reset();
    s_TempAllocator.reset();

    JPH::UnregisterTypes();
    delete JPH::Factory::sInstance;
    JPH::Factory::sInstance = nullptr;

    SP_CORE_INFO_TAG("Physics", "Jolt Physics shut down");
}

PhysicsSettings& PhysicsSystem::GetSettings()
{
    return s_Settings;
}

Ref<PhysicsScene> PhysicsSystem::CreateScene(Scene* scene)
{
    return Ref<JoltScene>::Create(scene);
}

JPH::JobSystem* PhysicsSystem::GetJobSystem()
{
    return s_JobSystem.get();
}

JPH::TempAllocator* PhysicsSystem::GetTempAllocator()
{
    return s_TempAllocator.get();
}

} // namespace Seraph
