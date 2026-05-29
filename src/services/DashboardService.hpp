// SPDX-License-Identifier: MIT
#pragma once

#include "app/AppState.hpp"
#include "models/PortfolioSnapshot.hpp"
#include "services/PerformanceCalculator.hpp"
#include "services/PortfolioCalculator.hpp"

#include <string>

struct DashboardData {
    PortfolioSummary portfolio;
    DividendSummary dividends;
    WatchlistSummary watchlist;
    RealizedGainSummary realizedGains;
    PerformanceComparison performance;
};

namespace DashboardService {
DashboardData build(const AppState& state, const std::string& today, const std::string& monthPrefix, const std::string& yearPrefix);
PortfolioSnapshot buildSnapshot(const DashboardData& data, const std::string& snapshotDate);
bool hasSnapshotForDate(const AppState& state, const std::string& snapshotDate);
const PortfolioSnapshot* latestSnapshot(const AppState& state);
const ImportBatch* latestImportBatch(const AppState& state);
}
