// SPDX-License-Identifier: MIT
#pragma once

#include "models/Account.hpp"
#include "models/Dividend.hpp"
#include "models/Goal.hpp"
#include "models/Holding.hpp"
#include "models/WatchlistItem.hpp"

#include <vector>

struct HoldingMetrics {
    double costBasis = 0.0;
    double marketValue = 0.0;
    double gainLossDollar = 0.0;
    double gainLossPercent = 0.0;
};

struct PortfolioSummary {
    double accountBalance = 0.0;
    double cashBalance = 0.0;
    double holdingsMarketValue = 0.0;
    double costBasis = 0.0;
    double gainLossDollar = 0.0;
    double gainLossPercent = 0.0;
    int activeAccounts = 0;
    int holdingCount = 0;
};

struct DividendSummary {
    double thisMonth = 0.0;
    double thisYear = 0.0;
    double lifetime = 0.0;
};

struct GoalMetrics {
    double progressPercent = 0.0;
    double remainingAmount = 0.0;
};

struct WatchlistSummary {
    int highPriority = 0;
    int mediumPriority = 0;
    int lowPriority = 0;
};

namespace PortfolioCalculator {
HoldingMetrics calculateHolding(const Holding& holding);
PortfolioSummary calculateSummary(const std::vector<Account>& accounts, const std::vector<Holding>& holdings);
DividendSummary calculateDividends(const std::vector<Dividend>& dividends, const std::string& monthPrefix, const std::string& yearPrefix);
GoalMetrics calculateGoal(const Goal& goal);
WatchlistSummary calculateWatchlist(const std::vector<WatchlistItem>& watchlist);
}
