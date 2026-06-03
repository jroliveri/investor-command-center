// SPDX-License-Identifier: MIT
#pragma once

#include "models/MarketPriceHistory.hpp"
#include "models/MarketQuote.hpp"

#include <optional>
#include <string>
#include <unordered_map>

class MarketDataProvider;
class MarketPriceHistoryRepository;
class MarketQuoteCacheRepository;

class MarketDataService {
public:
    MarketDataService(MarketDataProvider& provider, MarketQuoteCacheRepository& cacheRepository, MarketPriceHistoryRepository& historyRepository);

    const char* providerName() const;
    MarketQuoteResult fetchQuote(const std::string& symbol);
    HistoricalPriceResult fetchHistoricalPrices(const std::string& symbol, const std::string& range = "1Y", const std::string& interval = "1d");
    std::optional<MarketQuote> cachedQuote(const std::string& symbol, std::string& error) const;

private:
    static std::string normalizeSymbol(std::string symbol);

    MarketDataProvider& provider_;
    MarketQuoteCacheRepository& cacheRepository_;
    MarketPriceHistoryRepository& historyRepository_;
    std::unordered_map<std::string, MarketQuote> memoryCache_;
};
