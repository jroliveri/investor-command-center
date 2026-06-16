// SPDX-License-Identifier: MIT
#pragma once

#include "models/StockPickResearchReport.hpp"

#include <optional>

class AppState;

namespace StockPickReportService {
const char* reportPeriodLabel(StockPickReportPeriod period);
std::optional<StockPickResearchReport> buildResearchReport(const AppState& state, int accountId, StockPickReportPeriod period);
}
