//
// Project/Engine Settings window. Reads ONLY the Settings registry: groups every
// registered setting by its Section attribute, renders each via PropertyDrawer,
// with search, per-setting reset, scope chip, and a restart badge. Registering a
// setting anywhere makes it appear here with no panel change. See settings-plan.md.
//

#pragma once

namespace Seraph
{

class SettingsPanel
{
public:
    void OnImGuiRender();

    bool IsOpen() const { return m_Open; }
    void Open() { m_Open = true; }
    bool* OpenFlag() { return &m_Open; }

private:
    bool m_Open = false;
    char m_Filter[128] = {};
};

} // namespace Seraph
