//
// An image-based-lighting environment: a pair of pre-baked cube maps authored
// offline (e.g. with cmft) — a roughness-prefiltered radiance cube with a mip
// chain (specular ambient + skybox source) and a cosine-convolved irradiance
// cube (diffuse ambient). The cubes are ordinary Texture2D assets referenced by
// handle, so they load, dedupe, and pack through the normal asset pipeline; this
// asset just bundles them as one "environment" unit for the scene + renderer.
//
// Bake workflow (cmft), matching bgfx's 18-ibl:
//   radiance cube (_lod):  cmft --filter radiance --dstFaceSize 256 --mipCount 7
//     --excludeBase true --glossScale 10 --glossBias 2 --edgeFixup warp
//     --output0 <env>_lod --output0params dds,rgba16f,cubemap
//   irradiance cube (_irr): cmft --filter irradiance --dstFaceSize 64
//     --output0 <env>_irr --output0params dds,rgba16f,cubemap
//

#pragma once

#include "Seraph/Asset/Asset.h"
#include "Seraph/Asset/AssetHandle.h"
#include "Seraph/Core/Base.h"

#include <bgfx/bgfx.h>

#include <vector>

namespace Seraph
{

class EnvironmentMap : public Asset
{
public:
    ASSET_CLASS_TYPE(Environment)

    // Prefiltered radiance cube (mipped): specular IBL + the skybox background.
    AssetHandle Radiance = c_NullAssetHandle;
    // Cosine-convolved irradiance cube: diffuse IBL ambient.
    AssetHandle Irradiance = c_NullAssetHandle;

    // Resolved bgfx cube handles, via the AssetManager. Invalid until the
    // referenced textures finish loading (the renderer treats that as "no IBL
    // yet" rather than an error). Main-thread render use.
    [[nodiscard]] bgfx::TextureHandle RadianceCube() const;
    [[nodiscard]] bgfx::TextureHandle IrradianceCube() const;

    // Mip count of the radiance cube, for roughness -> LOD scaling in the shader
    // (textureCubeLod at roughness * (mips - 1)). 1 until the cube resolves.
    [[nodiscard]] u16 RadianceMipCount() const;

    // Both cubes are resolved and GPU-valid.
    [[nodiscard]] bool IsReady() const;

    [[nodiscard]] std::vector<AssetHandle> GetDependencies() const override
    {
        std::vector<AssetHandle> deps;
        if (Radiance != c_NullAssetHandle)
            deps.push_back(Radiance);
        if (Irradiance != c_NullAssetHandle)
            deps.push_back(Irradiance);
        return deps;
    }
};

} // namespace Seraph
