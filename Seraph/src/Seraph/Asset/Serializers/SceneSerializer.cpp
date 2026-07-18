#include "SceneSerializer.h"

#include "Seraph/Asset/AssetManager.h"
#include "Seraph/Asset/AssetRef.h"
#include "Seraph/Asset/Serializers/SerializationAttributes.h"
#include "Seraph/Core/Log.h"
#include "Seraph/Core/UUID.h"
#include "Seraph/Graphics/SceneCamera.h"
#include "Seraph/Reflection/Any.h"
#include "Seraph/Reflection/Reflection.h"
#include "Seraph/Reflection/Type.h"
#include "Seraph/Reflection/TypeId.h"
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
#include "Seraph/Scene/CopyableComponents.h"
#include "Seraph/Scene/Entity.h"
#include "Seraph/Scene/SceneAsset.h"
#include "Seraph/Scripts/ScriptComponent.h"
#include "Seraph/Scripts/ScriptTypes.h"
#include "Seraph/Scripts/ScriptableEntity.h"
#include "Seraph/Utilities/YAMLSerializationHelpers.h"

#include <glm/glm.hpp>
#include <yaml-cpp/yaml.h>

#include <algorithm>
#include <string>
#include <type_traits>
#include <vector>

namespace Seraph
{
namespace
{

// --- Serialization completeness guard ------------------------------------
// Emit/parse are hand-written per component, so a component that becomes part
// of a scene without matching blocks here silently fails to persist — the bug
// class fixed in commit 5f94d22 ("scripts not serialized"). This compile-time
// guard trips when a CopyableComponents type is missing from the serialized
// set below, forcing whoever adds the component to wire up SerializeEntity /
// LoadData too.
template<typename... Ts>
struct SerializedComponentList
{
    template<typename U>
    static constexpr bool Contains = (std::is_same_v<U, Ts> || ...);
};

// Every component type SceneSerializer persists. Keep in sync with the emit
// blocks in SerializeEntity and the parse blocks in LoadData.
using SerializedComponents = SerializedComponentList<
    TagComponent, TransformComponent, MeshComponent, CameraComponent,
    RigidBodyComponent, BoxColliderComponent, SphereColliderComponent,
    CapsuleColliderComponent, RelationshipComponent, ScriptComponent>;

template<typename Registry>
struct AllCopyablesSerialized;

template<typename... Cs>
struct AllCopyablesSerialized<TypeRegistry<Cs...>>
{
    static constexpr bool value = (SerializedComponents::template Contains<Cs> && ...);
};

static_assert(AllCopyablesSerialized<CopyableComponents>::value,
    "A CopyableComponents type has no SceneSerializer support. Add its emit "
    "block to SerializeEntity and its parse block to LoadData, then list it in "
    "SerializedComponents.");

// --- Reflection-driven component (de)serialization -----------------------
// Generic emit/parse for the pure-data components (RigidBody, colliders,
// Relationship, Script), driven by their reflected properties. Deliberately
// reproduces the pre-existing hand-written format byte-for-byte: enum -> int,
// UUID/AssetHandle -> u64, vec -> flow seq, vector -> block seq, and the
// Serialize::Attr::{Key,Flatten} overrides. Camera/Mesh/Transform/Tag stay
// bespoke below (getter-based / AssetRef / private-euler shapes).

std::string SerializeKey(const Property& p)
{
    if (const std::string* k = p.Attrs.Get<std::string>(Serialize::Attr::Key))
        return *k;
    return std::string(p.Name);
}

// Emit a single reflected value with the scene format's conventions.
void EmitAny(YAML::Emitter& e, const Any& v, const Type* t)
{
    if (!t || v.IsEmpty()) {
        e << YAML::Null;
        return;
    }
    if (t->Kind == TypeKind::Enum) { // enum -> underlying int
        // Guard the cast: a container element whose declared ElementType is an enum
        // but whose Any holds a different representation would otherwise null-deref.
        if (const s64* i = v.Cast<s64>())
            e << static_cast<int>(*i);
        else {
            SP_CORE_WARN_TAG("SceneSerializer",
                "enum value/type mismatch emitting '{}'", t->Name);
            e << YAML::Null;
        }
        return;
    }
    if (t->Kind == TypeKind::Reference) { // reference -> its target's id (u64)
        // A scalar reference property Get() yields Any(UUID); a container of
        // reference wrappers currently yields Any(wrapper), which does NOT cast to
        // UUID — guard rather than deref null (container-of-reference is unsupported;
        // see the tech-debt note).
        if (const UUID* id = v.Cast<UUID>())
            e << static_cast<u64>(*id);
        else {
            SP_CORE_WARN_TAG("SceneSerializer",
                "reference value/type mismatch emitting '{}' (container of "
                "references is not supported)", t->Name);
            e << YAML::Null;
        }
        return;
    }
    const TypeId id = t->Id;
    if (id == TypeIdOf<bool>())              e << *v.Cast<bool>();
    else if (id == TypeIdOf<s32>())          e << *v.Cast<s32>();
    else if (id == TypeIdOf<u32>())          e << *v.Cast<u32>();
    else if (id == TypeIdOf<s64>())          e << *v.Cast<s64>();
    else if (id == TypeIdOf<u64>())          e << *v.Cast<u64>();
    else if (id == TypeIdOf<f32>())          e << *v.Cast<f32>();
    else if (id == TypeIdOf<f64>())          e << *v.Cast<f64>();
    else if (id == TypeIdOf<std::string>())  e << *v.Cast<std::string>();
    else if (id == TypeIdOf<UUID>())         e << static_cast<u64>(*v.Cast<UUID>());
    else if (id == TypeIdOf<glm::vec2>()) {
        const auto& p = *v.Cast<glm::vec2>();
        e << YAML::Flow << YAML::BeginSeq << p.x << p.y << YAML::EndSeq;
    } else if (id == TypeIdOf<glm::vec3>()) {
        const auto& p = *v.Cast<glm::vec3>();
        e << YAML::Flow << YAML::BeginSeq << p.x << p.y << p.z << YAML::EndSeq;
    } else if (id == TypeIdOf<glm::vec4>()) {
        const auto& p = *v.Cast<glm::vec4>();
        e << YAML::Flow << YAML::BeginSeq << p.x << p.y << p.z << p.w << YAML::EndSeq;
    } else {
        SP_CORE_WARN_TAG("SceneSerializer", "no emit for reflected type '{}'", t->Name);
        e << YAML::Null;
    }
}

// Emit a reflected object's properties into an already-open map. Flatten inlines
// a nested struct's fields at this level; containers become block sequences.
void EmitProps(YAML::Emitter& e, const Type& type, void* obj)
{
    for (const Property& p : type.Properties) {
        const Type* pt = p.PropType;

        if (pt && pt->Kind == TypeKind::Struct && p.GetAddress) {
            if (p.Attrs.Has(Serialize::Attr::Flatten)) {
                EmitProps(e, *pt, p.GetAddress(obj)); // inline, no key/map
            } else {
                // Non-flattened nested struct: emit as a nested map under its key
                e << YAML::Key << SerializeKey(p) << YAML::Value << YAML::BeginMap;
                EmitProps(e, *pt, p.GetAddress(obj));
                e << YAML::EndMap;
            }
            continue;
        }

        if (pt && pt->Kind == TypeKind::Container && pt->Container && p.GetAddress) {
            const ContainerInfo& ci = *pt->Container;
            void* c = p.GetAddress(obj);
            const std::size_t n = ci.Size(c);
            if (n == 0 && p.Attrs.Has(Serialize::Attr::OmitEmpty))
                continue; // no key emitted for an empty container
            e << YAML::Key << SerializeKey(p) << YAML::Value;
            if (p.Attrs.Has(Serialize::Attr::Flow))
                e << YAML::Flow;
            e << YAML::BeginSeq;
            for (std::size_t i = 0; i < n; ++i)
                EmitAny(e, ci.GetElement(c, i), ci.ElementType);
            e << YAML::EndSeq;
        } else {
            e << YAML::Key << SerializeKey(p) << YAML::Value;
            EmitAny(e, p.Get(obj), pt);
        }
    }
}

void SerializeComponent(YAML::Emitter& e, const char* block, const Type& type, void* obj)
{
    e << YAML::Key << block << YAML::Value << YAML::BeginMap;
    EmitProps(e, type, obj);
    e << YAML::EndMap;
}

// Parse a scalar/enum/uuid/vec value from a YAML node into an Any of `t`. Returns
// an empty Any if the node can't be read as that type (caller keeps the default).
Any ParseAny(const YAML::Node& node, const Type* t)
{
    if (!t || !node)
        return {};
    try {
        if (t->Kind == TypeKind::Enum)
            return Any(static_cast<s64>(node.as<int>()));
        if (t->Kind == TypeKind::Reference)
            return Any(UUID(node.as<u64>()));
        const TypeId id = t->Id;
        if (id == TypeIdOf<bool>())              return Any(node.as<bool>());
        if (id == TypeIdOf<s32>())               return Any(node.as<s32>());
        if (id == TypeIdOf<u32>())               return Any(node.as<u32>());
        if (id == TypeIdOf<s64>())               return Any(node.as<s64>());
        if (id == TypeIdOf<u64>())               return Any(node.as<u64>());
        if (id == TypeIdOf<f32>())               return Any(node.as<f32>());
        if (id == TypeIdOf<f64>())               return Any(node.as<f64>());
        if (id == TypeIdOf<std::string>())       return Any(node.as<std::string>());
        if (id == TypeIdOf<UUID>())              return Any(UUID(node.as<u64>()));
        if (id == TypeIdOf<glm::vec3>() && node.IsSequence() && node.size() == 3)
            return Any(glm::vec3{node[0].as<float>(), node[1].as<float>(), node[2].as<float>()});
        if (id == TypeIdOf<glm::vec2>() && node.IsSequence() && node.size() == 2)
            return Any(glm::vec2{node[0].as<float>(), node[1].as<float>()});
        if (id == TypeIdOf<glm::vec4>() && node.IsSequence() && node.size() == 4)
            return Any(glm::vec4{node[0].as<float>(), node[1].as<float>(),
                                 node[2].as<float>(), node[3].as<float>()});
    } catch (const std::exception&) {
        // fall through -> empty Any -> keep default
    }
    return {};
}

// Populate a reflected object from a component block node. Absent keys keep the
// object's default (so `obj` should already be a default-constructed component).
void DeserializeComponent(const YAML::Node& node, const Type& type, void* obj)
{
    for (const Property& p : type.Properties) {
        const Type* pt = p.PropType;

        if (pt && pt->Kind == TypeKind::Struct && p.GetAddress
            && p.Attrs.Has(Serialize::Attr::Flatten)) {
            DeserializeComponent(node, *pt, p.GetAddress(obj)); // read from same node
            continue;
        }

        const YAML::Node child = node[SerializeKey(p)];
        if (!child)
            continue; // absent -> keep default

        // Non-flattened nested struct: read from its own sub-map (mirrors EmitProps).
        if (pt && pt->Kind == TypeKind::Struct && p.GetAddress) {
            if (child.IsMap())
                DeserializeComponent(child, *pt, p.GetAddress(obj));
            continue;
        }

        if (pt && pt->Kind == TypeKind::Container && pt->Container && p.GetAddress) {
            if (!child.IsSequence())
                continue;
            const ContainerInfo& ci = *pt->Container;
            void* c = p.GetAddress(obj);
            ci.Resize(c, child.size());
            std::size_t i = 0;
            for (const auto& el : child) {
                Any ev = ParseAny(el, ci.ElementType);
                if (!ev.IsEmpty())
                    ci.SetElement(c, i, ev);
                ++i;
            }
        } else {
            Any v = ParseAny(child, pt);
            if (!v.IsEmpty())
                p.Set(obj, v);
        }
    }
}

// --- Script component (bespoke) -------------------------------------------
// The authored field SET is dynamic — it depends on which class ScriptClass
// names — so it can't be a fixed reflected property. Emit ScriptClass (via the
// reflected ScriptComponent) plus a "Fields" sub-map of the script's reflected
// fields. A transient instance supplies the concrete reflected Type + each
// field's type; it's created and destroyed here (the Game module is already
// loaded — ProjectManager::Open loads it before any scene). If the class is
// unknown (no module / renamed), the Fields round-trip is skipped, not fatal.

void SerializeScript(YAML::Emitter& e, ScriptComponent& sc)
{
    e << YAML::Key << "Script" << YAML::Value << YAML::BeginMap;
    EmitProps(e, Reflection::Get<ScriptComponent>(), &sc); // ScriptClass

    ScriptableEntity* tmp = sc.ScriptClass.empty() && sc.Fields.empty()
                                ? nullptr
                                : ScriptTypes::Create(sc.ScriptClass);
    if (tmp) {
        // Class resolved: emit the live per-field values.
        const Type& type = tmp->GetType();
        e << YAML::Key << "Fields" << YAML::Value << YAML::BeginMap;
        for (const Property& p : type.Properties) {
            const auto it = sc.Fields.find(std::string(p.Name));
            if (it == sc.Fields.end() || it->second.IsEmpty())
                continue;
            e << YAML::Key << SerializeKey(p) << YAML::Value;
            EmitAny(e, it->second, p.PropType);
        }
        e << YAML::EndMap;
        delete tmp;
    } else if (!sc.RawFieldsYaml.empty()) {
        // Class unresolved but we preserved the authored Fields on load — re-emit
        // them verbatim so switching to a build without the module doesn't erase
        // the values (see DeserializeScript). Reparse the stored YAML into a node
        // and stream it back under the same key.
        e << YAML::Key << "Fields" << YAML::Value << YAML::Load(sc.RawFieldsYaml);
    }
    e << YAML::EndMap;
}

void DeserializeScript(const YAML::Node& node, ScriptComponent& sc)
{
    DeserializeComponent(node, Reflection::Get<ScriptComponent>(), &sc); // ScriptClass

    const YAML::Node fields = node["Fields"];
    if (!fields || sc.ScriptClass.empty())
        return;
    if (ScriptableEntity* tmp = ScriptTypes::Create(sc.ScriptClass)) {
        const Type& type = tmp->GetType();
        for (const Property& p : type.Properties) {
            const YAML::Node child = fields[SerializeKey(p)];
            if (!child)
                continue;
            Any v = ParseAny(child, p.PropType);
            if (!v.IsEmpty())
                sc.Fields[std::string(p.Name)] = v;
        }
        delete tmp;
    } else {
        // Class unresolved (module not built yet / renamed). Keep the raw Fields
        // YAML so a later save re-emits the authored values verbatim instead of
        // dropping them (previously this was a silent data loss on round-trip).
        sc.RawFieldsYaml = YAML::Dump(fields);
        SP_CORE_WARN_TAG("Scene",
            "Script class '{}' not resolved on load; preserving its authored "
            "field values verbatim for round-trip", sc.ScriptClass);
    }
}

void SerializeEntity(YAML::Emitter& emitter, Entity entity)
{
    emitter << YAML::BeginMap;
    emitter << YAML::Key << "Entity" << YAML::Value << static_cast<u64>(entity.GetUUID());

    if (entity.HasComponent<TagComponent>())
        emitter << YAML::Key << "Tag" << YAML::Value
                << entity.GetComponent<TagComponent>().Tag;

    if (entity.HasComponent<TransformComponent>())
        SerializeComponent(emitter, "Transform",
            Reflection::Get<TransformComponent>(),
            &entity.GetComponent<TransformComponent>());

    if (entity.HasComponent<CameraComponent>())
        SerializeComponent(emitter, "Camera", Reflection::Get<CameraComponent>(),
            &entity.GetComponent<CameraComponent>());

    if (entity.HasComponent<MeshComponent>())
        SerializeComponent(emitter, "Mesh", Reflection::Get<MeshComponent>(),
            &entity.GetComponent<MeshComponent>());

    // Reflection-driven blocks (pure-data components). Same order + block names
    // + byte format as before; the field emit is now generated from the reflected
    // properties (see SerializeComponent / EmitProps above).
    if (entity.HasComponent<RigidBodyComponent>())
        SerializeComponent(emitter, "RigidBody",
            Reflection::Get<RigidBodyComponent>(),
            &entity.GetComponent<RigidBodyComponent>());

    if (entity.HasComponent<BoxColliderComponent>())
        SerializeComponent(emitter, "BoxCollider",
            Reflection::Get<BoxColliderComponent>(),
            &entity.GetComponent<BoxColliderComponent>());

    if (entity.HasComponent<SphereColliderComponent>())
        SerializeComponent(emitter, "SphereCollider",
            Reflection::Get<SphereColliderComponent>(),
            &entity.GetComponent<SphereColliderComponent>());

    if (entity.HasComponent<CapsuleColliderComponent>())
        SerializeComponent(emitter, "CapsuleCollider",
            Reflection::Get<CapsuleColliderComponent>(),
            &entity.GetComponent<CapsuleColliderComponent>());

    if (entity.HasComponent<RelationshipComponent>())
        SerializeComponent(emitter, "Relationship",
            Reflection::Get<RelationshipComponent>(),
            &entity.GetComponent<RelationshipComponent>());

    if (entity.HasComponent<ScriptComponent>())
        SerializeScript(emitter, entity.GetComponent<ScriptComponent>());

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

            // Transform is auto-added by CreateEntityWithUUID; populate it. The
            // Rotation accessor property routes through SetRotationEuler.
            if (const YAML::Node t = node["Transform"])
                DeserializeComponent(t, Reflection::Get<TransformComponent>(),
                    &entity.GetComponent<TransformComponent>());

            if (const YAML::Node c = node["Camera"])
                DeserializeComponent(c, Reflection::Get<CameraComponent>(),
                    &entity.AddComponent<CameraComponent>());

            if (const YAML::Node m = node["Mesh"]) {
                auto& mc = entity.AddComponent<MeshComponent>();
                DeserializeComponent(m, Reflection::Get<MeshComponent>(), &mc);
                // Keep the authored handle even when it does not currently
                // resolve (transiently-missing asset), but warn — the generic
                // parse already retained it via SetMeshHandle.
                const AssetHandle h = mc.Mesh.Handle();
                if (h != c_NullAssetHandle && !AssetManager::IsAssetHandleValid(h))
                    SP_CORE_WARN_TAG("SceneSerializer",
                        "Mesh asset {} not found; keeping the reference for later resolution",
                        h);
            }

            // Reflection-driven parse (pure-data components). AddComponent gives
            // the defaults; DeserializeComponent overwrites only the keys present
            // (so absent keys keep the component's member-initializer defaults,
            // matching the previous per-field `.as<T>(default)` behaviour).
            if (const YAML::Node n = node["RigidBody"])
                DeserializeComponent(n, Reflection::Get<RigidBodyComponent>(),
                    &entity.AddComponent<RigidBodyComponent>());

            if (const YAML::Node n = node["BoxCollider"])
                DeserializeComponent(n, Reflection::Get<BoxColliderComponent>(),
                    &entity.AddComponent<BoxColliderComponent>());

            if (const YAML::Node n = node["SphereCollider"])
                DeserializeComponent(n, Reflection::Get<SphereColliderComponent>(),
                    &entity.AddComponent<SphereColliderComponent>());

            if (const YAML::Node n = node["CapsuleCollider"])
                DeserializeComponent(n, Reflection::Get<CapsuleColliderComponent>(),
                    &entity.AddComponent<CapsuleColliderComponent>());

            if (const YAML::Node n = node["Script"])
                DeserializeScript(n, entity.AddComponent<ScriptComponent>());

            // Relationship already exists (CreateEntityWithUUID auto-adds it), so
            // populate the existing instance rather than adding a new one.
            if (const YAML::Node n = node["Relationship"])
                DeserializeComponent(n, Reflection::Get<RelationshipComponent>(),
                    &entity.GetComponent<RelationshipComponent>());
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
