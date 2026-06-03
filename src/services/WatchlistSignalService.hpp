// SPDX-License-Identifier: MIT
#pragma once

#include "models/WatchlistItem.hpp"
#include "models/WatchlistPriceRefresh.hpp"

#include <string>
#include <vector>

class MarketDataService;
class WatchlistRepository;

namespace WatchlistSignalService {
std::string calculateSignalStatus(double currentPrice, double buySignalPrice, double sellSignalPrice);
int signalSortRank(const std::string& signalStatus);
int signalSortRank(const WatchlistItem& item);
bool hasSignalWarning(double currentPrice, double buySignalPrice, double sellSignalPrice);
bool hasSignalWarning(const WatchlistItem& item);
WatchlistPriceRefreshStatus refreshPrices(
    const std::vector<WatchlistItem>& items,
    MarketDataService& marketDataService,
    WatchlistRepository& repository,
    std::string& error);
}
