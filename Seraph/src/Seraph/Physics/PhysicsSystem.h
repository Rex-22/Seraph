//
// Global Jolt Physics lifecycle. Owns the process-wide Jolt state (allocator,
// factory, registered types) plus the long-lived job system and temp allocator
// that every simulation step uses. Init()/Shutdown() are driven from main()
// (EntryPoint.h). This header is Jolt-free so it can be included widely.
//

#pragma once

#include "PhysicsSettings.h"
#include "Seraph/Core/Ref.h"

// Forward-declare the Jolt types we hand back to the backend, to keep <Jolt/...>
// out of this header. Only the physics backend (.cpp) ever dereferences them.
namespace JPH
{
class JobSystem;
class TempAllocator;
}

namespace Seraph
{

class Scene;
class PhysicsScene;

class PhysicsSystem
{
public:
    static void Init();
    static void Shutdown();

    // Global simulation tuning, read when a scene starts.
    static PhysicsSettings& GetSettings();

    // Create a physics world bound to an entity scene (returns a JoltScene).
    // Takes a raw Scene* — the world is owned by and scoped within that scene.
    static Ref<PhysicsScene> CreateScene(Scene* scene);

    // Used by the Jolt backend's PhysicsSystem::Update — long-lived singletons.
    static JPH::JobSystem* GetJobSystem();
    static JPH::TempAllocator* GetTempAllocator();
};

} // namespace Seraph
