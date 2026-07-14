//
// Created by Ruben on 2026/07/14.
//

#pragma once

#include <Seraph/Scripts/ScriptableEntity.h>

namespace Sandbox
{

class Spinner: public Seraph::ScriptableEntity
{
public:
    void OnCreate() override;
    void OnUpdate(f64 dt) override;
    void OnCollisionBegin(Seraph::Entity other) override;

private:
    float m_DegreesPerSecond = 90.0f;
};

} // namespace Sandbox
