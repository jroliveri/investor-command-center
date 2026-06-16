// SPDX-License-Identifier: MIT
#pragma once

#include "models/StockPickResearchReport.hpp"

#include <optional>
#include <string>

class AppState;

class ReportsView {
public:
    void render(AppState& state);

private:
    void exportPdfPreview(AppState& state);
    void exportWordPreview(AppState& state);

    int selectedAccountId_ = 0;
    StockPickReportPeriod selectedPeriod_ = StockPickReportPeriod::Weekly;
    std::optional<StockPickResearchReport> preview_;
    std::string message_;
    bool messageIsError_ = false;
};
