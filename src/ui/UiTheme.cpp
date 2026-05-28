// SPDX-License-Identifier: MIT
#include "ui/UiTheme.hpp"

#include <string>

namespace UiTheme {

void apply()
{
    ImGuiStyle& style = ImGui::GetStyle();
    style.WindowPadding = ImVec2(18.0f, 18.0f);
    style.FramePadding = ImVec2(11.0f, 8.0f);
    style.CellPadding = ImVec2(12.0f, 9.0f);
    style.ItemSpacing = ImVec2(12.0f, 11.0f);
    style.ItemInnerSpacing = ImVec2(9.0f, 7.0f);
    style.ScrollbarSize = 14.0f;
    style.WindowRounding = 0.0f;
    style.ChildRounding = 8.0f;
    style.FrameRounding = 6.0f;
    style.PopupRounding = 8.0f;
    style.GrabRounding = 6.0f;
    style.TabRounding = 6.0f;
    style.WindowBorderSize = 0.0f;
    style.ChildBorderSize = 1.0f;
    style.FrameBorderSize = 1.0f;

    ImVec4* colors = style.Colors;
    colors[ImGuiCol_Text] = ImVec4(0.90f, 0.93f, 0.91f, 1.0f);
    colors[ImGuiCol_TextDisabled] = MutedText;
    colors[ImGuiCol_WindowBg] = ImVec4(0.055f, 0.060f, 0.060f, 1.0f);
    colors[ImGuiCol_ChildBg] = PanelBackground;
    colors[ImGuiCol_PopupBg] = ImVec4(0.090f, 0.100f, 0.100f, 1.0f);
    colors[ImGuiCol_Border] = ImVec4(0.22f, 0.25f, 0.23f, 1.0f);
    colors[ImGuiCol_FrameBg] = ImVec4(0.135f, 0.150f, 0.145f, 1.0f);
    colors[ImGuiCol_FrameBgHovered] = ImVec4(0.18f, 0.22f, 0.20f, 1.0f);
    colors[ImGuiCol_FrameBgActive] = ImVec4(0.21f, 0.27f, 0.23f, 1.0f);
    colors[ImGuiCol_TitleBg] = ImVec4(0.065f, 0.070f, 0.070f, 1.0f);
    colors[ImGuiCol_TitleBgActive] = ImVec4(0.080f, 0.090f, 0.085f, 1.0f);
    colors[ImGuiCol_MenuBarBg] = ImVec4(0.080f, 0.090f, 0.085f, 1.0f);
    colors[ImGuiCol_ScrollbarBg] = ImVec4(0.075f, 0.080f, 0.080f, 1.0f);
    colors[ImGuiCol_ScrollbarGrab] = ImVec4(0.25f, 0.29f, 0.27f, 1.0f);
    colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.33f, 0.38f, 0.35f, 1.0f);
    colors[ImGuiCol_CheckMark] = Gain;
    colors[ImGuiCol_SliderGrab] = Gain;
    colors[ImGuiCol_SliderGrabActive] = ImVec4(0.50f, 0.88f, 0.64f, 1.0f);
    colors[ImGuiCol_Button] = ImVec4(0.16f, 0.20f, 0.18f, 1.0f);
    colors[ImGuiCol_ButtonHovered] = ImVec4(0.22f, 0.30f, 0.25f, 1.0f);
    colors[ImGuiCol_ButtonActive] = ImVec4(0.30f, 0.42f, 0.34f, 1.0f);
    colors[ImGuiCol_Header] = ImVec4(0.18f, 0.25f, 0.21f, 1.0f);
    colors[ImGuiCol_HeaderHovered] = ImVec4(0.24f, 0.34f, 0.28f, 1.0f);
    colors[ImGuiCol_HeaderActive] = ImVec4(0.30f, 0.42f, 0.34f, 1.0f);
    colors[ImGuiCol_Separator] = ImVec4(0.24f, 0.27f, 0.25f, 1.0f);
    colors[ImGuiCol_ResizeGrip] = ImVec4(0.24f, 0.34f, 0.28f, 0.45f);
    colors[ImGuiCol_ResizeGripHovered] = ImVec4(0.35f, 0.50f, 0.40f, 0.70f);
    colors[ImGuiCol_Tab] = ImVec4(0.13f, 0.16f, 0.15f, 1.0f);
    colors[ImGuiCol_TabHovered] = ImVec4(0.23f, 0.32f, 0.27f, 1.0f);
    colors[ImGuiCol_TabActive] = ImVec4(0.20f, 0.28f, 0.24f, 1.0f);
    colors[ImGuiCol_TableHeaderBg] = ImVec4(0.14f, 0.17f, 0.16f, 1.0f);
    colors[ImGuiCol_TableBorderStrong] = ImVec4(0.25f, 0.28f, 0.27f, 1.0f);
    colors[ImGuiCol_TableBorderLight] = ImVec4(0.18f, 0.20f, 0.19f, 1.0f);
    colors[ImGuiCol_TableRowBg] = ImVec4(0.09f, 0.10f, 0.10f, 1.0f);
    colors[ImGuiCol_TableRowBgAlt] = ImVec4(0.11f, 0.12f, 0.12f, 1.0f);
}

ImVec4 moneyColor(double value)
{
    if (value > 0.0) {
        return Gain;
    }
    if (value < 0.0) {
        return Loss;
    }
    return MutedText;
}

void textMoney(double value, const char* text)
{
    ImGui::TextColored(moneyColor(value), "%s", text);
}

void sectionHeading(const char* title, const char* subtitle)
{
    ImGui::Spacing();
    ImGui::Text("%s", title);
    if (subtitle != nullptr && subtitle[0] != '\0') {
        ImGui::TextColored(MutedText, "%s", subtitle);
    }
    ImGui::Separator();
}

void metricCard(const char* title, const char* value, const char* caption, ImVec4 accent, ImVec2 size)
{
    ImGui::PushStyleColor(ImGuiCol_ChildBg, CardBackground);
    ImGui::BeginChild(title, size, true, ImGuiWindowFlags_NoScrollbar);
    ImGui::TextColored(MutedText, "%s", title);
    ImGui::Spacing();
    ImGui::SetWindowFontScale(1.18f);
    ImGui::Text("%s", value);
    ImGui::SetWindowFontScale(1.0f);
    ImGui::Spacing();
    ImGui::TextColored(accent, "%s", caption);
    ImGui::EndChild();
    ImGui::PopStyleColor();
}

void emptyState(const char* title, const char* body)
{
    ImGui::BeginChild(title, ImVec2(0.0f, 132.0f), true);
    ImGui::Dummy(ImVec2(0.0f, 8.0f));
    ImGui::TextColored(Amber, "%s", title);
    ImGui::TextColored(MutedText, "%s", body);
    ImGui::EndChild();
}

void formError(const std::string& message)
{
    if (!message.empty()) {
        ImGui::Spacing();
        ImGui::TextColored(Loss, "%s", message.c_str());
    }
}

void badge(const char* label, ImVec4 color)
{
    ImGui::TextColored(color, "%s", label);
}

}
