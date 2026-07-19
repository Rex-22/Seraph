#include "Seraph/Console/ConsolePanel.h"

#include "Seraph/Console/Console.h"
#include "Seraph/Console/ConsoleEvaluator.h"
#include "Seraph/Console/ConsoleSink.h"
#include "Seraph/Core/Input.h"

#include <spdlog/common.h> // spdlog::level

#include <imgui.h>

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <string>

namespace Seraph
{

namespace
{

// Fraction of the viewport height the drop-down console covers, and a hard cap.
constexpr float k_HeightFraction = 0.42f;
constexpr float k_MaxHeight = 520.0f;

ImVec4 LevelColor(int level)
{
    switch (static_cast<spdlog::level::level_enum>(level))
    {
        case spdlog::level::trace:
        case spdlog::level::debug:
            return ImVec4(0.55f, 0.55f, 0.55f, 1.0f); // gray
        case spdlog::level::warn:
            return ImVec4(1.0f, 0.78f, 0.28f, 1.0f); // amber
        case spdlog::level::err:
        case spdlog::level::critical:
            return ImVec4(1.0f, 0.4f, 0.35f, 1.0f); // red
        default:
            return ImVec4(0.86f, 0.86f, 0.86f, 1.0f); // info/default
    }
}

} // namespace

ConsolePanel::ConsolePanel()
{
    // Route the `clear` built-in / Console::ClearOutput() to the shared buffer;
    // the version bump makes RefreshCache pick up the empty buffer next frame.
    Console::SetClearHandler([] { ConsoleOutput::Clear(); });
}

void ConsolePanel::RefreshCache()
{
    const u64 v = ConsoleOutput::Version();
    if (v == m_CachedVersion)
        return;
    ConsoleOutput::Read(m_Cached);
    m_CachedVersion = v;
    m_ScrollToBottom = true; // new output arrived — follow the tail
    m_SelAnchor = m_SelEnd = -1; // line indices shifted; drop any stale selection
}

void ConsolePanel::DrawOutput()
{
    const float footer = ImGui::GetStyle().ItemSpacing.y
                         + ImGui::GetFrameHeightWithSpacing();
    if (!ImGui::BeginChild("##console-output", ImVec2(0, -footer), false,
                           ImGuiWindowFlags_HorizontalScrollbar))
    {
        ImGui::EndChild();
        return;
    }

    // Each line is a Selectable so it can be clicked/dragged to select and copied
    // (Ctrl+C or the toolbar Copy button); the level color is kept on the label.
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(4, 1));
    for (int i = 0; i < static_cast<int>(m_Cached.size()); ++i)
    {
        const ConsoleLine& line = m_Cached[i];
        ImGui::PushStyleColor(ImGuiCol_Text, LevelColor(line.Level));
        ImGui::PushID(i);
        ImGui::Selectable(line.Text.c_str(), InSelection(i));
        if (ImGui::IsItemHovered())
        {
            if (ImGui::IsMouseClicked(ImGuiMouseButton_Left))
            {
                if (ImGui::GetIO().KeyShift && m_SelAnchor >= 0)
                    m_SelEnd = i; // shift-click extends the range
                else
                    m_SelAnchor = m_SelEnd = i;
            }
            else if (m_SelAnchor >= 0
                     && ImGui::IsMouseDragging(ImGuiMouseButton_Left))
                m_SelEnd = i; // drag extends the range
        }
        ImGui::PopID();
        ImGui::PopStyleColor();
    }
    ImGui::PopStyleVar();

    // Ctrl+C copies the selection (or everything if nothing is selected).
    if (ImGui::IsWindowFocused() && ImGui::GetIO().KeyCtrl
        && ImGui::IsKeyPressed(ImGuiKey_C, false))
        CopySelection();

    if (m_ScrollToBottom
        || (ImGui::GetScrollY() >= ImGui::GetScrollMaxY() - 1.0f))
        ImGui::SetScrollHereY(1.0f);
    m_ScrollToBottom = false;

    ImGui::EndChild();
}

