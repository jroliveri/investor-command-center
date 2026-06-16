// SPDX-License-Identifier: MIT
#pragma once

#include "models/SignalRules.hpp"
#include "models/TechnicalIndicatorSnapshot.hpp"
#include "models/WatchlistItem.hpp"
#include "models/WatchlistPriceRefresh.hpp"

#include <optional>
#include <string>
#include <vector>

class MarketDataService;
class TechnicalIndicatorService;
class WatchlistRepository;

struct WatchlistSignalResult {
    std::string signal = "Hold";
    bool priceConditionMet = false;
    bool sellConditionMet = false;
    bool rsiConditionMet = false;
    bool macdConditionMet = false;
    bool hasCurrentPrice = false;
    bool hasRsi = false;
    bool hasMacd = false;
    std::string reasonText;
};

namespace WatchlistSignalService {
std::string signalDetailText(
    const WatchlistItem& item,
    const std::optional<TechnicalIndicatorSnapshot>& technicalIndicators,
    const TechnicalIndicatorEvaluation* displayedTechnicals = nullptr);
std::string signalDetailText(
    const WatchlistItem& item,
    const std::optional<TechnicalIndicatorSnapshot>& technicalIndicators,
    const TechnicalIndicatorEvaluation* displayedTechnicals,
    const SignalRules& rules);
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
    const SignalRules& rules);
WatchlistSignalResult calculateSignal(const WatchlistItem& item, const std::optional<TechnicalIndicatorSnapshot>& technicalIndicators);
WatchlistSignalResult calculateSignal(const WatchlistItem& item, const std::optional<TechnicalIndicatorSnapshot>& technicalIndicators, const SignalRules& rules);
std::string calculateSignalStatus(double currentPrice, double buySignalPrice, double sellSignalPrice);
std::string calculateSignalStatus(const WatchlistItem& item, const std::optional<TechnicalIndicatorSnapshot>& technicalIndicators);
int signalSortRank(const std::string& signalStatus);
int signalSortRank(const WatchlistItem& item);
int signalSortRank(const WatchlistItem& item, const std::optional<TechnicalIndicatorSnapshot>& technicalIndicators);
bool hasSignalWarning(double currentPrice, double buySignalPrice, double sellSignalPrice);
bool hasSignalWarning(const WatchlistItem& item);
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
    std::string& error);
}
