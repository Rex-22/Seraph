#include "SceneSerializer.h"

#include "Seraph/Asset/AssetManager.h"
#include "Seraph/Asset/AssetRef.h"
#include "Seraph/Core/Log.h"
#include "Seraph/Graphics/SceneCamera.h"
#include "Seraph/Physics/PhysicsTypes.h"
#include "Seraph/Scene/Components/BoxColliderComponent.h"
#include "Seraph/Scene/Components/CameraComponent.h"
#include "Seraph/Scene/Components/CapsuleColliderComponent.h"
#include "Seraph/Scene/Components/IDComponent.h"
#include "Seraph/Scene/Components/MeshComponent.h"
#include "Seraph/Scene/Components/RelationshipComponent.h"
#include "Seraph/Scene/Components/RigidBodyComponent.h"
#include "Seraph/Scene/Components/SphereColliderComponent.h"
#include "Seraph/Scene/Components/TagComponent.h"
#include "Seraph/Scene/Components/TransformComponent.h"
#include "Seraph/Scene/Entity.h"
#include "Seraph/Scene/SceneAsset.h"
#include "Seraph/Scripts/ScriptComponent.h"
#include "Seraph/Utilities/YAMLSerializationHelpers.h"

#include <glm/glm.hpp>
#include <yaml-cpp/yaml.h>

#include <algorithm>
#include <string>
#include <vector>

