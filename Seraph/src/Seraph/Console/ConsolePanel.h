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

    // Fill the input with a suggestion the user clicked (mouse path; the keyboard
    // path fills through the InputText callback instead).
    void FillFromMouse(int suggestionIndex);
    // Output line selection (click / shift-click / drag) + clipboard copy.
    [[nodiscard]] bool InSelection(int lineIndex) const;
    void CopySelection() const;

    // ImGui InputText callback (history + completion). UserData is `this`.
    static int InputTextCallback(ImGuiInputTextCallbackData* data);
    // Replace the current token with `pick` and record it as auto-filled.
    static void FillSuggestion(ImGuiInputTextCallbackData* data, ConsolePanel* self,
                               std::size_t replaceFrom, const std::string& pick);

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

    // Autocomplete state. The dropdown (a separate window below the input, so it
    // isn't clipped by the overlay) lists matches for the current token; Up/Down
    // (or Tab) move m_SuggestSelected and fill that suggestion into the input.
    // m_LastFilled is the buffer text we last auto-filled, used to tell a fill
    // apart from the user typing (which resets the selection).
    bool m_ShowSuggest = false;
    // Was the dropdown hovered/focused last frame? Keeps it open across a mouse
    // press-then-release even though the press deactivates the input field —
    // otherwise the popover closes before the click (release) registers.
    bool m_SuggestHovered = false;
    float m_SuggestX = 0.0f, m_SuggestY = 0.0f, m_SuggestW = 0.0f;
    std::vector<std::string> m_SuggestItems;
    int m_SuggestSelected = -1;
    std::size_t m_SuggestReplaceFrom = 0;
    std::string m_LastFilled;
    // A mouse-clicked suggestion is applied on the next frame AFTER the InputText
    // call — by then ImGui's deactivation write-back has already run, so writing
    // m_Input here isn't clobbered. Empty = nothing pending.
    bool m_HasPendingFill = false;
    std::string m_PendingFill;

    // Output selection: a line range [min(anchor,end), max(...)]; -1 = nothing.
    int m_SelAnchor = -1;
    int m_SelEnd = -1;
};

} // namespace Seraph
