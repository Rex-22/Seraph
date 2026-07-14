# Gameplay Scripting — Overview

## The one-liner

We're giving Seraph scenes **behaviour**. Today a scene is furniture — entities,
meshes, colliders, cameras — but nothing *does* anything except physics. We're
adding a **scripting layer** so an entity can have gameplay logic attached:
a player controller, a door that opens, a pickup that despawns on touch.

## Why now

Scenes are already fully data-driven: we author them in the editor, save them to
`.sscene` files, cook them into asset packs, and load them in the runtime. The
one thing missing is the piece that turns a static scene into a *game* — code
that runs each frame and reacts to input and collisions. Our own architecture
notes have flagged this gap for a while (the "C++-scene tension"): the seams were
deliberately left open (an unused physics contact callback, a reserved
`"Scripting"` log tag) waiting for exactly this feature.

## The approach: native C++ (Unreal-style), not a scripting VM

There are two well-trodden ways to add gameplay logic to an engine:

- **An embedded scripting language** (Lua, or C# like Hazel/Unity). Great for
  hot-reload and non-programmer authoring, but it means adopting a whole VM,
  hand-binding every engine API across a language boundary, and paying a runtime
  cost — months of plumbing before it matches what C++ already gives us.
- **Native C++ classes** (like Unreal's Actors). You write a C++ class, attach it
  to an entity, and it gets `OnCreate` / `OnUpdate(dt)` / collision callbacks.

**We're going native C++.** It needs zero new dependencies, gives scripts full,
type-safe access to the whole engine at native speed, and fits how Seraph is
already built (statically linked, Hazel-derived). It's the fastest path to
actually shipping gameplay. We're keeping the door open to add a scripting VM
later — a script is identified by a **name string**, so a Lua/C# backend could
slot in behind the same system down the line — but that's explicitly not now.

## What it looks like

You write a class:

```cpp
class PlayerController : public Seraph::ScriptableEntity
{
    void OnUpdate(f32 dt) override
    {
        if (Seraph::Input::IsKeyDown(Key::W))
            Transform().Translation += Forward() * m_Speed * dt;
    }

    void OnCollisionBegin(Entity other) override
    {
        SP_INFO("bumped into {}", other.GetUUID());
    }

    float m_Speed = 5.0f;
};
SP_REGISTER_SCRIPT(PlayerController, "PlayerController");
```

Then in the editor you select an entity, **Add Component ▸ Script**, and pick
`PlayerController` from a dropdown. Press play — it runs. Save the scene and the
binding persists. That's the whole authoring loop.

## How it fits together

- **`ScriptableEntity`** — the base class your gameplay code derives from. Gives
  you the entity, its components, input, and physics.
- **`ScriptComponent`** — attaches a script to an entity by name.
- **`ScriptEngine`** — per-scene; creates the script instances when play starts,
  ticks them each frame (before physics, so a script can push a body and have it
  move the same frame), routes collision events to them, and cleans them up on
  stop. Works identically in the standalone runtime and in play-in-editor.
- **The `Game` module** — each project gets a small C++ target holding its
  scripts, linked into the editor and runtime. This is the Unreal model: engine +
  your game's code. Creating a new project from the launcher scaffolds this
  module for you so there's a working example script on day one.

## The one tradeoff to know

Because scripts are compiled C++, **changing gameplay logic means a rebuild** —
the editor can't hot-reload native code (exactly like Unreal recompiling your
game module). Editing a *scene* is instant as always; editing a *script* is a
compile. That's the deliberate price for native speed and zero new dependencies,
and it's the thing to keep in mind while we build.

## Where we're going

The work is broken into 8 tasks on the `ScriptingBoard`, built bottom-up: the
core types → the per-scene driver → wiring it into the scene lifecycle →
serialization → the project `Game` module + a demo script → editor authoring →
new-project scaffolding → end-to-end verification. Each task links back to
[`Todo/scripting-plan.md`](../Todo/scripting-plan.md), which also walks through
the handful of genuinely tricky bits (linker gotchas, ownership, tick ordering)
with worked examples.
