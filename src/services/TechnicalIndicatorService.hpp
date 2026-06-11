// SPDX-License-Identifier: MIT
#pragma once

#include "models/TechnicalIndicatorSettings.hpp"
#include "models/TechnicalIndicatorSnapshot.hpp"
#include "services/WatchlistIndicatorDisplay.hpp"

#include <optional>
#include <string>

class MarketPriceHistoryRepository;
class TechnicalIndicatorCacheRepository;

class TechnicalIndicatorService {
public:
    TechnicalIndicatorService(MarketPriceHistoryRepository& historyRepository, TechnicalIndicatorCacheRepository& cacheRepository);

    TechnicalIndicatorResult calculateAndCache(const std::string& symbol, const std::string& provider = "Yahoo Finance") const;
    TechnicalIndicatorResult calculateAndCache(const std::string& symbol, const std::string& provider, const TechnicalIndicatorSettings& settings) const;
    TechnicalIndicatorEvaluation evaluate(const std::string& symbol, const std::string& provider, const TechnicalIndicatorSettings& settings) const;
    std::optional<TechnicalIndicatorSnapshot> cachedSnapshot(const std::string& symbol, const std::string& provider, std::string& error) const;
    std::optional<WatchlistMomentum7D> watchlistMomentum(const std::string& symbol, const std::string& provider, int lookbackSessions, std::string& error) const;
    std::optional<WatchlistMomentum7D> watchlistMomentum(const std::string& symbol, const std::string& provider, const TechnicalIndicatorSettings& settings, std::string& error) const;
    std::optional<WatchlistMomentum7D> watchlistMomentum7D(const std::string& symbol, const std::string& provider, std::string& error) const;

private:
    static std::string normalizeSymbol(std::string symbol);

    MarketPriceHistoryRepository& historyRepository_;
    TechnicalIndicatorCacheRepository& cacheRepository_;
};
