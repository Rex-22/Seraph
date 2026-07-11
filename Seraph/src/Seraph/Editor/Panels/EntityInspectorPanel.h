//
// Created by ruben on 2026/07/11.
//

#pragma once

#include "Seraph/Scene/Entity.h"

namespace Seraph
{

class EntityInspectorPanel
{
public:
    EntityInspectorPanel() = default;

    void SetSelectedEntity(Entity entity) { m_SelectedEntity = entity; }
    Entity GetSelectedEntity() const { return m_SelectedEntity; }

    void OnImGuiRender();

private:
    void DrawTagComponent();
    void DrawTransformComponent();
    void DrawCameraComponent();
    void DrawMeshComponent();
    void DrawAddComponentMenu();

    Entity m_SelectedEntity;
};

} // namespace Seraph
