//
// ConsolePanel — the ImGui developer-console widget. Engine-level (not editor-
// only) so both the editor and the shipped runtime can host it. Renders the
// captured log output (ConsoleSink ring buffer, colored by level), an input line
// with command history (Up/Down) and fuzzy autocomplete (Tab), and a live
// suggestion list. On submit it hands the line to Console::Execute.
//
// Usage: hold one as a member, call OnImGuiRender() each frame, and toggle m_Open
// (OpenFlag()/Toggle()) from a hotkey.
//

#pragma once

#include "Seraph/Console/ConsoleSink.h" // ConsoleLine

#include <string>
#include <vector>

struct ImGuiInputTextCallbackData; // avoid dragging imgui.h into consumers

namespace Seraph
{

class ConsolePanel
{
public:
    ConsolePanel();

    void OnImGuiRender();

    [[nodiscard]] bool IsOpen() const { return m_Open; }
    void Open()
    {
        if (!m_Open)
            m_JustOpened = true;
        m_Open = true;
    }
    void Close() { m_Open = false; }
    void Toggle()
    {
        m_Open = !m_Open;
        if (m_Open)
            m_JustOpened = true;
    }
    bool* OpenFlag() { return &m_Open; }

    // Constrain the drop-down to a screen-space rect (the editor's viewport panel).
    // Pass a zero width/height to fall back to the full main viewport (runtime,
    // where the whole window IS the viewport).
    void SetViewportRect(float x, float y, float w, float h)
    {
        m_RectX = x;
        m_RectY = y;
        m_RectW = w;
        m_RectH = h;
    }

private:
    void DrawOutput();
    void DrawInput();
    void Submit();
    void RefreshCache();

    // ImGui InputText callback (history + completion). UserData is `this`.
    static int InputTextCallback(ImGuiInputTextCallbackData* data);

    bool m_Open = false;
    bool m_JustOpened = false; // focus the input the frame the console opens
    char m_Input[512] = {};

    std::vector<ConsoleLine> m_Cached;
    u64 m_CachedVersion = static_cast<u64>(-1); // force an initial read
    bool m_ScrollToBottom = false;
    bool m_ReclaimFocus = false;

    int m_HistoryPos = -1; // -1 = editing a fresh line; else index into history

    // Screen-space rect the overlay is pinned to (the editor viewport). Zero
    // width/height means "use the whole main viewport".
    float m_RectX = 0.0f, m_RectY = 0.0f, m_RectW = 0.0f, m_RectH = 0.0f;

    // Live autocomplete dropdown, drawn as a separate window just below the input
    // (so it isn't clipped by the overlay's bottom edge).
    bool m_ShowSuggest = false;
    float m_SuggestX = 0.0f, m_SuggestY = 0.0f, m_SuggestW = 0.0f;
    std::vector<std::string> m_SuggestItems;

    // Tab-completion cycle state. Repeated Tab (without editing) advances through
    // matches; m_TabLastWrite is the text we last wrote, used to detect edits.
    // m_TabReplaceFrom is where in the buffer the completed token begins.
    std::vector<std::string> m_TabMatches;
    int m_TabIndex = -1;
    std::size_t m_TabReplaceFrom = 0;
    std::string m_TabLastWrite;
};

} // namespace Seraph
