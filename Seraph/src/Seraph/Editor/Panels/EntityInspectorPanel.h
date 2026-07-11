//
// Created by ruben on 2026/07/11.
//

#pragma once

#include "Seraph/Core/Ref.h"
#include "Seraph/Scene/Entity.h"

namespace Seraph
{

class Material;

class EntityInspectorPanel
{
public:
    EntityInspectorPanel() = default;

    void SetSelectedEntity(Entity entity) { m_SelectedEntity = entity; }
    Entity GetSelectedEntity() const { return m_SelectedEntity; }

    // Material used when creating new mesh components from the inspector.
    void SetDefaultMaterial(Ref<Material> material);

    void OnImGuiRender();

private:
    void DrawTagComponent();
    void DrawTransformComponent();
    void DrawCameraComponent();
    void DrawMeshComponent();
    void DrawAddComponentMenu();

    Entity m_SelectedEntity;
    Ref<Material> m_DefaultMaterial;
};

} // namespace Seraph
