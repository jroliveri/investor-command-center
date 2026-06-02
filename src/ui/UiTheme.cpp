// SPDX-License-Identifier: MIT
#include "ui/UiTheme.hpp"

#include <string>

namespace UiTheme {

ImVec4 Gain = ImVec4(0.40f, 0.78f, 0.55f, 1.0f);
ImVec4 Loss = ImVec4(0.86f, 0.36f, 0.36f, 1.0f);
ImVec4 Amber = ImVec4(0.93f, 0.68f, 0.30f, 1.0f);
ImVec4 MutedText = ImVec4(0.62f, 0.66f, 0.64f, 1.0f);
ImVec4 CardBackground = ImVec4(0.105f, 0.115f, 0.115f, 1.0f);
ImVec4 PanelBackground = ImVec4(0.085f, 0.095f, 0.092f, 1.0f);
ImVec4 PanelHeader = ImVec4(0.14f, 0.17f, 0.16f, 1.0f);
ImVec4 Accent = ImVec4(0.32f, 0.50f, 0.86f, 1.0f);
ImVec4 TextPrimary = ImVec4(0.90f, 0.93f, 0.91f, 1.0f);
ImVec4 TextSecondary = ImVec4(0.74f, 0.78f, 0.76f, 1.0f);
ImVec4 TextMuted = ImVec4(0.62f, 0.66f, 0.64f, 1.0f);
ImVec4 TextWarning = ImVec4(0.93f, 0.68f, 0.30f, 1.0f);
ImVec4 TextSuccess = ImVec4(0.40f, 0.78f, 0.55f, 1.0f);
ImVec4 TextDanger = ImVec4(0.86f, 0.36f, 0.36f, 1.0f);

namespace {

ThemeMode ActiveTheme = ThemeMode::DarkCommandCenter;
ImVec4 Clear = ImVec4(0.055f, 0.060f, 0.060f, 1.0f);

void setPalette(ThemeMode theme)
{
    ActiveTheme = theme;

    if (theme == ThemeMode::LightTradingTerminal) {
        Gain = ImVec4(0.00f, 0.48f, 0.20f, 1.0f);
        Loss = ImVec4(0.74f, 0.15f, 0.15f, 1.0f);
        Amber = ImVec4(0.72f, 0.48f, 0.12f, 1.0f);
        MutedText = ImVec4(0.36f, 0.39f, 0.41f, 1.0f);
        CardBackground = ImVec4(0.96f, 0.97f, 0.97f, 1.0f);
        PanelBackground = ImVec4(0.92f, 0.93f, 0.93f, 1.0f);
        PanelHeader = ImVec4(0.78f, 0.80f, 0.82f, 1.0f);
        Accent = ImVec4(0.20f, 0.40f, 0.72f, 1.0f);
        TextPrimary = ImVec4(0.08f, 0.09f, 0.10f, 1.0f);
        TextSecondary = ImVec4(0.22f, 0.25f, 0.27f, 1.0f);
        TextMuted = ImVec4(0.36f, 0.39f, 0.41f, 1.0f);
        TextWarning = ImVec4(0.62f, 0.36f, 0.04f, 1.0f);
        TextSuccess = Gain;
        TextDanger = Loss;
        Clear = ImVec4(0.84f, 0.85f, 0.86f, 1.0f);
        return;
    }

    Gain = ImVec4(0.40f, 0.78f, 0.55f, 1.0f);
    Loss = ImVec4(0.86f, 0.36f, 0.36f, 1.0f);
    Amber = ImVec4(0.93f, 0.68f, 0.30f, 1.0f);
    MutedText = ImVec4(0.62f, 0.66f, 0.64f, 1.0f);
    CardBackground = ImVec4(0.105f, 0.115f, 0.115f, 1.0f);
    PanelBackground = ImVec4(0.085f, 0.095f, 0.092f, 1.0f);
    PanelHeader = ImVec4(0.14f, 0.17f, 0.16f, 1.0f);
    Accent = ImVec4(0.32f, 0.50f, 0.86f, 1.0f);
    TextPrimary = ImVec4(0.90f, 0.93f, 0.91f, 1.0f);
    TextSecondary = ImVec4(0.74f, 0.78f, 0.76f, 1.0f);
    TextMuted = ImVec4(0.62f, 0.66f, 0.64f, 1.0f);
    TextWarning = ImVec4(0.93f, 0.68f, 0.30f, 1.0f);
    TextSuccess = Gain;
    TextDanger = Loss;
    Clear = ImVec4(0.055f, 0.060f, 0.060f, 1.0f);
}

void applyCompactGeometry(ImGuiStyle& style)
{
    style.WindowPadding = ImVec2(10.0f, 9.0f);
    style.FramePadding = ImVec2(8.0f, 5.0f);
    style.CellPadding = ImVec2(7.0f, 4.0f);
    style.ItemSpacing = ImVec2(8.0f, 6.0f);
    style.ItemInnerSpacing = ImVec2(6.0f, 4.0f);
    style.ScrollbarSize = 13.0f;
    style.WindowRounding = 0.0f;
    style.ChildRounding = 2.0f;
    style.FrameRounding = 2.0f;
    style.PopupRounding = 2.0f;
    style.GrabRounding = 2.0f;
    style.TabRounding = 2.0f;
    style.WindowBorderSize = 0.0f;
    style.ChildBorderSize = 1.0f;
    style.FrameBorderSize = 1.0f;
}

void applyLightColors(ImVec4* colors)
{
    colors[ImGuiCol_Text] = ImVec4(0.08f, 0.09f, 0.10f, 1.0f);
    colors[ImGuiCol_TextDisabled] = MutedText;
    colors[ImGuiCol_WindowBg] = ImVec4(0.84f, 0.85f, 0.86f, 1.0f);
    colors[ImGuiCol_ChildBg] = PanelBackground;
    colors[ImGuiCol_PopupBg] = ImVec4(0.96f, 0.97f, 0.97f, 1.0f);
    colors[ImGuiCol_Border] = ImVec4(0.58f, 0.61f, 0.63f, 1.0f);
    colors[ImGuiCol_FrameBg] = ImVec4(0.98f, 0.98f, 0.98f, 1.0f);
    colors[ImGuiCol_FrameBgHovered] = ImVec4(0.88f, 0.92f, 0.98f, 1.0f);
    colors[ImGuiCol_FrameBgActive] = ImVec4(0.80f, 0.87f, 0.96f, 1.0f);
    colors[ImGuiCol_TitleBg] = PanelHeader;
    colors[ImGuiCol_TitleBgActive] = PanelHeader;
    colors[ImGuiCol_MenuBarBg] = ImVec4(0.74f, 0.76f, 0.78f, 1.0f);
    colors[ImGuiCol_ScrollbarBg] = ImVec4(0.80f, 0.81f, 0.82f, 1.0f);
    colors[ImGuiCol_ScrollbarGrab] = ImVec4(0.55f, 0.57f, 0.59f, 1.0f);
    colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.44f, 0.47f, 0.50f, 1.0f);
    colors[ImGuiCol_CheckMark] = Accent;
    colors[ImGuiCol_SliderGrab] = Accent;
    colors[ImGuiCol_SliderGrabActive] = ImVec4(0.14f, 0.32f, 0.62f, 1.0f);
    colors[ImGuiCol_Button] = ImVec4(0.82f, 0.84f, 0.86f, 1.0f);
    colors[ImGuiCol_ButtonHovered] = ImVec4(0.76f, 0.83f, 0.94f, 1.0f);
    colors[ImGuiCol_ButtonActive] = ImVec4(0.66f, 0.76f, 0.90f, 1.0f);
    colors[ImGuiCol_Header] = ImVec4(0.76f, 0.83f, 0.94f, 1.0f);
    colors[ImGuiCol_HeaderHovered] = ImVec4(0.68f, 0.78f, 0.92f, 1.0f);
    colors[ImGuiCol_HeaderActive] = ImVec4(0.58f, 0.72f, 0.90f, 1.0f);
    colors[ImGuiCol_Separator] = ImVec4(0.58f, 0.61f, 0.63f, 1.0f);
    colors[ImGuiCol_ResizeGrip] = ImVec4(0.38f, 0.50f, 0.72f, 0.45f);
    colors[ImGuiCol_ResizeGripHovered] = ImVec4(0.20f, 0.40f, 0.72f, 0.70f);
    colors[ImGuiCol_Tab] = ImVec4(0.78f, 0.80f, 0.82f, 1.0f);
    colors[ImGuiCol_TabHovered] = ImVec4(0.70f, 0.80f, 0.94f, 1.0f);
    colors[ImGuiCol_TabActive] = ImVec4(0.90f, 0.93f, 0.98f, 1.0f);
    colors[ImGuiCol_TableHeaderBg] = ImVec4(0.76f, 0.78f, 0.80f, 1.0f);
    colors[ImGuiCol_TableBorderStrong] = ImVec4(0.50f, 0.53f, 0.55f, 1.0f);
    colors[ImGuiCol_TableBorderLight] = ImVec4(0.68f, 0.70f, 0.72f, 1.0f);
    colors[ImGuiCol_TableRowBg] = ImVec4(0.95f, 0.96f, 0.96f, 1.0f);
    colors[ImGuiCol_TableRowBgAlt] = ImVec4(0.90f, 0.92f, 0.93f, 1.0f);
}

void applyDarkColors(ImVec4* colors)
{
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

}

ThemeMode themeFromKey(const std::string& key)
{
    if (key == "light_trading_terminal") {
        return ThemeMode::LightTradingTerminal;
    }
    return ThemeMode::DarkCommandCenter;
}

std::string themeKey(ThemeMode theme)
{
    return theme == ThemeMode::LightTradingTerminal ? "light_trading_terminal" : "dark_command_center";
}

const char* themeName(ThemeMode theme)
{
    return theme == ThemeMode::LightTradingTerminal ? "Light Trading Terminal" : "Dark Command Center";
}

ThemeMode currentTheme()
{
    return ActiveTheme;
}

ImVec4 clearColor()
{
    return Clear;
}

void apply(ThemeMode theme)
{
    setPalette(theme);
    ImGuiStyle& style = ImGui::GetStyle();
    applyCompactGeometry(style);

    if (theme == ThemeMode::LightTradingTerminal) {
        applyLightColors(style.Colors);
    } else {
        applyDarkColors(style.Colors);
    }
}

void apply()
{
    apply(ActiveTheme);
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
    ImGui::TextColored(TextPrimary, "%s", title);
    if (subtitle != nullptr && subtitle[0] != '\0') {
        ImGui::TextColored(TextMuted, "%s", subtitle);
    }
    ImGui::Separator();
}

void metricCard(const char* title, const char* value, const char* caption, ImVec4 accent, ImVec2 size)
{
    ImGui::PushStyleColor(ImGuiCol_ChildBg, CardBackground);
    ImGui::BeginChild(title, size, true, ImGuiWindowFlags_NoScrollbar);
    ImGui::TextColored(TextSecondary, "%s", title);
    ImGui::SetWindowFontScale(1.08f);
    ImGui::TextColored(TextPrimary, "%s", value);
    ImGui::SetWindowFontScale(1.0f);
    ImGui::TextColored(accent, "%s", caption);
    ImGui::EndChild();
    ImGui::PopStyleColor();
}

void emptyState(const char* title, const char* body)
{
    ImGui::BeginChild(title, ImVec2(0.0f, 108.0f), true);
    ImGui::TextColored(TextWarning, "%s", title);
    ImGui::TextColored(TextMuted, "%s", body);
    ImGui::EndChild();
}

void formError(const std::string& message)
{
    if (!message.empty()) {
        ImGui::Spacing();
        ImGui::TextColored(TextDanger, "%s", message.c_str());
    }
}

void badge(const char* label, ImVec4 color)
{
    ImGui::TextColored(color, "%s", label);
}

}