bool ConsolePanel::InSelection(int lineIndex) const
{
    if (m_SelAnchor < 0 || m_SelEnd < 0)
        return false;
    const int lo = std::min(m_SelAnchor, m_SelEnd);
    const int hi = std::max(m_SelAnchor, m_SelEnd);
    return lineIndex >= lo && lineIndex <= hi;
}

void ConsolePanel::CopySelection() const
{
    std::string out;
    const bool hasSel = m_SelAnchor >= 0 && m_SelEnd >= 0;
    const int lo = hasSel ? std::min(m_SelAnchor, m_SelEnd) : 0;
    const int hi = hasSel ? std::max(m_SelAnchor, m_SelEnd)
                          : static_cast<int>(m_Cached.size()) - 1;
    for (int i = lo; i <= hi && i < static_cast<int>(m_Cached.size()); ++i)
    {
        if (!out.empty())
            out += '\n';
        out += m_Cached[i].Text;
    }
    if (!out.empty())
        ImGui::SetClipboardText(out.c_str());
}

void ConsolePanel::Submit()
{
    if (m_Input[0] != '\0')
    {
        Console::Execute(m_Input);
        m_Input[0] = '\0';
    }
    m_HistoryPos = -1;
    m_SuggestSelected = -1;
    m_SuggestItems.clear();
    m_ShowSuggest = false;
    m_LastFilled.clear();
    m_ScrollToBottom = true;
    m_ReclaimFocus = true;
}

void ConsolePanel::DrawInput()
{
    ImGui::TextUnformatted("]");
    ImGui::SameLine();

    ImGui::SetNextItemWidth(-FLT_MIN);
    const ImGuiInputTextFlags flags =
        ImGuiInputTextFlags_EnterReturnsTrue
        | ImGuiInputTextFlags_CallbackCompletion
        | ImGuiInputTextFlags_CallbackHistory;

    if (ImGui::InputTextWithHint("##console-input",
                                 "command or variable  (Up/Down or Tab to complete)",
                                 m_Input, sizeof(m_Input), flags,
                                 &ConsolePanel::InputTextCallback, this))
        Submit();

    // Apply a mouse-picked suggestion AFTER the InputText call: by now ImGui's
    // deactivation write-back (which would clobber m_Input with the pre-click text)
    // has already run this frame, so our write sticks. Then reclaim the caret.
    if (m_HasPendingFill)
    {
        std::snprintf(m_Input, sizeof(m_Input), "%s", m_PendingFill.c_str());
        m_LastFilled = m_Input;
        m_HasPendingFill = false;
        m_ReclaimFocus = true;
    }

    // Keep the caret in the input: on open and after every submit / click-fill.
    if (m_JustOpened || m_ReclaimFocus)
    {
        ImGui::SetKeyboardFocusHere(-1);
        m_JustOpened = false;
        m_ReclaimFocus = false;
    }

    // Live suggestions for the current token, drawn as a separate dropdown window
    // (below the input) in OnImGuiRender so the overlay's bottom edge doesn't clip
    // them. Up/Down (or Tab) move the highlight and fill it.
    // Keep the dropdown up while the input is active OR while the user is pressing
    // on the dropdown itself (a press deactivates the input, but we must survive to
    // the release so the click registers).
    m_ShowSuggest = false;
    if (m_Input[0] != '\0' && (ImGui::IsItemActive() || m_SuggestHovered))
    {
        // Recompute the list ONLY when the user actually typed. While navigating,
        // each pick fills the token so the buffer equals our last fill — recomputing
        // then would collapse the list to just that match, so keep the original.
        const bool navigating = (m_LastFilled == m_Input) && !m_SuggestItems.empty();
        if (!navigating)
        {
            ConsoleEvaluator::Completion comp =
                ConsoleEvaluator::Complete(m_Input, 10);
            m_SuggestItems = std::move(comp.Matches);
            m_SuggestReplaceFrom = comp.ReplaceFrom;
            m_SuggestSelected = -1; // fresh input -> no highlight yet
        }

        if (!m_SuggestItems.empty())
        {
            const ImVec2 mn = ImGui::GetItemRectMin();
            const ImVec2 mx = ImGui::GetItemRectMax();
            m_SuggestX = mn.x;
            m_SuggestY = mx.y;
            m_SuggestW = mx.x - mn.x;
            m_ShowSuggest = true;
        }
    }
}

