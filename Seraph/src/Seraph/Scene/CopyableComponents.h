//
// The set of component types Scene::Copy duplicates automatically by iterating
// this registry. A component belongs here only if it can be copied on its own,
// independent of any other component.
//
// Handled explicitly by Scene::Copy (NOT listed here):
//   - IDComponent           : identity; the copy reuses the source UUID via
//                             CreateEntityWithUUID, so it is never re-copied.
//   - RelationshipComponent : pure UUIDs, copied verbatim so the parent/child
//                             hierarchy stays valid (UUIDs are stable).
//   - RigidBodyComponent    : copied LAST, after transforms + colliders, so a
//                             future signal-driven body-creation path sees a
//                             fully-formed entity.
//

#pragma once

#include "Seraph/Reflection/TypeRegistry.h"
#include "Seraph/Scene/Components/BoxColliderComponent.h"
#include "Seraph/Scene/Components/CameraComponent.h"
#include "Seraph/Scene/Components/CapsuleColliderComponent.h"
#include "Seraph/Scene/Components/MeshComponent.h"
#include "Seraph/Scene/Components/SphereColliderComponent.h"
#include "Seraph/Scene/Components/TagComponent.h"
#include "Seraph/Scene/Components/TransformComponent.h"
#include "Seraph/Scripts/ScriptComponent.h"

namespace Seraph
{

//   - ScriptComponent : only ScriptClass matters; Copy runs on the authored
//                       (non-playing) scene where Instance is always null, so
//                       duplicating the pointer never aliases a live instance.
using CopyableComponents = TypeRegistry<
    TagComponent,
    TransformComponent,
    MeshComponent,
    CameraComponent,
    BoxColliderComponent,
    SphereColliderComponent,
    CapsuleColliderComponent,
    ScriptComponent>;

} // namespace Seraph
