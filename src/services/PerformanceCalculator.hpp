// SPDX-License-Identifier: MIT
#pragma once

#include "models/PortfolioSnapshot.hpp"
#include "models/Transaction.hpp"

#include <string>
#include <vector>

struct RealizedGainSummary {
    double today = 0.0;
    double thisMonth = 0.0;
    double thisYear = 0.0;
    double lifetime = 0.0;
    int sellCount = 0;
};

struct PerformanceComparison {
    bool hasDaily = false;
    bool hasMonthly = false;
    bool hasYearly = false;
    double daily = 0.0;
    double monthly = 0.0;
    double yearly = 0.0;
};

namespace PerformanceCalculator {
RealizedGainSummary calculateRealizedGains(
    const std::vector<Transaction>& transactions,
    const std::string& today,
    const std::string& monthPrefix,
    const std::string& yearPrefix);

PerformanceComparison calculateSnapshotPerformance(
    const std::vector<PortfolioSnapshot>& snapshots,
    double currentPortfolioValue,
    const std::string& today,
    const std::string& monthPrefix,
    const std::string& yearPrefix);
}
