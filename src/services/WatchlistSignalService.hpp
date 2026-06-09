// SPDX-License-Identifier: MIT
#pragma once

<<<<<<< Updated upstream
=======
#include "models/TechnicalIndicatorSnapshot.hpp"
#include "models/SignalRules.hpp"
>>>>>>> Stashed changes
#include "models/WatchlistItem.hpp"
#include "models/WatchlistPriceRefresh.hpp"

#include <string>
#include <vector>

class MarketDataService;
class WatchlistRepository;

namespace WatchlistSignalService {
<<<<<<< Updated upstream
std::string calculateSignalStatus(double currentPrice, double buySignalPrice, double sellSignalPrice);
=======
WatchlistSignalResult calculateSignal(
    double currentPrice,
    double buySignalPrice,
    double sellSignalPrice,
    const std::optional<TechnicalIndicatorSnapshot>& technicalIndicators);
WatchlistSignalResult calculateSignal(
    double currentPrice,
    double buySignalPrice,
    double sellSignalPrice,
    const std::optional<TechnicalIndicatorSnapshot>& technicalIndicators,
    const SignalRules& signalRules);
WatchlistSignalResult calculateSignal(const WatchlistItem& item, const std::optional<TechnicalIndicatorSnapshot>& technicalIndicators);
WatchlistSignalResult calculateSignal(const WatchlistItem& item, const std::optional<TechnicalIndicatorSnapshot>& technicalIndicators, const SignalRules& signalRules);
std::string calculateSignalStatus(double currentPrice, double buySignalPrice, double sellSignalPrice);
std::string calculateSignalStatus(double currentPrice, double buySignalPrice, double sellSignalPrice, const SignalRules& signalRules);
std::string calculateSignalStatus(const WatchlistItem& item, const std::optional<TechnicalIndicatorSnapshot>& technicalIndicators);
std::string calculateSignalStatus(const WatchlistItem& item, const std::optional<TechnicalIndicatorSnapshot>& technicalIndicators, const SignalRules& signalRules);
int signalSortRank(const std::string& signalStatus);
int signalSortRank(const WatchlistItem& item);
int signalSortRank(const WatchlistItem& item, const std::optional<TechnicalIndicatorSnapshot>& technicalIndicators);
int signalSortRank(const WatchlistItem& item, const std::optional<TechnicalIndicatorSnapshot>& technicalIndicators, const SignalRules& signalRules);
bool hasSignalWarning(double currentPrice, double buySignalPrice, double sellSignalPrice);
bool hasSignalWarning(const WatchlistItem& item);
>>>>>>> Stashed changes
WatchlistPriceRefreshStatus refreshPrices(
    const std::vector<WatchlistItem>& items,
    MarketDataService& marketDataService,
    WatchlistRepository& repository,
    std::string& error);
WatchlistPriceRefreshStatus refreshPrices(
    const std::vector<WatchlistItem>& items,
    MarketDataService& marketDataService,
    TechnicalIndicatorService& technicalIndicatorService,
    WatchlistRepository& repository,
    std::string& error,
    const SignalRules& signalRules);
}
