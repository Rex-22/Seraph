//
// Created for the HDR rendering pipeline (RenderingBoard "Render 4"); first slice
// of "Render 37 — ProjectGraphicsSettings".
//

#include "RenderSystem.h"

#include "Seraph/Settings/Settings.h"

namespace Seraph
{

ProjectGraphicsSettings& RenderSystem::GetSettings()
{
    static ProjectGraphicsSettings s;
    return s;
}

void RenderSystem::RegisterSettings()
{
    ProjectGraphicsSettings& s = GetSettings();

    Settings::Register("engine.graphics.tonemap")
        .Bind(&s.Tonemap).Scope(SettingScope::Project)
        .Section("Graphics").Display("Tone Mapping")
        .Tooltip("Operator used to resolve the HDR scene to the display");

    Settings::Register("engine.graphics.exposure")
        .Bind(&s.Exposure).Scope(SettingScope::Project)
        .Section("Graphics").Display("Exposure")
        .Tooltip("Linear exposure multiplier applied before tone mapping")
        .Min(0.0f).Max(16.0f);

    Settings::Register("engine.graphics.shadowBias")
        .Bind(&s.ShadowBias).Scope(SettingScope::Project)
        .Section("Graphics").Display("Shadow Bias")
        .Tooltip("Sun shadow depth bias in world units (slope-scaled)")
        .Min(0.0f).Max(0.5f);

    Settings::Register("engine.graphics.shadowNormalOffset")
        .Bind(&s.ShadowNormalOffset).Scope(SettingScope::Project)
        .Section("Graphics").Display("Shadow Normal Offset")
        .Tooltip("Extra offset along the surface normal (world units) to reduce shadow acne")
        .Min(0.0f).Max(1.0f);
}

} // namespace Seraph
