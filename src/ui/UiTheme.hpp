// SPDX-License-Identifier: MIT
#pragma once

#include <imgui.h>

#include <string>

namespace UiTheme {
enum class ThemeMode {
    DarkCommandCenter,
    LightTradingTerminal
};

extern ImVec4 Gain;
extern ImVec4 Loss;
extern ImVec4 Amber;
extern ImVec4 MutedText;
extern ImVec4 CardBackground;
extern ImVec4 PanelBackground;
extern ImVec4 PanelHeader;
extern ImVec4 Accent;
extern ImVec4 TextPrimary;
extern ImVec4 TextSecondary;
extern ImVec4 TextMuted;
extern ImVec4 TextWarning;
extern ImVec4 TextSuccess;
extern ImVec4 TextDanger;

ThemeMode themeFromKey(const std::string& key);
std::string themeKey(ThemeMode theme);
const char* themeName(ThemeMode theme);
ThemeMode currentTheme();
ImVec4 clearColor();
void apply();
void apply(ThemeMode theme);
ImVec4 moneyColor(double value);
void textMoney(double value, const char* text);
void sectionHeading(const char* title, const char* subtitle = nullptr);
void metricCard(const char* title, const char* value, const char* caption, ImVec4 accent, ImVec2 size);
void emptyState(const char* title, const char* body);
void formError(const std::string& message);
void badge(const char* label, ImVec4 color);
}
