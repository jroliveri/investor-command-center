// SPDX-License-Identifier: MIT
#pragma once

#include <imgui.h>

#include <string>

namespace UiTheme {
constexpr ImVec4 Gain = ImVec4(0.40f, 0.78f, 0.55f, 1.0f);
constexpr ImVec4 Loss = ImVec4(0.86f, 0.36f, 0.36f, 1.0f);
constexpr ImVec4 Amber = ImVec4(0.93f, 0.68f, 0.30f, 1.0f);
constexpr ImVec4 MutedText = ImVec4(0.62f, 0.66f, 0.64f, 1.0f);
constexpr ImVec4 CardBackground = ImVec4(0.105f, 0.115f, 0.115f, 1.0f);
constexpr ImVec4 PanelBackground = ImVec4(0.085f, 0.095f, 0.092f, 1.0f);

void apply();
ImVec4 moneyColor(double value);
void textMoney(double value, const char* text);
void sectionHeading(const char* title, const char* subtitle = nullptr);
void metricCard(const char* title, const char* value, const char* caption, ImVec4 accent, ImVec2 size);
void emptyState(const char* title, const char* body);
void formError(const std::string& message);
void badge(const char* label, ImVec4 color);
}
