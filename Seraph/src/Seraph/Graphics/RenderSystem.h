//
// Project-wide graphics/quality settings, registered with the engine Settings
// system so they surface in the Settings panel and are settable from the console
// (the console bridges to Settings::Find/SetAny). Mirrors PhysicsSystem's
// settings ownership.
//
// This is the first slice of RenderingBoard "Render 37 — ProjectGraphicsSettings":
// it currently carries the HDR tonemap resolve knobs. Render 37 expands the same
// struct + RegisterSettings() with renderer path, HDR/MSAA/AA, shadows, VSync,
// upscaling, and quality tiers. Per-scene look overrides (exposure/tonemap as an
// artist-authored mood) later move to SceneEnvironmentSettings ("Render 34").
//

#pragma once

#include "Seraph/Core/Base.h"
#include "Seraph/Reflection/Annotations.h"

namespace Seraph
{

// Tone-mapping operator applied by the HDR resolve pass (fs_tonemap). Values must
// match the operator ids the shader switches on.
enum class SENUM() TonemapOperator : u8
{
    None     = 0,
    Reinhard = 1,
    ACES     = 2,
};

struct ProjectGraphicsSettings
{
    TonemapOperator Tonemap  = TonemapOperator::ACES;
    f32             Exposure = 1.0f; // linear multiplier applied before tone mapping
};

class RenderSystem
{
public:
    // The process-global graphics settings. The Settings system binds directly to
    // these fields, so this always reflects the current (panel/console) values.
    static ProjectGraphicsSettings& GetSettings();

    // Register the graphics settings with the Settings system (Project scope,
    // "Graphics" section). Call once at engine init, alongside the other
    // RegisterSettings hooks.
    static void RegisterSettings();
};

} // namespace Seraph