void ConsolePanel::OnImGuiRender()
{
    // The open console owns keyboard + mouse so polling gameplay/camera code (e.g.
    // Player.cpp's Input::IsKeyDown(W) / IsMouseButtonPressed) stops responding —
    // typing goes to the console and clicking the viewport won't grab the cursor.
    Input::SetKeyboardCaptured(m_Open);
    Input::SetMouseCaptured(m_Open);

    if (!m_Open)
        return;

    // Anchor to the editor viewport rect if one was supplied this frame; otherwise
    // (runtime / no rect) fall back to the whole main viewport.
    ImVec2 basePos, baseSize;
    if (m_RectW > 0.0f && m_RectH > 0.0f)
    {
        basePos = ImVec2(m_RectX, m_RectY);
        baseSize = ImVec2(m_RectW, m_RectH);
    }
    else
    {
        const ImGuiViewport* vp = ImGui::GetMainViewport();
        basePos = vp->WorkPos;
        baseSize = vp->WorkSize;
    }
    const float height = std::min(baseSize.y * k_HeightFraction, k_MaxHeight);

    // A top drop-down overlay pinned to the viewport, spanning its width — not a
    // movable/dockable window. Focused on open so it draws over the scene image.
    ImGui::SetNextWindowPos(basePos, ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(baseSize.x, height), ImGuiCond_Always);
    ImGui::SetNextWindowBgAlpha(0.92f);
    if (m_JustOpened)
        ImGui::SetNextWindowFocus();

    const ImGuiWindowFlags flags =
        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize
        | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse
        | ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoSavedSettings
        | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse;

    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    if (ImGui::Begin("##console-overlay", nullptr, flags))
    {
        RefreshCache();

        if (ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows)
            && ImGui::IsKeyPressed(ImGuiKey_Escape))
            m_Open = false;

        // Toolbar: copy the selection (or all), clear the buffer.
        if (ImGui::SmallButton("Copy"))
            CopySelection();
        ImGui::SameLine();
        if (ImGui::SmallButton("Clear"))
            Console::ClearOutput();
        ImGui::Separator();

        DrawOutput();
        ImGui::Separator();
        DrawInput();
    }
    ImGui::End();
    ImGui::PopStyleVar();

    // Autocomplete dropdown: a separate borderless window pinned just under the
    // input field, so it floats over the viewport instead of being clipped by the
    // overlay's bottom edge. Up/Down highlight a row (m_SuggestSelected) and fill
    // it into the input; the highlight is shown here.
    if (m_ShowSuggest)
    {
        ImGui::SetNextWindowPos(ImVec2(m_SuggestX, m_SuggestY));
        ImGui::SetNextWindowSize(ImVec2(m_SuggestW, 0.0f)); // 0 height -> fit rows
        ImGui::SetNextWindowBgAlpha(0.96f);
        const ImGuiWindowFlags sf =
            ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize
            | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings
            | ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav;
        bool hovered = false;
        if (ImGui::Begin("##console-suggest", nullptr, sf))
        {
            for (int i = 0; i < static_cast<int>(m_SuggestItems.size()); ++i)
            {
                ImGui::PushID(i);
                // Click a row to pick it (fills the token + refocuses the input).
                if (ImGui::Selectable(m_SuggestItems[i].c_str(),
                                      i == m_SuggestSelected))
                    FillFromMouse(i);
                ImGui::PopID();
            }
            // Stay open across a press->release even though the press deactivates
            // the input (RootAndChildWindows + AllowWhenBlockedByActiveItem covers
            // the frames while a row is held down).
            hovered = ImGui::IsWindowHovered(
                          ImGuiHoveredFlags_AllowWhenBlockedByActiveItem)
                      || ImGui::IsWindowFocused();
        }
        ImGui::End();
        m_SuggestHovered = hovered;
    }
    else
    {
        m_SuggestHovered = false;
    }
}

