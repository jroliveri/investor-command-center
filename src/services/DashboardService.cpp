// SPDX-License-Identifier: MIT
#include "services/DashboardService.hpp"

namespace DashboardService {

DashboardData build(const AppState& state, const std::string& today, const std::string& monthPrefix, const std::string& yearPrefix)
{
    DashboardData data;
    data.portfolio = PortfolioCalculator::calculateSummary(state.accounts, state.holdings);
    data.dividends = PortfolioCalculator::calculateDividends(state.dividends, monthPrefix, yearPrefix);
    data.watchlist = PortfolioCalculator::calculateWatchlist(state.watchlist);
    data.realizedGains = PerformanceCalculator::calculateRealizedGains(state.transactions, today, monthPrefix, yearPrefix);
    data.performance = PerformanceCalculator::calculateSnapshotPerformance(
        state.portfolioSnapshots,
        data.portfolio.accountBalance,
        today,
        monthPrefix,
        yearPrefix);
    return data;
}

PortfolioSnapshot buildSnapshot(const DashboardData& data, const std::string& snapshotDate)
{
    PortfolioSnapshot snapshot;
    snapshot.snapshotDate = snapshotDate;
    snapshot.portfolioValue = data.portfolio.accountBalance;
    snapshot.holdingsValue = data.portfolio.holdingsMarketValue;
    snapshot.cashBalance = data.portfolio.cashBalance;
    snapshot.costBasis = data.portfolio.costBasis;
    snapshot.unrealizedGain = data.portfolio.gainLossDollar;
    snapshot.realizedGainDay = data.realizedGains.today;
    snapshot.realizedGainMonth = data.realizedGains.thisMonth;
    snapshot.realizedGainYear = data.realizedGains.thisYear;
    snapshot.notes = "Created from dashboard.";
    return snapshot;
}

bool hasSnapshotForDate(const AppState& state, const std::string& snapshotDate)
{
    for (const PortfolioSnapshot& snapshot : state.portfolioSnapshots) {
        if (snapshot.snapshotDate == snapshotDate) {
            return true;
        }
    }

    return false;
}

const PortfolioSnapshot* latestSnapshot(const AppState& state)
{
    const PortfolioSnapshot* latest = nullptr;
    for (const PortfolioSnapshot& snapshot : state.portfolioSnapshots) {
        if (latest == nullptr || snapshot.snapshotDate > latest->snapshotDate ||
            (snapshot.snapshotDate == latest->snapshotDate && snapshot.id > latest->id)) {
            latest = &snapshot;
        }
    }

    return latest;
}

const ImportBatch* latestImportBatch(const AppState& state)
{
    const ImportBatch* latest = nullptr;
    for (const ImportBatch& batch : state.importBatches) {
        if (latest == nullptr || batch.importDate > latest->importDate || (batch.importDate == latest->importDate && batch.id > latest->id)) {
            latest = &batch;
        }
    }

    return latest;
}

}
