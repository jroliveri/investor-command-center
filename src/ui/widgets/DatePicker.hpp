// SPDX-License-Identifier: MIT
#pragma once

#include <string>

namespace DatePicker {
bool draw(const char* label, std::string& value, bool optional = false);
void drawTableDate(const std::string& value, bool optional = false);
}
