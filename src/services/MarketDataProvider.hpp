// SPDX-License-Identifier: MIT
#pragma once

#include "models/MarketPriceHistory.hpp"
#include "models/MarketQuote.hpp"

#include <string>
#include <vector>

class MarketDataProvider {
public:
    virtual ~MarketDataProvider() = default;

    virtual const char* providerName() const = 0;
    virtual MarketQuoteResult fetchQuote(const std::string& symbol) = 0;
    virtual std::vector<MarketQuoteResult> fetchQuotes(const std::vector<std::string>& symbols);
    virtual MarketQuoteResult fetchBasicMetrics(const std::string& symbol);
    virtual HistoricalPriceResult fetchHistoricalPrices(const std::string& symbol, const std::string& range, const std::string& interval);
};
