//
// Per-scene environment / background settings. The minimal seed of the larger
// "Render 34 — SceneEnvironmentSettings": it holds the image-based-lighting
// source (an EnvironmentMap asset) plus how it is used as the background and how
// strongly it lights the scene. Serialized at the scene level.
//

#pragma once

#include "Seraph/Asset/AssetHandle.h"
#include "Seraph/Core/Base.h"

namespace Seraph
{

// What fills pixels no geometry covers.
enum class SceneBackgroundMode : u8
{
    SolidColor = 0, // the renderer's flat clear color
    Skybox     = 1, // the environment radiance cube
};

struct SceneEnvironment
{
    // EnvironmentMap asset (radiance + irradiance cubes). Null = no IBL / no sky.
    AssetHandle Environment = c_NullAssetHandle;

    SceneBackgroundMode Background = SceneBackgroundMode::SolidColor;

    // Multiplier on both the skybox and the image-based ambient (diffuse+specular).
    f32 Intensity = 1.0f;

    // Yaw rotation of the environment about +Y, in radians.
    f32 Rotation = 0.0f;
};

} // namespace Seraph
