// SPDX-License-Identifier: MIT
#include "services/PortfolioCalculator.hpp"

#include <algorithm>

namespace PortfolioCalculator {

HoldingMetrics calculateHolding(const Holding& holding)
{
    HoldingMetrics metrics;
    metrics.costBasis = holding.shares * holding.averageCost;
    metrics.marketValue = holding.shares * holding.currentPrice;
    metrics.gainLossDollar = metrics.marketValue - metrics.costBasis;
    metrics.gainLossPercent = metrics.costBasis == 0.0 ? 0.0 : (metrics.gainLossDollar / metrics.costBasis) * 100.0;
    return metrics;
}

PortfolioSummary calculateSummary(const std::vector<Account>& accounts, const std::vector<Holding>& holdings)
{
    PortfolioSummary summary;

    for (const Account& account : accounts) {
        summary.accountBalance += account.currentBalance;
        summary.cashBalance += account.cashBalance;
        if (account.status == "Active") {
            ++summary.activeAccounts;
        }
    }

    summary.holdingCount = static_cast<int>(holdings.size());

    for (const Holding& holding : holdings) {
        const HoldingMetrics metrics = calculateHolding(holding);
        summary.holdingsMarketValue += metrics.marketValue;
        summary.costBasis += metrics.costBasis;
        summary.gainLossDollar += metrics.gainLossDollar;
    }

    summary.gainLossPercent = summary.costBasis == 0.0
        ? 0.0
        : (summary.gainLossDollar / summary.costBasis) * 100.0;

    return summary;
}

DividendSummary calculateDividends(const std::vector<Dividend>& dividends, const std::string& monthPrefix, const std::string& yearPrefix)
{
    DividendSummary summary;

    for (const Dividend& dividend : dividends) {
        summary.lifetime += dividend.amountReceived;

        if (dividend.dateReceived.rfind(yearPrefix, 0) == 0) {
            summary.thisYear += dividend.amountReceived;
        }

        if (dividend.dateReceived.rfind(monthPrefix, 0) == 0) {
            summary.thisMonth += dividend.amountReceived;
        }
    }

    return summary;
}

GoalMetrics calculateGoal(const Goal& goal)
{
    GoalMetrics metrics;
    metrics.remainingAmount = std::max(0.0, goal.targetAmount - goal.currentAmount);
    metrics.progressPercent = goal.targetAmount == 0.0 ? 0.0 : (goal.currentAmount / goal.targetAmount) * 100.0;
    metrics.progressPercent = std::clamp(metrics.progressPercent, 0.0, 100.0);
    return metrics;
}

WatchlistSummary calculateWatchlist(const std::vector<WatchlistItem>& watchlist)
{
    WatchlistSummary summary;

    for (const WatchlistItem& item : watchlist) {
        if (item.priority == "High") {
            ++summary.highPriority;
        } else if (item.priority == "Low") {
            ++summary.lowPriority;
        } else {
            ++summary.mediumPriority;
        }
    }

    return summary;
}

}
