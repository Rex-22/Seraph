//
// Created by Ruben on 2026/07/19.
//

#include "EnvironmentMap.h"

#include "Seraph/Asset/AssetManager.h"
#include "Seraph/Graphics/Texture2D.h"

namespace Seraph
{

bgfx::TextureHandle EnvironmentMap::RadianceCube() const
{
    if (Ref<Texture2D> t = AssetManager::GetAsset<Texture2D>(Radiance))
        return t->Handle();
    return BGFX_INVALID_HANDLE;
}

bgfx::TextureHandle EnvironmentMap::IrradianceCube() const
{
    if (Ref<Texture2D> t = AssetManager::GetAsset<Texture2D>(Irradiance))
        return t->Handle();
    return BGFX_INVALID_HANDLE;
}

u16 EnvironmentMap::RadianceMipCount() const
{
    if (Ref<Texture2D> t = AssetManager::GetAsset<Texture2D>(Radiance))
        return t->MipCount();
    return 1;
}

bool EnvironmentMap::IsReady() const
{
    return bgfx::isValid(RadianceCube()) && bgfx::isValid(IrradianceCube());
}

} // namespace Seraph
