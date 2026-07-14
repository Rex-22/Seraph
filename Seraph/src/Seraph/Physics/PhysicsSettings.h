//
// Global physics tuning + the collision-layer matrix. Jolt-free.
//

#pragma once

#include "Seraph/Core/Base.h"

#include <glm/glm.hpp>

#include <string>
#include <vector>

namespace Seraph
{

// One collision layer. Two bodies collide iff their layers are configured to.
// BitValue = (1u << LayerID); CollidesWith is a bitmask of other layers.
struct PhysicsLayer
{
    u32 LayerID = 0;
    std::string Name;
    u32 BitValue = 0;
    u32 CollidesWith = 0;
    bool CollidesWithSelf = true;
};

// Registry of collision layers + the pairwise collision matrix. Static because
// the layer set is process-global (the Jolt layer-filter interfaces read it).
// InitDefaults() is called from PhysicsSystem::Init().
class PhysicsLayerManager
{
public:
    // Reset to the canonical two-layer setup: 0 = Static, 1 = Moving.
    // Static-vs-Static does not collide; Moving collides with Static + Moving.
    static void InitDefaults();

    // Register a new layer; returns its LayerID. When collidesWithAll is true the
    // new layer collides with every existing layer (and they with it).
    static u32 AddLayer(const std::string& name, bool collidesWithAll = true);

    static void SetLayerCollision(u32 layerA, u32 layerB, bool shouldCollide);
    static bool ShouldCollide(u32 layerA, u32 layerB);

    static const PhysicsLayer& GetLayer(u32 layerID);
    static const std::vector<PhysicsLayer>& GetLayers();
    static u32 GetLayerCount();
    static bool IsLayerValid(u32 layerID);

    // Convenience accessors for the two defaults.
    static u32 StaticLayer() { return 0; }
    static u32 MovingLayer() { return 1; }

private:
    static std::vector<PhysicsLayer> s_Layers;
};

// Global simulation tuning, owned by PhysicsSystem and read when a scene starts.
struct PhysicsSettings
{
    f32 FixedTimestep = 1.0f / 60.0f;
    glm::vec3 Gravity = { 0.0f, -9.81f, 0.0f };
    u32 MaxBodies = 10240;
    u32 MaxBodyPairs = 65536;
    u32 MaxContactConstraints = 10240;
    u32 PositionSolverIterations = 2;
    u32 VelocitySolverIterations = 10;
};

} // namespace Seraph
