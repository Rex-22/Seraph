//
// Canonical bgfx view ids for the engine's passes. bgfx submits views in
// ascending id order, so a pass that samples another pass's output must have a
// higher id. Centralized here so the editor, runtime, picker, and ImGui layers
// agree on one numbering instead of each defining its own constants.
//

#pragma once

#include "Seraph/Core/Base.h"

namespace Seraph::ViewId
{

constexpr u16 Backbuffer = 0;   // backbuffer clear only; no scene geometry

// Shadow depth passes render BEFORE the scene samples them, so their ids are
// lower than Scene. Reserve a contiguous range for directional cascades (Render
// 21) so adding CSM needs no renumber.
constexpr u16 Shadow            = 1;   // directional shadow map (cascade 0)
constexpr u16 ShadowCascadeMax  = 4;   // cascades occupy Shadow .. Shadow+3

constexpr u16 Scene      = 5;   // scene geometry (the HDR target in the HDR pipeline)
constexpr u16 Pick       = 6;   // editor entity color-id render (EntityPicker)
constexpr u16 PickBlit   = 7;   // editor entity color-id readback blit
constexpr u16 Tonemap    = 8;   // fullscreen HDR -> LDR resolve
constexpr u16 EnvBake    = 9;   // one-time environment/IBL bakes (BRDF LUT, ...)
constexpr u16 ImGui      = 255; // Dear ImGui overlay

} // namespace Seraph::ViewId
