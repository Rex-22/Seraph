# Plan: scene serialization + project bootstrap

Status: **plan only** — not yet implemented. Depends on the asset system and
`ApplicationSpecification` (both landed).

## Where we are

- Components are already handle-based: `MeshComponent { AssetRef Mesh; }`, and
  `RelationshipComponent` links entities by `UUID`. So the entity data is
  serialization-ready — it holds values and stable ids, not live pointers.
- `ApplicationSpecification` already picks `EditorAssetManager` (loose files) vs
  `RuntimeAssetManager` (pack) at startup.
- **Gap:** scenes are C++ (`ExampleScene::OnLoaded` builds entities in code). A
  shipped runtime can't compile scenes — it must *load* one. And nothing tells the
  runtime *which* scene/pack to start with.

Two pieces close the gap: **scene serialization** (turn a scene into a loadable
asset) and a **project/bootstrap** (tell the runtime what to load and run).

---

## Piece 1 — Scene serialization

### SceneAsset + SceneSerializer

A scene becomes a first-class asset, exactly like textures/meshes:

- `AssetType::Scene`.
- `SceneAsset : Asset` wrapping a populated `Ref<Scene>`.
- `SceneSerializer : AssetSerializer` — **pure CPU, no `Finalize`**
  (`RequiresFinalize()` stays false). It parses bytes into a `Scene` full of
  entities/components; the GPU assets those components reference (meshes,
  textures) load lazily through `AssetManager` at render time, as they do today.

`LoadData` reads scene bytes (YAML in editor, the same bytes from a pack at
runtime — the source-bytes pack model already carries them). `Serialize` walks the
`entt::registry` and emits the entities.

### Component (de)serialization

`entt` has no built-in reflection, and the editor already hand-writes per-component
UI (`EntityInspectorPanel::Draw*Component`). Mirror that: a small **component
serializer registry** (or a hand-written switch in `SceneSerializer`) with one
serialize/deserialize pair per component type. Start hand-written — there are only
six components — and lift to a registry when it grows:

```cpp
// Per entity, keyed by the stable IDComponent UUID:
- IDComponent            -> UUID (the entity's stable id)
- TagComponent           -> string
- TransformComponent     -> Translation (vec3), RotationEuler (vec3), Scale (vec3)
- CameraComponent        -> Type, IsPrimary, projection params
- MeshComponent          -> AssetRef  (serializes as its u64 handle — trivial)
- RelationshipComponent  -> ParentHandle (UUID) + Children (UUID[])
```

Entity identity is the `IDComponent` UUID; `entt::entity` handles are runtime-only.
On load, create entities, populate `Scene::m_EntityIDMap` (UUID → Entity), then
resolve `RelationshipComponent` parent/child UUIDs into the rebuilt entities — same
two-pass approach the scene hierarchy already implies.

### YAML format (editor)

Reuse yaml-cpp (already linked). Example `scenes/example.sscene`:

```yaml
Scene: Example Scene
Entities:
  - Entity: 12847...            # IDComponent UUID
    Tag: Camera
    Transform: { Translation: [0,0,10], Rotation: [0,0,0], Scale: [1,1,1] }
    Camera: { Type: Perspective, IsPrimary: true, Fov: 60, Near: 0.01, Far: 1000 }
  - Entity: 98311...
    Tag: Cube
    Transform: { Translation: [0,0,0], Rotation: [0,0,0], Scale: [1,1,1] }
    MeshComponent: { Mesh: 15782833944256588489 }   # AssetRef handle
```

`MeshComponent.Mesh` is just the handle — the referenced mesh is a separate asset
in the registry/pack, resolved on demand. This is why handle-based components make
scene serialization cheap.

### Editor integration

- `AssetType::Scene` mapped to `.sscene` in `EditorAssetManager::AssetTypeFromExtension`.
- Menu **File ▸ Save Scene / Open Scene** in `EditorLayer` (next to the existing
  Assets ▸ Build Asset Pack), calling `SceneSerializer`/`SaveAsset` and
  `AssetManager::GetAsset<SceneAsset>`.
