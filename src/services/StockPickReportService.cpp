// SPDX-License-Identifier: MIT
#include "services/StockPickReportService.hpp"

#include "app/AppState.hpp"
#include "services/DashboardService.hpp"
#include "services/PortfolioCalculator.hpp"
#include "util/Date.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <string>
#include <vector>

namespace {

bool isActiveHolding(const Holding& holding)
{
    return holding.status.empty() || holding.status == "Active";
}

std::string normalizedSymbol(std::string symbol)
{
    symbol.erase(symbol.begin(), std::find_if(symbol.begin(), symbol.end(), [](unsigned char character) {
        return std::isspace(character) == 0;
    }));
    symbol.erase(std::find_if(symbol.rbegin(), symbol.rend(), [](unsigned char character) {
        return std::isspace(character) == 0;
    }).base(), symbol.end());
    std::transform(symbol.begin(), symbol.end(), symbol.begin(), [](unsigned char character) {
        return static_cast<char>(std::toupper(character));
    });
    return symbol;
}

const Account* findAccount(const AppState& state, int accountId)
{
    for (const Account& account : state.accounts) {
        if (account.id == accountId) {
            return &account;
        }
    }

    return nullptr;
}

const WatchlistItem* findWatchlistItem(const AppState& state, const std::string& ticker)
{
    const std::string symbol = normalizedSymbol(ticker);
    if (symbol.empty()) {
        return nullptr;
    }

    for (const WatchlistItem& item : state.watchlist) {
        if (normalizedSymbol(item.ticker) == symbol) {
            return &item;
        }
    }

    return nullptr;
}

std::string normalizedSignalStatus(const WatchlistItem& item)
{
    const std::string signal = item.signalStatus;
    if (signal == "Buy Signal") {
        return "Buy";
    }
    if (signal == "Sell Signal") {
        return "Sell";
    }
    if (signal.empty()) {
        return "Hold";
    }
    return signal;
}

int signalStatusRank(const std::string& signalStatus)
{
    if (signalStatus == "Buy") {
        return 0;
    }
    if (signalStatus == "Sell") {
        return 1;
    }
    return 2;
}

int signalRank(const StockPickResearchReportHoldingRow& row)
{
    if (!row.tracked) {
        return 3;
    }
    return signalStatusRank(row.signalStatus);
}

int priorityRank(const StockPickResearchReportHoldingRow& row)
{
    if (row.priority == "High") {
        return 0;
    }
    if (row.priority == "Medium") {
        return 1;
    }
    if (row.priority == "Low") {
        return 2;
    }
    return 3;
}

std::vector<Holding> selectedAccountHoldings(const AppState& state, int accountId)
{
    std::vector<Holding> holdings = DashboardService::holdingsWithDashboardPrices(state);
    holdings.erase(
        std::remove_if(holdings.begin(), holdings.end(), [accountId](const Holding& holding) {
            return holding.accountId != accountId || !isActiveHolding(holding);
        }),
        holdings.end());
    return holdings;
}

} // namespace

namespace StockPickReportService {

const char* reportPeriodLabel(StockPickReportPeriod period)
{
    return period == StockPickReportPeriod::Monthly ? "Monthly" : "Weekly";
}

std::optional<StockPickResearchReport> buildResearchReport(const AppState& state, int accountId, StockPickReportPeriod period)
{
    const Account* account = findAccount(state, accountId);
    if (account == nullptr) {
        return std::nullopt;
    }

    const std::vector<Holding> holdings = selectedAccountHoldings(state, accountId);
    const AccountMetrics accountMetrics = PortfolioCalculator::calculateAccount(*account, holdings);

    StockPickResearchReport report;
    report.period = period;
    report.title = std::string(reportPeriodLabel(period)) + " Stock Pick Helper Research Report";
    report.generatedAt = Date::nowIso8601();
    report.accountName = account->accountName;
    report.totalAccountValue = accountMetrics.calculatedBalance;
    report.holdingsValue = accountMetrics.holdingsMarketValue;
    report.cashBalance = accountMetrics.cashBalance;
    report.holdingCount = static_cast<int>(holdings.size());

    for (const Holding& holding : holdings) {
        const HoldingMetrics holdingMetrics = PortfolioCalculator::calculateHolding(holding);
        const HoldingAllocationMetrics allocation = PortfolioCalculator::calculateHoldingAllocation(holding, accountMetrics.calculatedBalance);
        const WatchlistItem* watchlistItem = findWatchlistItem(state, holding.ticker);

        StockPickResearchReportHoldingRow row;
        row.ticker = holding.ticker;
        row.assetName = holding.assetName;
        row.shares = holding.shares;
        row.currentPrice = holding.currentPrice;
        row.hasCurrentPrice = holding.currentPrice > 0.0;
        row.marketValue = holdingMetrics.marketValue;
        row.hasMarketValue = row.hasCurrentPrice;
        row.currentAllocationPercent = allocation.actualAllocationPercent;
        row.targetAllocationPercent = allocation.targetAllocationPercent;
        row.hasTargetAllocation = allocation.targetAllocationPercent > 0.005;
        row.allocationDifferencePercent = allocation.differencePercent;
        row.hasAllocationDifference = row.hasTargetAllocation;

        if (watchlistItem != nullptr) {
            row.tracked = true;
            row.signalStatus = normalizedSignalStatus(*watchlistItem);
            row.buyLevel = watchlistItem->buySignalPrice;
            row.hasBuyLevel = watchlistItem->buySignalPrice > 0.0;
            row.priority = watchlistItem->priority;
            row.hasPriority = !watchlistItem->priority.empty();
        }

        report.holdings.push_back(row);
    }

    std::stable_sort(report.holdings.begin(), report.holdings.end(), [](const StockPickResearchReportHoldingRow& left, const StockPickResearchReportHoldingRow& right) {
        const int leftSignalRank = signalRank(left);
        const int rightSignalRank = signalRank(right);
        if (leftSignalRank != rightSignalRank) {
            return leftSignalRank < rightSignalRank;
        }

        const int leftPriorityRank = priorityRank(left);
        const int rightPriorityRank = priorityRank(right);
        if (leftPriorityRank != rightPriorityRank) {
            return leftPriorityRank < rightPriorityRank;
        }

        const double leftDifference = left.hasAllocationDifference ? std::fabs(left.allocationDifferencePercent) : 0.0;
        const double rightDifference = right.hasAllocationDifference ? std::fabs(right.allocationDifferencePercent) : 0.0;
        if (std::fabs(leftDifference - rightDifference) > 0.005) {
            return leftDifference > rightDifference;
        }

        return normalizedSymbol(left.ticker) < normalizedSymbol(right.ticker);
    });

    return report;
}

}
