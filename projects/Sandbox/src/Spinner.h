//
// Created by Ruben on 2026/07/14.
//

#pragma once

#include <Seraph/Reflection/Annotations.h>
#include <Seraph/Reflection/Reflect.h>
#include <Seraph/Scripts/ScriptableEntity.h>

namespace Sandbox
{

class SCLASS() Spinner : public Seraph::ScriptableEntity
{
    SP_REFLECT(Spinner);

public:
    void OnCreate() override;
    void OnUpdate(f64 dt) override;
    void OnCollisionBegin(Seraph::Entity other) override;

private:
    SPROPERTY(settings.display = "Degrees / sec", settings.step = 1.0f)
    float m_DegreesPerSecond = 90.0f;
};

} // namespace Sandbox
