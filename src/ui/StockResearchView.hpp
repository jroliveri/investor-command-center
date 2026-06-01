// SPDX-License-Identifier: MIT
#pragma once

#include "models/MarketQuote.hpp"

#include <functional>
#include <string>

class AppState;
class MarketDataService;
class WatchlistRepository;

class StockResearchView {
public:
    void render(AppState& state,
        MarketDataService& marketDataService,
        WatchlistRepository& watchlistRepository,
        const std::function<void()>& reloadData);
    void refreshCurrent(MarketDataService& marketDataService, AppState& state);

private:
    void fetchSymbol(MarketDataService& marketDataService, AppState& state);
    void renderQuoteSummary();
    void renderMetrics();
    void renderHistoryPlaceholder();
    void renderWatchlistAction(AppState& state, WatchlistRepository& watchlistRepository, const std::function<void()>& reloadData);

    std::string searchSymbol_;
    MarketQuoteResult lastResult_;
    bool hasResult_ = false;
};
