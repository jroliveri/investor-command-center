// SPDX-License-Identifier: MIT
#pragma once

#include "models/MarketQuote.hpp"

#include <optional>
#include <string>
#include <unordered_map>

class MarketDataProvider;
class MarketQuoteCacheRepository;

class MarketDataService {
public:
    MarketDataService(MarketDataProvider& provider, MarketQuoteCacheRepository& cacheRepository);

    const char* providerName() const;
    MarketQuoteResult fetchQuote(const std::string& symbol);
    std::optional<MarketQuote> cachedQuote(const std::string& symbol, std::string& error) const;

private:
    static std::string normalizeSymbol(std::string symbol);

    MarketDataProvider& provider_;
    MarketQuoteCacheRepository& cacheRepository_;
    std::unordered_map<std::string, MarketQuote> memoryCache_;
};
