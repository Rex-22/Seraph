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
}

} // namespace Seraph