namespace Seraph
{
namespace
{

YAML::Emitter& operator<<(YAML::Emitter& emitter, const glm::vec3& v)
{
    emitter << YAML::Flow << YAML::BeginSeq << v.x << v.y << v.z << YAML::EndSeq;
    return emitter;
}

glm::vec3 DecodeVec3(const YAML::Node& node, const glm::vec3& fallback)
{
    if (!node || !node.IsSequence() || node.size() != 3)
        return fallback;
    return { node[0].as<float>(), node[1].as<float>(), node[2].as<float>() };
}

const char* ProjectionTypeToString(SceneCamera::ProjectionType type)
{
    return type == SceneCamera::ProjectionType::Orthographic ? "Orthographic"
                                                             : "Perspective";
}

SceneCamera::ProjectionType ProjectionTypeFromString(const std::string& s)
{
    return s == "Orthographic" ? SceneCamera::ProjectionType::Orthographic
                               : SceneCamera::ProjectionType::Perspective;
}

void SerializeEntity(YAML::Emitter& emitter, Entity entity)
{
    emitter << YAML::BeginMap;
    emitter << YAML::Key << "Entity" << YAML::Value << static_cast<u64>(entity.GetUUID());

    if (entity.HasComponent<TagComponent>())
        emitter << YAML::Key << "Tag" << YAML::Value
                << entity.GetComponent<TagComponent>().Tag;

    if (entity.HasComponent<TransformComponent>()) {
        const auto& t = entity.GetComponent<TransformComponent>();
        emitter << YAML::Key << "Transform" << YAML::Value << YAML::BeginMap;
        emitter << YAML::Key << "Translation" << YAML::Value << t.Translation;
        emitter << YAML::Key << "Rotation" << YAML::Value << t.GetRotationEuler();
        emitter << YAML::Key << "Scale" << YAML::Value << t.Scale;
        emitter << YAML::EndMap;
    }

    if (entity.HasComponent<CameraComponent>()) {
        const auto& cc = entity.GetComponent<CameraComponent>();
        const SceneCamera& cam = cc.Camera;
        emitter << YAML::Key << "Camera" << YAML::Value << YAML::BeginMap;
        emitter << YAML::Key << "ProjectionType" << YAML::Value
                << ProjectionTypeToString(cam.GetProjectionType());
        emitter << YAML::Key << "IsPrimary" << YAML::Value << cc.IsPrimary;
        emitter << YAML::Key << "PerspectiveFov" << YAML::Value
                << cam.GetDegPerspectiveVerticalFOV();
        emitter << YAML::Key << "PerspectiveNear" << YAML::Value
                << cam.GetPerspectiveNearClip();
        emitter << YAML::Key << "PerspectiveFar" << YAML::Value
                << cam.GetPerspectiveFarClip();
        emitter << YAML::Key << "OrthographicSize" << YAML::Value
                << cam.GetOrthographicSize();
        emitter << YAML::Key << "OrthographicNear" << YAML::Value
                << cam.GetOrthographicNearClip();
        emitter << YAML::Key << "OrthographicFar" << YAML::Value
                << cam.GetOrthographicFarClip();
        emitter << YAML::EndMap;
    }

    if (entity.HasComponent<MeshComponent>()) {
        const auto& mc = entity.GetComponent<MeshComponent>();
        emitter << YAML::Key << "Mesh" << YAML::Value << YAML::BeginMap;
        emitter << YAML::Key << "Mesh" << YAML::Value
                << static_cast<u64>(mc.Mesh.Handle());
        if (!mc.MaterialOverrides.empty()) {
            emitter << YAML::Key << "MaterialOverrides" << YAML::Value
                    << YAML::Flow << YAML::BeginSeq;
            for (const AssetHandle handle : mc.MaterialOverrides)
                emitter << static_cast<u64>(handle);
            emitter << YAML::EndSeq;
        }
        emitter << YAML::EndMap;
    }

    if (entity.HasComponent<RigidBodyComponent>()) {
        const auto& rb = entity.GetComponent<RigidBodyComponent>();
        emitter << YAML::Key << "RigidBody" << YAML::Value << YAML::BeginMap;
        emitter << YAML::Key << "BodyType" << YAML::Value << static_cast<int>(rb.Type);
        emitter << YAML::Key << "LayerID" << YAML::Value << rb.LayerID;
        emitter << YAML::Key << "Mass" << YAML::Value << rb.Mass;
        emitter << YAML::Key << "LinearDrag" << YAML::Value << rb.LinearDrag;
        emitter << YAML::Key << "AngularDrag" << YAML::Value << rb.AngularDrag;
        emitter << YAML::Key << "GravityFactor" << YAML::Value << rb.GravityFactor;
        emitter << YAML::Key << "InitialLinearVelocity" << YAML::Value
                << rb.InitialLinearVelocity;
        emitter << YAML::Key << "InitialAngularVelocity" << YAML::Value
                << rb.InitialAngularVelocity;
        emitter << YAML::EndMap;
    }

    if (entity.HasComponent<BoxColliderComponent>()) {
        const auto& c = entity.GetComponent<BoxColliderComponent>();
        emitter << YAML::Key << "BoxCollider" << YAML::Value << YAML::BeginMap;
        emitter << YAML::Key << "HalfExtents" << YAML::Value << c.HalfExtents;
        emitter << YAML::Key << "Offset" << YAML::Value << c.Offset;
        emitter << YAML::Key << "IsTrigger" << YAML::Value << c.IsTrigger;
        emitter << YAML::Key << "Friction" << YAML::Value << c.Material.Friction;
        emitter << YAML::Key << "Restitution" << YAML::Value << c.Material.Restitution;
        emitter << YAML::EndMap;
    }

    if (entity.HasComponent<SphereColliderComponent>()) {
        const auto& c = entity.GetComponent<SphereColliderComponent>();
        emitter << YAML::Key << "SphereCollider" << YAML::Value << YAML::BeginMap;
        emitter << YAML::Key << "Radius" << YAML::Value << c.Radius;
        emitter << YAML::Key << "Offset" << YAML::Value << c.Offset;
        emitter << YAML::Key << "IsTrigger" << YAML::Value << c.IsTrigger;
        emitter << YAML::Key << "Friction" << YAML::Value << c.Material.Friction;
        emitter << YAML::Key << "Restitution" << YAML::Value << c.Material.Restitution;
        emitter << YAML::EndMap;
    }

    if (entity.HasComponent<CapsuleColliderComponent>()) {
        const auto& c = entity.GetComponent<CapsuleColliderComponent>();
        emitter << YAML::Key << "CapsuleCollider" << YAML::Value << YAML::BeginMap;
        emitter << YAML::Key << "Radius" << YAML::Value << c.Radius;
        emitter << YAML::Key << "HalfHeight" << YAML::Value << c.HalfHeight;
        emitter << YAML::Key << "Offset" << YAML::Value << c.Offset;
        emitter << YAML::Key << "IsTrigger" << YAML::Value << c.IsTrigger;
        emitter << YAML::Key << "Friction" << YAML::Value << c.Material.Friction;
        emitter << YAML::Key << "Restitution" << YAML::Value << c.Material.Restitution;
        emitter << YAML::EndMap;
    }

    if (entity.HasComponent<RelationshipComponent>()) {
        const auto& rc = entity.GetComponent<RelationshipComponent>();
        emitter << YAML::Key << "Relationship" << YAML::Value << YAML::BeginMap;
        emitter << YAML::Key << "Parent" << YAML::Value
                << static_cast<u64>(rc.ParentHandle);
        emitter << YAML::Key << "Children" << YAML::Value << YAML::BeginSeq;
        for (UUID child : rc.Children)
            emitter << static_cast<u64>(child);
        emitter << YAML::EndSeq;
        emitter << YAML::EndMap;
    }

    if (entity.HasComponent<ScriptComponent>()) {
        const auto& sc = entity.GetComponent<ScriptComponent>();
        emitter << YAML::Key << "Script" << YAML::Value << YAML::BeginMap;
        emitter << YAML::Key << "ScriptClass" << YAML::Value << sc.ScriptClass;
        emitter << YAML::EndMap;
    }

    emitter << YAML::EndMap;
}

} // namespace

Ref<Asset> SceneSerializer::LoadData(const AssetMetadata&, const Buffer& bytes)
{
    if (!bytes)
        return nullptr;

    YAML::Node data;
    try {
        data = YAML::Load(std::string(
            reinterpret_cast<const char*>(bytes.Data()), bytes.Size()));
    } catch (const std::exception& e) {
        SP_CORE_ERROR_TAG("Scene", "Failed to parse scene YAML: {}", e.what());
        return nullptr;
    }

    const std::string sceneName =
        data["Scene"] ? data["Scene"].as<std::string>() : "Untitled Scene";
    Ref<Scene> scene = Ref<Scene>::Create(sceneName);

    if (const YAML::Node entities = data["Entities"]; entities && entities.IsSequence()) {
        for (const auto& node : entities) {
            if (!node["Entity"])
                continue;
            const u64 uuid = node["Entity"].as<u64>();
            const std::string tag =
                node["Tag"] ? node["Tag"].as<std::string>() : std::string();

            // Auto-adds ID / Transform / Tag / Relationship — we populate below.
            Entity entity = scene->CreateEntityWithUUID(uuid, tag);

            if (const YAML::Node t = node["Transform"]) {
                auto& tc = entity.GetComponent<TransformComponent>();
                tc.Translation = DecodeVec3(t["Translation"], glm::vec3(0.0f));
                tc.Scale = DecodeVec3(t["Scale"], glm::vec3(1.0f));
                tc.SetRotationEuler(DecodeVec3(t["Rotation"], glm::vec3(0.0f)));
            }

            if (const YAML::Node c = node["Camera"]) {
                auto& cc = entity.AddComponent<CameraComponent>();
                SceneCamera& cam = cc.Camera;
                const SceneCamera::ProjectionType proj = ProjectionTypeFromString(
                    c["ProjectionType"].as<std::string>(std::string("Perspective")));
                cam.SetProjectionType(proj);
                cam.SetDegPerspectiveVerticalFOV(c["PerspectiveFov"].as<float>(45.0f));
                cam.SetPerspectiveNearClip(c["PerspectiveNear"].as<float>(0.01f));
                cam.SetPerspectiveFarClip(c["PerspectiveFar"].as<float>(1000.0f));
                cam.SetOrthographicSize(c["OrthographicSize"].as<float>(10.0f));
                cam.SetOrthographicNearClip(c["OrthographicNear"].as<float>(-1.0f));
                cam.SetOrthographicFarClip(c["OrthographicFar"].as<float>(1.0f));
                cc.IsPrimary = c["IsPrimary"].as<bool>(false);
                cc.ProjectionType =
                    proj == SceneCamera::ProjectionType::Orthographic
                        ? CameraComponent::Type::Orthographic
                        : CameraComponent::Type::Perspective;
            }

            if (const YAML::Node m = node["Mesh"]) {
                auto& mc = entity.AddComponent<MeshComponent>();
                auto handle = m["Mesh"].as<AssetHandle>(0);
                if (AssetManager::IsAssetHandleValid(handle))
                {
                    mc.Mesh = handle;
                } else {
                    SP_CORE_ERROR_TAG("SceneSerializer", "Missing asset {}", handle);
                    mc.Mesh = 0;
                }

                if (const YAML::Node overrides = m["MaterialOverrides"];
                    overrides && overrides.IsSequence())
                    for (const auto& o : overrides)
                        mc.MaterialOverrides.emplace_back(o.as<u64>(0));
            }

            if (const YAML::Node n = node["RigidBody"]) {
                auto& rb = entity.AddComponent<RigidBodyComponent>();
                rb.Type = static_cast<BodyType>(
                    n["BodyType"].as<int>(static_cast<int>(BodyType::Static)));
                rb.LayerID = n["LayerID"].as<u32>(0u);
                rb.Mass = n["Mass"].as<float>(1.0f);
                rb.LinearDrag = n["LinearDrag"].as<float>(0.01f);
                rb.AngularDrag = n["AngularDrag"].as<float>(0.05f);
                rb.GravityFactor = n["GravityFactor"].as<float>(1.0f);
                rb.InitialLinearVelocity =
                    DecodeVec3(n["InitialLinearVelocity"], glm::vec3(0.0f));
                rb.InitialAngularVelocity =
                    DecodeVec3(n["InitialAngularVelocity"], glm::vec3(0.0f));
            }

            if (const YAML::Node n = node["BoxCollider"]) {
                auto& c = entity.AddComponent<BoxColliderComponent>();
                c.HalfExtents = DecodeVec3(n["HalfExtents"], glm::vec3(0.5f));
                c.Offset = DecodeVec3(n["Offset"], glm::vec3(0.0f));
                c.IsTrigger = n["IsTrigger"].as<bool>(false);
                c.Material.Friction = n["Friction"].as<float>(0.5f);
                c.Material.Restitution = n["Restitution"].as<float>(0.15f);
            }

            if (const YAML::Node n = node["SphereCollider"]) {
                auto& c = entity.AddComponent<SphereColliderComponent>();
                c.Radius = n["Radius"].as<float>(0.5f);
                c.Offset = DecodeVec3(n["Offset"], glm::vec3(0.0f));
                c.IsTrigger = n["IsTrigger"].as<bool>(false);
                c.Material.Friction = n["Friction"].as<float>(0.5f);
                c.Material.Restitution = n["Restitution"].as<float>(0.15f);
            }

            if (const YAML::Node n = node["CapsuleCollider"]) {
                auto& c = entity.AddComponent<CapsuleColliderComponent>();
                c.Radius = n["Radius"].as<float>(0.5f);
                c.HalfHeight = n["HalfHeight"].as<float>(0.5f);
                c.Offset = DecodeVec3(n["Offset"], glm::vec3(0.0f));
                c.IsTrigger = n["IsTrigger"].as<bool>(false);
                c.Material.Friction = n["Friction"].as<float>(0.5f);
                c.Material.Restitution = n["Restitution"].as<float>(0.15f);
            }

            if (const YAML::Node s = node["Script"]) {
                auto& sc = entity.AddComponent<ScriptComponent>();
                sc.ScriptClass = s["ScriptClass"].as<std::string>(std::string());
            }

            if (const YAML::Node r = node["Relationship"]) {
                auto& rc = entity.GetComponent<RelationshipComponent>();
                rc.ParentHandle = r["Parent"].as<u64>(0);
                rc.Children.clear();
                if (const YAML::Node children = r["Children"];
                    children && children.IsSequence())
                    for (const auto& ch : children)
                        rc.Children.emplace_back(ch.as<u64>());
            }
        }
    }

    return Ref<SceneAsset>::Create(scene);
}

bool SceneSerializer::Serialize(
    const AssetMetadata&, const Ref<Asset>& asset, Buffer& out)
{
    Ref<SceneAsset> sceneAsset = asset.As<SceneAsset>();
    if (!sceneAsset)
        return false;

    Ref<Scene> scene = sceneAsset->GetScene();
    if (!scene)
        return false;

    // Collect entities and sort by UUID for stable, diff-friendly output.
    std::vector<Entity> entities;
    for (auto handle : scene->GetAllEntitiesWith<IDComponent>())
        entities.emplace_back(handle, scene.Raw());
    std::sort(entities.begin(), entities.end(), [](Entity a, Entity b) {
        return static_cast<u64>(a.GetUUID()) < static_cast<u64>(b.GetUUID());
    });

    YAML::Emitter emitter;
    emitter << YAML::BeginMap;
    emitter << YAML::Key << "Scene" << YAML::Value << scene->GetName();
    emitter << YAML::Key << "Entities" << YAML::Value << YAML::BeginSeq;
    for (Entity entity : entities)
        SerializeEntity(emitter, entity);
    emitter << YAML::EndSeq;
    emitter << YAML::EndMap;

    out = Buffer::Copy(emitter.c_str(), static_cast<u64>(emitter.size()));
    return static_cast<bool>(out);
}

} // namespace Seraph
