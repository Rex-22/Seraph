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
constexpr u16 Scene      = 1;   // scene geometry (the HDR target in the HDR pipeline)
constexpr u16 Pick       = 2;   // editor entity color-id render (EntityPicker)
constexpr u16 PickBlit   = 3;   // editor entity color-id readback blit
constexpr u16 Tonemap    = 4;   // fullscreen HDR -> LDR resolve
constexpr u16 EnvBake    = 8;   // one-time environment/IBL bakes (BRDF LUT, ...)
constexpr u16 ImGui      = 255; // Dear ImGui overlay

} // namespace Seraph::ViewId
