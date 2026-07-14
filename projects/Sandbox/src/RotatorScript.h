//
// Sandbox demo script — proves the native scripting pipeline end-to-end
// (registration, serialization, instantiation, tick, and physics contacts).
//

#pragma once

#include <Seraph/Scripts/ScriptableEntity.h>

namespace Sandbox
{

// Spins its entity around Y and logs its lifecycle + collisions. Attach it to a
// plain entity to watch it spin, or to a dynamic rigid body to see
// OnCollisionBegin fire when it lands on something.
class RotatorScript : public Seraph::ScriptableEntity
{
public:
    void OnCreate() override;
    void OnUpdate(f64 dt) override;
    void OnCollisionBegin(Seraph::Entity other) override;

private:
    float m_DegreesPerSecond = 90.0f;
};

} // namespace Sandbox
