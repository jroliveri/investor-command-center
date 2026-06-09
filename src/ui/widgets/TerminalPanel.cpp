// SPDX-License-Identifier: MIT
#include "ui/widgets/TerminalPanel.hpp"

#include "ui/UiTheme.hpp"

#include <imgui.h>

namespace TerminalPanel {

bool begin(const char* title, ImVec2 size, ImGuiWindowFlags flags)
{
    UiTheme::pushPanelStyle();
    const bool open = ImGui::BeginChild(title, size, true, flags);

    const ImVec2 panelMin = ImGui::GetWindowPos();
    const ImVec2 panelMax(panelMin.x + ImGui::GetWindowSize().x, panelMin.y + ImGui::GetWindowSize().y);
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    drawList->AddRectFilled(panelMin, ImVec2(panelMax.x, panelMin.y + 34.0f), ImGui::GetColorU32(UiTheme::withAlpha(UiTheme::SurfaceElevated, 0.15f)), 16.0f, ImDrawFlags_RoundCornersTop);
    drawList->AddRectFilled(ImVec2(panelMin.x + 14.0f, panelMin.y + 9.0f), ImVec2(panelMin.x + 60.0f, panelMin.y + 11.0f), ImGui::GetColorU32(UiTheme::withAlpha(UiTheme::ElectricCyan, 0.30f)), 2.0f);
    drawList->AddRect(panelMin, panelMax, ImGui::GetColorU32(UiTheme::withAlpha(UiTheme::BorderSubtle, 0.08f)), 16.0f);

    ImGui::PushStyleColor(ImGuiCol_ChildBg, UiTheme::withAlpha(UiTheme::SurfaceMain, 0.0f));
    ImGui::BeginChild("Header", ImVec2(0.0f, 30.0f), false, ImGuiWindowFlags_NoScrollbar);
    ImGui::TextColored(UiTheme::TextSecondary, "%s", title);
    ImGui::EndChild();
    ImGui::PopStyleColor();
    ImGui::Spacing();
    return open;
}

void end()
{
    ImGui::EndChild();
    UiTheme::popPanelStyle();
}

void metricCell(const char* label, const char* value, ImVec4 accent)
{
    ImGui::TextColored(UiTheme::TextSecondary, "%s", label);
    UiTheme::pushNumericFont();
    ImGui::TextColored(accent, "%s", value);
    UiTheme::popNumericFont();
}

}
