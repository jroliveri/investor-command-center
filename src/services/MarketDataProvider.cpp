// SPDX-License-Identifier: MIT
#include "services/MarketDataProvider.hpp"

std::vector<MarketQuoteResult> MarketDataProvider::fetchQuotes(const std::vector<std::string>& symbols)
{
    std::vector<MarketQuoteResult> results;
    results.reserve(symbols.size());
    for (const std::string& symbol : symbols) {
        results.push_back(fetchQuote(symbol));
    }
    return results;
}

MarketQuoteResult MarketDataProvider::fetchBasicMetrics(const std::string& symbol)
{
    return fetchQuote(symbol);
}

HistoricalPriceResult MarketDataProvider::fetchHistoricalPrices(const std::string& symbol, const std::string& range, const std::string& interval)
{
    HistoricalPriceResult result;
    result.success = false;
    result.symbol = symbol;
    result.range = range;
    result.interval = interval;
    result.error = "Historical prices are planned. Requested range " + range + " and interval " + interval + ".";
    return result;
}
