// SPDX-License-Identifier: MIT
#pragma once

#include "models/Account.hpp"
#include "models/Dividend.hpp"
#include "models/Goal.hpp"
#include "models/Holding.hpp"
#include "models/WatchlistItem.hpp"

#include <vector>

enum class AllocationStatus {
    Underweight,
    OnTarget,
    Overweight,
};

struct HoldingMetrics {
    double costBasis = 0.0;
    double marketValue = 0.0;
    double gainLossDollar = 0.0;
    double gainLossPercent = 0.0;
};

struct HoldingAllocationMetrics {
    double actualAllocationPercent = 0.0;
    double targetAllocationPercent = 0.0;
    double targetAmount = 0.0;
    double differenceDollar = 0.0;
    double differencePercent = 0.0;
    AllocationStatus status = AllocationStatus::OnTarget;
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

struct AccountMetrics {
    double holdingsMarketValue = 0.0;
    double cashBalance = 0.0;
    double calculatedBalance = 0.0;
};

struct DividendSummary {
    double thisMonth = 0.0;
    double thisYear = 0.0;
    double lifetime = 0.0;
};

struct GoalMetrics {
    double effectiveCurrentAmount = 0.0;
    double progressPercent = 0.0;
    double remainingAmount = 0.0;
    bool usesAccountValue = false;
    bool missingLinkedAccount = false;
};

struct WatchlistSummary {
    int highPriority = 0;
    int mediumPriority = 0;
    int lowPriority = 0;
};

namespace PortfolioCalculator {
HoldingMetrics calculateHolding(const Holding& holding);
HoldingAllocationMetrics calculateHoldingAllocation(const Holding& holding, double totalInvestmentAccountValue);
AllocationStatus allocationStatus(double actualAllocationPercent, double targetAllocationPercent);
const char* allocationStatusLabel(AllocationStatus status);
AccountMetrics calculateAccount(const Account& account, const std::vector<Holding>& holdings);
PortfolioSummary calculateSummary(const std::vector<Account>& accounts, const std::vector<Holding>& holdings);
DividendSummary calculateDividends(const std::vector<Dividend>& dividends, const std::string& monthPrefix, const std::string& yearPrefix);
GoalMetrics calculateGoal(const Goal& goal);
GoalMetrics calculateGoal(const Goal& goal, const std::vector<Account>& accounts, const std::vector<Holding>& holdings);
WatchlistSummary calculateWatchlist(const std::vector<WatchlistItem>& watchlist);
}
