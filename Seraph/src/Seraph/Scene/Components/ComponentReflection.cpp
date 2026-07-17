//
// Runtime reflection registrations for the ECS components the entity inspector
// draws via PropertyDrawer. Hand-written (so they work with SeraphHeaderTool
// off); when SHT is enabled these could be generated from SPROPERTY annotations
// instead. Metadata uses the Setting::Attr vocabulary the PropertyDrawer reads.
//
// v2.6 scope: the pure-data physics components (RigidBody + colliders + the
// nested ColliderMaterial + BodyType enum). Transform/Camera/Mesh/Script keep
// bespoke inspector widgets (colored vec3 + euler/quat sync, SceneCamera getters,
// AssetRef picker, script-class dropdown) — see EntityInspectorPanel.
//

#include "Seraph/Physics/PhysicsTypes.h"
#include "Seraph/Reflection/Reflect.h"
#include "Seraph/Scene/Components/BoxColliderComponent.h"
#include "Seraph/Scene/Components/CapsuleColliderComponent.h"
#include "Seraph/Scene/Components/RigidBodyComponent.h"
#include "Seraph/Scene/Components/SphereColliderComponent.h"
#include "Seraph/Settings/SettingDescriptor.h"

using namespace Seraph;

// SP_REFLECT_ENUM(Seraph::BodyType)
//     .Value("Static", Seraph::BodyType::Static)
//     .Value("Dynamic", Seraph::BodyType::Dynamic)
//     .Value("Kinematic", Seraph::BodyType::Kinematic)
// SP_REFLECT_ENUM_END();

SP_REFLECT_TYPE(Seraph::ColliderMaterial)
    .Property<&Seraph::ColliderMaterial::Friction>("Friction")
        .Attr(Setting::Attr::Min, Any(0.0f)).Attr(Setting::Attr::Max, Any(1.0f))
    .Property<&Seraph::ColliderMaterial::Restitution>("Restitution")
        .Attr(Setting::Attr::Min, Any(0.0f)).Attr(Setting::Attr::Max, Any(1.0f))
SP_REFLECT_END();

SP_REFLECT_TYPE(Seraph::RigidBodyComponent)
    .Property<&Seraph::RigidBodyComponent::Type>("Body Type")
    // LayerID is drawn by a bespoke Layer dropdown (PhysicsLayerManager registry),
    // so hide it from the generic reflection walk.
    .Property<&Seraph::RigidBodyComponent::LayerID>("LayerID")
        .Attr(Setting::Attr::Flags, Any(static_cast<u32>(SettingFlag_Hidden)))
    .Property<&Seraph::RigidBodyComponent::Mass>("Mass")
        .Attr(Setting::Attr::Min, Any(0.0f))
    .Property<&Seraph::RigidBodyComponent::LinearDrag>("Linear Drag")
        .Attr(Setting::Attr::Min, Any(0.0f))
    .Property<&Seraph::RigidBodyComponent::AngularDrag>("Angular Drag")
        .Attr(Setting::Attr::Min, Any(0.0f))
    .Property<&Seraph::RigidBodyComponent::GravityFactor>("Gravity Factor")
    .Property<&Seraph::RigidBodyComponent::InitialLinearVelocity>("Init Lin Vel")
    .Property<&Seraph::RigidBodyComponent::InitialAngularVelocity>("Init Ang Vel")
SP_REFLECT_END();

SP_REFLECT_TYPE(Seraph::BoxColliderComponent)
    .Property<&Seraph::BoxColliderComponent::HalfExtents>("Half Extents")
    .Property<&Seraph::BoxColliderComponent::Offset>("Offset")
    .Property<&Seraph::BoxColliderComponent::IsTrigger>("Is Trigger")
    .Property<&Seraph::BoxColliderComponent::Material>("Material")
SP_REFLECT_END();

SP_REFLECT_TYPE(Seraph::SphereColliderComponent)
    .Property<&Seraph::SphereColliderComponent::Radius>("Radius")
        .Attr(Setting::Attr::Min, Any(0.0f))
    .Property<&Seraph::SphereColliderComponent::Offset>("Offset")
    .Property<&Seraph::SphereColliderComponent::IsTrigger>("Is Trigger")
    .Property<&Seraph::SphereColliderComponent::Material>("Material")
SP_REFLECT_END();

SP_REFLECT_TYPE(Seraph::CapsuleColliderComponent)
    .Property<&Seraph::CapsuleColliderComponent::Radius>("Radius")
        .Attr(Setting::Attr::Min, Any(0.0f))
    .Property<&Seraph::CapsuleColliderComponent::HalfHeight>("Half Height")
        .Attr(Setting::Attr::Min, Any(0.0f))
    .Property<&Seraph::CapsuleColliderComponent::Offset>("Offset")
    .Property<&Seraph::CapsuleColliderComponent::IsTrigger>("Is Trigger")
    .Property<&Seraph::CapsuleColliderComponent::Material>("Material")
SP_REFLECT_END();
