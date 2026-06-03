// SPDX-License-Identifier: MIT
#pragma once

#include "models/TechnicalIndicatorSnapshot.hpp"

#include <optional>
#include <string>

class MarketPriceHistoryRepository;
class TechnicalIndicatorCacheRepository;

class TechnicalIndicatorService {
public:
    TechnicalIndicatorService(MarketPriceHistoryRepository& historyRepository, TechnicalIndicatorCacheRepository& cacheRepository);

    TechnicalIndicatorResult calculateAndCache(const std::string& symbol, const std::string& provider = "Yahoo Finance") const;
    std::optional<TechnicalIndicatorSnapshot> cachedSnapshot(const std::string& symbol, const std::string& provider, std::string& error) const;

private:
    static std::string normalizeSymbol(std::string symbol);

    MarketPriceHistoryRepository& historyRepository_;
    TechnicalIndicatorCacheRepository& cacheRepository_;
};
