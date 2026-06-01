// SPDX-License-Identifier: MIT
#include "services/DashboardService.hpp"

#include <algorithm>
#include <cctype>

namespace {

std::string normalizeSymbol(std::string symbol)
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

}

namespace DashboardService {

DashboardData build(const AppState& state, const std::string& today, const std::string& monthPrefix, const std::string& yearPrefix)
{
    DashboardData data;
    const std::vector<Holding> pricedHoldings = holdingsWithDashboardPrices(state);
    data.portfolio = PortfolioCalculator::calculateSummary(state.accounts, pricedHoldings);
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

std::vector<Holding> holdingsWithDashboardPrices(const AppState& state)
{
    std::vector<Holding> holdings = state.holdings;
    for (Holding& holding : holdings) {
        if (const DashboardPriceOverride* priceOverride = priceOverrideForSymbol(state, holding.ticker)) {
            holding.currentPrice = priceOverride->currentPrice;
        }
    }
    return holdings;
}

const DashboardPriceOverride* priceOverrideForSymbol(const AppState& state, const std::string& symbol)
{
    const std::string normalizedSymbol = normalizeSymbol(symbol);
    if (normalizedSymbol.empty()) {
        return nullptr;
    }

    for (const DashboardPriceOverride& priceOverride : state.dashboardPriceOverrides) {
        if (normalizeSymbol(priceOverride.symbol) == normalizedSymbol) {
            return &priceOverride;
        }
    }

    return nullptr;
}

std::string priceSourceForHolding(const AppState& state, const Holding& holding)
{
    if (const DashboardPriceOverride* priceOverride = priceOverrideForSymbol(state, holding.ticker)) {
        return priceOverride->fromCache ? "Cached Quote" : "Live Quote";
    }
    if (holding.lastImportBatchId != 0 || !holding.lastSeenAt.empty()) {
        return "CSV Import";
    }
    return "Manual";
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
