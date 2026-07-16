#include "Seraph/Editor/Panels/SettingsPanel.h"

#include "Seraph/Editor/PropertyDrawer.h"
#include "Seraph/Settings/Settings.h"

#include <imgui.h>

#include <algorithm>
#include <cctype>
#include <map>
#include <string>
#include <vector>

namespace Seraph
{

namespace
{
std::string Lower(std::string s)
{
    std::transform(s.begin(), s.end(), s.begin(),
                   [](unsigned char c) { return std::tolower(c); });
    return s;
}

const char* ScopeName(SettingScope s)
{
    switch (s)
    {
        case SettingScope::Engine:  return "Engine";
        case SettingScope::Project: return "Project";
        case SettingScope::User:    return "User";
    }
    return "?";
}
} // namespace

void SettingsPanel::OnImGuiRender()
{
    if (!m_Open)
        return;
    if (!ImGui::Begin("Settings", &m_Open))
    {
        ImGui::End();
        return;
    }

    if (Settings::IsRestartPending())
        ImGui::TextColored(ImVec4(1.0f, 0.6f, 0.2f, 1.0f),
                           "Some changes take effect after restart.");

    ImGui::InputTextWithHint("##settings-filter", "Search...", m_Filter,
                             sizeof(m_Filter));
    ImGui::Separator();

    // Group registered settings by Section (default "General").
    std::map<std::string, std::vector<const SettingDescriptor*>> sections;
    for (const SettingDescriptor* d : Settings::All())
    {
        if (d->HasFlag(SettingFlag_Hidden))
            continue;
        const std::string* sec = d->Attrs.Get<std::string>(Setting::Attr::Section);
        sections[sec ? *sec : std::string("General")].push_back(d);
    }

    const std::string filter = Lower(m_Filter);

    for (auto& [section, items] : sections)
    {
        if (!ImGui::CollapsingHeader(section.c_str(),
                                     ImGuiTreeNodeFlags_DefaultOpen))
            continue;

        for (const SettingDescriptor* d : items)
        {
            const std::string* disp =
                d->Attrs.Get<std::string>(Setting::Attr::DisplayName);
            const std::string label = disp ? *disp : d->Key;

            if (!filter.empty() && Lower(label).find(filter) == std::string::npos
                && Lower(d->Key).find(filter) == std::string::npos)
                continue;

            ImGui::PushID(d->Key.c_str());

            Any value = d->Read();
            const bool readOnly = d->HasFlag(SettingFlag_ReadOnly);
            if (readOnly)
                ImGui::BeginDisabled();

            const std::string widgetLabel = label + "##w";
            if (PropertyDrawer::DrawValue(widgetLabel.c_str(), value,
                                          d->ValueType, d->Attrs))
                Settings::SetAny(d->Key, value);

            if (readOnly)
                ImGui::EndDisabled();

            if (!d->DefaultValue.IsEmpty())
            {
                ImGui::SameLine();
                if (ImGui::SmallButton("Reset"))
                    Settings::ResetToDefault(d->Key);
            }

            ImGui::SameLine();
            ImGui::TextDisabled("[%s%s]", ScopeName(d->Scope),
                                d->HasFlag(SettingFlag_RequiresRestart)
                                    ? " · restart"
                                    : "");

            ImGui::PopID();
        }
    }

    ImGui::Separator();
    if (ImGui::Button("Save"))
        Settings::SaveDirty();

    ImGui::End();
}

} // namespace Seraph
