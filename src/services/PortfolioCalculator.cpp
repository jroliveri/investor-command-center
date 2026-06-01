// SPDX-License-Identifier: MIT
#include "services/PortfolioCalculator.hpp"

#include <algorithm>

namespace PortfolioCalculator {

namespace {

bool isActiveHolding(const Holding& holding)
{
    return holding.status.empty() || holding.status == "Active";
}

bool isActiveAccount(const Account& account)
{
    return account.status.empty() || account.status == "Active";
}

const Account* findAccount(const std::vector<Account>& accounts, int accountId)
{
    for (const Account& account : accounts) {
        if (account.id == accountId) {
            return &account;
        }
    }

    return nullptr;
}

GoalMetrics calculateGoalFromCurrentAmount(const Goal& goal, double currentAmount)
{
    GoalMetrics metrics;
    metrics.effectiveCurrentAmount = currentAmount;
    metrics.remainingAmount = std::max(0.0, goal.targetAmount - currentAmount);
    metrics.progressPercent = goal.targetAmount == 0.0 ? 0.0 : (currentAmount / goal.targetAmount) * 100.0;
    metrics.progressPercent = std::clamp(metrics.progressPercent, 0.0, 100.0);
    return metrics;
}

}

HoldingMetrics calculateHolding(const Holding& holding)
{
    HoldingMetrics metrics;
    metrics.costBasis = holding.shares * holding.averageCost;
    metrics.marketValue = holding.shares * holding.currentPrice;
    metrics.gainLossDollar = metrics.marketValue - metrics.costBasis;
    metrics.gainLossPercent = metrics.costBasis == 0.0 ? 0.0 : (metrics.gainLossDollar / metrics.costBasis) * 100.0;
    return metrics;
}

AccountMetrics calculateAccount(const Account& account, const std::vector<Holding>& holdings)
{
    AccountMetrics metrics;
    metrics.cashBalance = account.cashBalance;

    for (const Holding& holding : holdings) {
        if (holding.accountId == account.id && isActiveHolding(holding)) {
            metrics.holdingsMarketValue += calculateHolding(holding).marketValue;
        }
    }

    metrics.calculatedBalance = metrics.holdingsMarketValue + metrics.cashBalance;
    return metrics;
}

PortfolioSummary calculateSummary(const std::vector<Account>& accounts, const std::vector<Holding>& holdings)
{
    PortfolioSummary summary;

    for (const Account& account : accounts) {
        if (!isActiveAccount(account)) {
            continue;
        }
        const AccountMetrics accountMetrics = calculateAccount(account, holdings);
        summary.accountBalance += accountMetrics.calculatedBalance;
        summary.cashBalance += account.cashBalance;
        ++summary.activeAccounts;
    }

    for (const Holding& holding : holdings) {
        if (!isActiveHolding(holding)) {
            continue;
        }

        const Account* account = findAccount(accounts, holding.accountId);
        if (account == nullptr || !isActiveAccount(*account)) {
            continue;
        }

        ++summary.holdingCount;
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
    GoalMetrics metrics = calculateGoalFromCurrentAmount(goal, goal.currentAmount);
    metrics.usesAccountValue = goal.useAccountValue;
    metrics.missingLinkedAccount = goal.useAccountValue;
    return metrics;
}

GoalMetrics calculateGoal(const Goal& goal, const std::vector<Account>& accounts, const std::vector<Holding>& holdings)
{
    if (!goal.useAccountValue) {
        return calculateGoal(goal);
    }

    const Account* account = findAccount(accounts, goal.linkedAccountId);
    if (account == nullptr) {
        GoalMetrics metrics = calculateGoalFromCurrentAmount(goal, 0.0);
        metrics.usesAccountValue = true;
        metrics.missingLinkedAccount = true;
        return metrics;
    }

    GoalMetrics metrics = calculateGoalFromCurrentAmount(goal, calculateAccount(*account, holdings).calculatedBalance);
    metrics.usesAccountValue = true;
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
