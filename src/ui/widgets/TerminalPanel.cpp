// SPDX-License-Identifier: MIT
#include "ui/widgets/TerminalPanel.hpp"

#include "ui/UiTheme.hpp"

#include <imgui.h>

namespace TerminalPanel {

bool begin(const char* title, ImVec2 size, ImGuiWindowFlags flags)
{
    ImGui::PushStyleColor(ImGuiCol_ChildBg, UiTheme::PanelBackground);
    const bool open = ImGui::BeginChild(title, size, true, flags);
    ImGui::PushStyleColor(ImGuiCol_ChildBg, UiTheme::PanelHeader);
    ImGui::BeginChild("Header", ImVec2(0.0f, 26.0f), false, ImGuiWindowFlags_NoScrollbar);
    ImGui::TextColored(UiTheme::Accent, "%s", title);
    ImGui::EndChild();
    ImGui::PopStyleColor();
    return open;
}

void end()
{
    ImGui::EndChild();
    ImGui::PopStyleColor();
}

void metricCell(const char* label, const char* value, ImVec4 accent)
{
    ImGui::TextColored(UiTheme::TextSecondary, "%s", label);
    ImGui::TextColored(accent, "%s", value);
}

}
