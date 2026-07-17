//
// Sandbox demo script implementation. See RotatorScript.h.
//

#include "RotatorScript.h"

#include <Seraph/Core/Log.h>

#include <glm/glm.hpp>

namespace Sandbox
{

void RotatorScript::OnCreate()
{
    SP_INFO_TAG("Scripting", "RotatorScript::OnCreate (entity {})",
        static_cast<u64>(GetUUID()));
}

void RotatorScript::OnUpdate(f64 dt)
{
    glm::vec3 euler = Transform().GetRotationEuler();
    euler.y += glm::radians(m_DegreesPerSecond) * static_cast<float>(dt);
    Transform().SetRotationEuler(euler);
}

void RotatorScript::OnCollisionBegin(Seraph::Entity other)
{
    SP_INFO_TAG("Scripting", "RotatorScript collided with entity {}",
        static_cast<u64>(other.GetUUID()));
}

} // namespace Sandbox
