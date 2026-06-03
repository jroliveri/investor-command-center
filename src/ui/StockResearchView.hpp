// SPDX-License-Identifier: MIT
#pragma once

#include "models/MarketPriceHistory.hpp"
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
    void refreshCurrentHistory(MarketDataService& marketDataService, AppState& state);

private:
    void fetchSymbol(MarketDataService& marketDataService, AppState& state);
    void fetchHistory(MarketDataService& marketDataService, AppState& state, const std::string& range);
    void clearResult(AppState& state);
    bool addCurrentQuoteToWatchlist(AppState& state, WatchlistRepository& watchlistRepository, const std::function<void()>& reloadData);
    void renderToolbar(AppState& state, MarketDataService& marketDataService, WatchlistRepository& watchlistRepository, const std::function<void()>& reloadData);
    void renderStatusStrip(const MarketDataService& marketDataService);
    void renderQuoteSummary();
    void renderMetrics();
    void renderHistoryPanel(MarketDataService& marketDataService, AppState& state);
    void renderWatchlistAction(AppState& state, WatchlistRepository& watchlistRepository, const std::function<void()>& reloadData);

    std::string searchSymbol_;
    MarketQuoteResult lastResult_;
    HistoricalPriceResult lastHistoryResult_;
    std::string historyRange_ = "1Y";
    bool hasResult_ = false;
    bool historyRequestHasRun_ = false;
};
