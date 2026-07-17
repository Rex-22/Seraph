//
// Created by Ruben on 2026/07/14.
//

#include "Spinner.h"

#include <Seraph/Core/Log.h>

namespace Sandbox {

void Spinner::OnCreate()
{
    SP_INFO_TAG("Scripting", "SpinnerScript::OnCreate (entity {})",
        static_cast<u64>(GetUUID()));
}

void Spinner::OnUpdate(f64 dt)
{
    glm::vec3 euler = Transform().GetRotationEuler();
    euler.z += glm::radians(m_DegreesPerSecond) * static_cast<float>(dt);
    Transform().SetRotationEuler(euler);
}

void Spinner::OnCollisionBegin(Seraph::Entity other)
{
    SP_INFO_TAG("Scripting", "SpinnerScript collided with entity {}",
        static_cast<u64>(other.GetUUID()));
}

} // Sandbox