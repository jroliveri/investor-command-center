// SPDX-License-Identifier: MIT
#pragma once

#include <string>

namespace Money {
std::string format(double value);
std::string formatNumber(double value, int decimals = 2);
std::string formatQuantity(double value);
std::string formatPercent(double value, bool includeSign = false);
}