// Mouse path for picking a suggestion. We can't safely write m_Input here (ImGui
// clobbers it on the deactivation frame), so build the full command line the pick
// produces and stash it; DrawInput writes it after the InputText call next frame.
void ConsolePanel::FillFromMouse(int suggestionIndex)
{
    if (suggestionIndex < 0
        || suggestionIndex >= static_cast<int>(m_SuggestItems.size()))
        return;
    m_SuggestSelected = suggestionIndex;
    const std::size_t from = std::min(m_SuggestReplaceFrom, std::strlen(m_Input));
    m_PendingFill = std::string(m_Input, from); // keep the prefix before the token
    m_PendingFill += m_SuggestItems[suggestionIndex];
    m_HasPendingFill = true;
}

// Replace the current token (from replaceFrom to the end) with `pick`, and record
// what we wrote so the next-frame recompute keeps the highlight instead of
// treating it as fresh typing.
void ConsolePanel::FillSuggestion(ImGuiInputTextCallbackData* data,
                                  ConsolePanel* self, std::size_t replaceFrom,
                                  const std::string& pick)
{
    int from = static_cast<int>(replaceFrom);
    if (from > data->BufTextLen)
        from = data->BufTextLen;
    data->DeleteChars(from, data->BufTextLen - from);
    data->InsertChars(from, pick.c_str());
    self->m_LastFilled = std::string(data->Buf, data->BufTextLen);
}

int ConsolePanel::InputTextCallback(ImGuiInputTextCallbackData* data)
{
    auto* self = static_cast<ConsolePanel*>(data->UserData);
    const int count = static_cast<int>(self->m_SuggestItems.size());
    const bool haveSuggest = self->m_ShowSuggest && count > 0;

    switch (data->EventFlag)
    {
        case ImGuiInputTextFlags_CallbackCompletion: // Tab
        {
            if (!haveSuggest)
                break;
            self->m_SuggestSelected = (self->m_SuggestSelected + 1) % count;
            FillSuggestion(data, self, self->m_SuggestReplaceFrom,
                           self->m_SuggestItems[self->m_SuggestSelected]);
            break;
        }
        case ImGuiInputTextFlags_CallbackHistory: // Up / Down
        {
            // While the suggestion dropdown is up, arrows move the highlight and
            // fill it; otherwise they walk the command history.
            if (haveSuggest)
            {
                if (data->EventKey == ImGuiKey_UpArrow)
                    self->m_SuggestSelected =
                        (self->m_SuggestSelected <= 0) ? count - 1
                                                       : self->m_SuggestSelected - 1;
                else if (data->EventKey == ImGuiKey_DownArrow)
                    self->m_SuggestSelected = (self->m_SuggestSelected + 1) % count;
                FillSuggestion(data, self, self->m_SuggestReplaceFrom,
                               self->m_SuggestItems[self->m_SuggestSelected]);
                break;
            }

            const std::vector<std::string>& hist = Console::History();
            if (hist.empty())
                break;
            const int prev = self->m_HistoryPos;
            if (data->EventKey == ImGuiKey_UpArrow)
            {
                if (self->m_HistoryPos == -1)
                    self->m_HistoryPos = static_cast<int>(hist.size()) - 1;
                else if (self->m_HistoryPos > 0)
                    --self->m_HistoryPos;
            }
            else if (data->EventKey == ImGuiKey_DownArrow)
            {
                if (self->m_HistoryPos != -1)
                {
                    if (++self->m_HistoryPos >= static_cast<int>(hist.size()))
                        self->m_HistoryPos = -1; // past newest -> fresh line
                }
            }
            if (prev != self->m_HistoryPos)
            {
                const char* line =
                    self->m_HistoryPos == -1 ? "" : hist[self->m_HistoryPos].c_str();
                data->DeleteChars(0, data->BufTextLen);
                data->InsertChars(0, line);
            }
            break;
        }
        default:
            break;
    }
    return 0;
}

} // namespace Seraph
