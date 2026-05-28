// SPDX-License-Identifier: MIT
#pragma once

#include <string>

namespace Date {
std::string nowIso8601();
std::string todayIso8601();
std::string currentMonthPrefix();
std::string currentYearPrefix();
bool isIsoDate(const std::string& value);
}