- The editor's active scene comes from a `SceneAsset` instead of `new ExampleScene`.
- Scenes are file-backed assets, so `AssetPackBuilder` already packs them with no
  change.

---

## Piece 2 — Project + bootstrap

### Project file

A `Project` (matches the reserved `"Project"` log tag) is a small YAML file the
editor authors and the runtime reads:

```yaml
Project: Sandbox
AssetDirectory: assets
StartupScene: 40718...        # SceneAsset handle
Packs: [ assets.pack ]
Window: { Width: 1280, Height: 720, Title: "Sandbox", VSync: false }
```

A `ProjectManager`/`Project` type loads it. The editor uses it to know the asset
root + startup scene; the runtime uses it to know what to boot.

### RuntimeLayer

The shipped game needs a layer without editor UI. `EditorLayer` already contains
the runtime render path (`m_RuntimeMode`: render the scene to the backbuffer,
forward events/update, no panels). Extract that into a lean `RuntimeLayer` that
owns `Ref<Scene>` + `Ref<SceneRenderer>` and does only update + backbuffer render.
`EditorLayer` keeps play-in-editor by reusing the same path.

### Bootstrap flow

```
main (EntryPoint)
  → CreateApplication()                     // client
      reads Project file
      → ApplicationSpecification {          // window + asset mode from the project
            Assets = Runtime, AssetPack = project.Packs[0], Window = project.Window }
  → Application(spec)                        // installs RuntimeAssetManager(pack)
  → client body:                            // manager is up here
      scene = AssetManager::GetAsset<SceneAsset>(project.StartupScene)->GetScene()
      PushLayer(RuntimeLayer(scene, SceneRenderer(scene)))
  → Run()
```

The manager is installed in the ctor (from the spec), and the startup scene loads
in the client body afterward — the same ordering `SandboxApp` already uses. The
editor build is identical up to the layer: it installs `EditorAssetManager` and
pushes `EditorLayer` instead.

---

## The C++-scene tension

`ExampleScene` is a `Scene` subclass with gameplay in `OnUpdate` (camera fly-cam).
A data-loaded scene is a plain `Scene` populated from a `SceneAsset` — it has no
place to put that C++ logic. Near-term resolution:

- Scene **data** (entities/components/hierarchy) is serialized; scene **behavior**
  stays in C++ for now, hosted by the layer or a future script/system component.
- The fly-cam logic moves into the editor camera / a system, not the serialized
  scene.

A full data-driven runtime eventually needs a script or system-component layer;
that's out of scope here. This plan makes scenes *loadable*, not *scriptable*.

---

## Phasing

1. **Scene serialization** — `AssetType::Scene`, `SceneAsset`, `SceneSerializer`,
   per-component (de)serialize, YAML. Editor Save/Open Scene. Verify round-trip:
   author in editor → save → reopen → identical entities.
2. **Project file** — `Project`/`ProjectManager`, YAML load/save, editor opens a
   project (asset dir + startup scene).
3. **Runtime bootstrap** — `RuntimeLayer`, wire Project → `ApplicationSpecification`
   + startup-scene load. Confirm a `Runtime`-mode build boots a packed scene with no
   editor. Scenes pack automatically (file-backed).
4. **Polish** — missing-asset placeholders, scene-load failure handling, and
   (optional) a binary scene format if YAML parse cost matters at load.

Each phase stands alone: Phase 1 is useful in the editor before any runtime exists.

## Open decisions

- **Scene format in the pack** — YAML bytes (reuses everything, human-diffable) vs a
  cooked binary. Recommend YAML-in-pack first; add a binary cook later only if load
  time warrants it.
- **Component registry vs hand-written serializer** — hand-written now (6
  components), registry when it grows or when gameplay/script components arrive.
- **Where the project lives at runtime** — a loose file next to the exe, or baked
  into the pack as a well-known asset. Recommend next-to-exe for the bootstrap
  (chicken-and-egg: you need it before the pack is open), packing it later if
  desired.
