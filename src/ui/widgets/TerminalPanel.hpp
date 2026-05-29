// SPDX-License-Identifier: MIT
#pragma once

#include <imgui.h>

namespace TerminalPanel {
bool begin(const char* title, ImVec2 size = ImVec2(0.0f, 0.0f), ImGuiWindowFlags flags = 0);
void end();
void metricCell(const char* label, const char* value, ImVec4 accent);
}
