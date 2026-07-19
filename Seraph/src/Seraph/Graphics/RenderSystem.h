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

    // Directional-shadow anti-acne controls (Render 18). Bias is a constant depth
    // offset in shadow-clip space; NormalOffset pushes the sampled point along the
    // surface normal (world units, slope-scaled by 1-NoL). Kept small: the shadow
    // pass renders back faces only (front-face cull), so self-shadow acne is
    // already largely gone — large values here just reintroduce contact gaps.
    f32 ShadowBias         = 0.0008f;
    f32 ShadowNormalOffset = 0.01f;
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
