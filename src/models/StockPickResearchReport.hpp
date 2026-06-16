// SPDX-License-Identifier: MIT
#pragma once

#include <string>
#include <vector>

enum class StockPickReportPeriod {
    Weekly,
    Monthly,
};

struct StockPickResearchReportHoldingRow {
    std::string ticker;
    std::string assetName;
    double shares = 0.0;
    double currentPrice = 0.0;
    bool hasCurrentPrice = false;
    double marketValue = 0.0;
    bool hasMarketValue = false;
    double currentAllocationPercent = 0.0;
    double targetAllocationPercent = 0.0;
    bool hasTargetAllocation = false;
    double allocationDifferencePercent = 0.0;
    bool hasAllocationDifference = false;
    std::string signalStatus = "Not Tracked";
    bool tracked = false;
    double buyLevel = 0.0;
    bool hasBuyLevel = false;
    std::string priority;
    bool hasPriority = false;
};

struct StockPickResearchReport {
    StockPickReportPeriod period = StockPickReportPeriod::Weekly;
    std::string title;
    std::string generatedAt;
    std::string accountName;
    double totalAccountValue = 0.0;
    double holdingsValue = 0.0;
    double cashBalance = 0.0;
    int holdingCount = 0;
    std::vector<StockPickResearchReportHoldingRow> holdings;
};
