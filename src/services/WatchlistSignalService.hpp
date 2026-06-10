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
WatchlistPriceRefreshStatus refreshPrices(
    const std::vector<WatchlistItem>& items,
    MarketDataService& marketDataService,
    WatchlistRepository& repository,
    std::string& error);
}
