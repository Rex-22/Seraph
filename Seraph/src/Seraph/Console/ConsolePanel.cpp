#include "Seraph/Console/ConsolePanel.h"

#include "Seraph/Console/Console.h"
#include "Seraph/Console/ConsoleEvaluator.h"
#include "Seraph/Console/ConsoleSink.h"

#include <spdlog/common.h> // spdlog::level

#include <imgui.h>

#include <algorithm>

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

    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(4, 1));
    for (const ConsoleLine& line : m_Cached)
    {
        ImGui::PushStyleColor(ImGuiCol_Text, LevelColor(line.Level));
        ImGui::TextUnformatted(line.Text.c_str());
        ImGui::PopStyleColor();
    }
    ImGui::PopStyleVar();

    if (m_ScrollToBottom
        || (ImGui::GetScrollY() >= ImGui::GetScrollMaxY() - 1.0f))
        ImGui::SetScrollHereY(1.0f);
    m_ScrollToBottom = false;

    ImGui::EndChild();
}

void ConsolePanel::Submit()
{
    if (m_Input[0] != '\0')
    {
        Console::Execute(m_Input);
        m_Input[0] = '\0';
    }
    m_HistoryPos = -1;
    m_TabIndex = -1;
    m_TabMatches.clear();
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
                                 "command or variable  (Tab cycles completions)",
                                 m_Input, sizeof(m_Input), flags,
                                 &ConsolePanel::InputTextCallback, this))
        Submit();

    // Keep the caret in the input: on open and after every submit.
    if (m_JustOpened || m_ReclaimFocus)
    {
        ImGui::SetKeyboardFocusHere(-1);
        m_JustOpened = false;
        m_ReclaimFocus = false;
    }
}

void ConsolePanel::OnImGuiRender()
{
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

        DrawOutput();
        ImGui::Separator();
        DrawInput();
    }
    ImGui::End();
    ImGui::PopStyleVar();
}

int ConsolePanel::InputTextCallback(ImGuiInputTextCallbackData* data)
{
    auto* self = static_cast<ConsolePanel*>(data->UserData);

    switch (data->EventFlag)
    {
        case ImGuiInputTextFlags_CallbackCompletion:
        {
            const std::string cur(data->Buf, data->BufTextLen);
            const bool cycling = self->m_TabIndex >= 0
                                 && !self->m_TabMatches.empty()
                                 && cur == self->m_TabLastWrite;
            if (cycling)
            {
                // Advance to the next match (wraps).
                self->m_TabIndex = (self->m_TabIndex + 1)
                                   % static_cast<int>(self->m_TabMatches.size());
            }
            else
            {
                // Fresh completion: rank matches for the current text.
                std::vector<ConsoleEvaluator::Suggestion> sugg =
                    ConsoleEvaluator::Autocomplete(cur, 64);
                self->m_TabMatches.clear();
                for (const ConsoleEvaluator::Suggestion& s : sugg)
                    self->m_TabMatches.push_back(s.Name);
                if (self->m_TabMatches.empty())
                {
                    self->m_TabIndex = -1;
                    break;
                }
                self->m_TabIndex = 0;
            }

            const std::string& pick = self->m_TabMatches[self->m_TabIndex];
            data->DeleteChars(0, data->BufTextLen);
            data->InsertChars(0, pick.c_str());
            self->m_TabLastWrite = pick;
            break;
        }
        case ImGuiInputTextFlags_CallbackHistory:
        {
            self->m_TabIndex = -1; // navigating history breaks any tab cycle
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
