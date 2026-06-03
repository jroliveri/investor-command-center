// SPDX-License-Identifier: MIT
#pragma once

#include "models/MarketPriceHistory.hpp"
#include "models/MarketQuote.hpp"
#include "models/TechnicalIndicatorSnapshot.hpp"

#include <functional>
#include <string>

class AppState;
class MarketDataService;
class TechnicalIndicatorService;
class WatchlistRepository;

class StockResearchView {
public:
    void render(AppState& state,
        MarketDataService& marketDataService,
        TechnicalIndicatorService& technicalIndicatorService,
        WatchlistRepository& watchlistRepository,
        const std::function<void()>& reloadData);
    void refreshCurrent(MarketDataService& marketDataService, AppState& state);
    void refreshCurrentHistory(MarketDataService& marketDataService, TechnicalIndicatorService& technicalIndicatorService, AppState& state);

private:
    void fetchSymbol(MarketDataService& marketDataService, AppState& state);
    void fetchHistory(MarketDataService& marketDataService, TechnicalIndicatorService& technicalIndicatorService, AppState& state, const std::string& range);
    void clearResult(AppState& state);
    bool addCurrentQuoteToWatchlist(AppState& state, WatchlistRepository& watchlistRepository, const std::function<void()>& reloadData);
    void renderToolbar(AppState& state, MarketDataService& marketDataService, WatchlistRepository& watchlistRepository, const std::function<void()>& reloadData);
    void renderStatusStrip(const MarketDataService& marketDataService);
    void renderQuoteSummary();
    void renderMetrics();
    void renderHistoryPanel(MarketDataService& marketDataService, TechnicalIndicatorService& technicalIndicatorService, AppState& state);
    void renderTechnicalsPanel(TechnicalIndicatorService& technicalIndicatorService);
    void renderWatchlistAction(AppState& state, WatchlistRepository& watchlistRepository, const std::function<void()>& reloadData);

    std::string searchSymbol_;
    MarketQuoteResult lastResult_;
    HistoricalPriceResult lastHistoryResult_;
    TechnicalIndicatorResult lastTechnicalResult_;
    std::string historyRange_ = "1Y";
    bool hasResult_ = false;
    bool historyRequestHasRun_ = false;
    bool technicalRequestHasRun_ = false;
};
