// SPDX-License-Identifier: MIT
#include "services/MarketDataService.hpp"

#include "repositories/MarketQuoteCacheRepository.hpp"
#include "services/MarketDataProvider.hpp"

#include <algorithm>
#include <cctype>

MarketDataService::MarketDataService(MarketDataProvider& provider, MarketQuoteCacheRepository& cacheRepository)
    : provider_(provider)
    , cacheRepository_(cacheRepository)
{
}

const char* MarketDataService::providerName() const
{
    return provider_.providerName();
}

MarketQuoteResult MarketDataService::fetchQuote(const std::string& symbol)
{
    const std::string normalizedSymbol = normalizeSymbol(symbol);
    if (normalizedSymbol.empty()) {
        MarketQuoteResult result;
        result.error = "Ticker is required.";
        return result;
    }

    MarketQuoteResult result = provider_.fetchQuote(normalizedSymbol);
    if (result.success) {
        result.quote.symbol = normalizeSymbol(result.quote.symbol);
        std::string cacheError;
        if (!cacheRepository_.upsert(result.quote, cacheError)) {
            result.error = "Quote fetched, but cache could not be updated: " + cacheError;
        }
        memoryCache_[normalizedSymbol] = result.quote;
        return result;
    }

    if (const auto cached = memoryCache_.find(normalizedSymbol); cached != memoryCache_.end()) {
        MarketQuoteResult cachedResult;
        cachedResult.success = true;
        cachedResult.quote = cached->second;
        cachedResult.fromCache = true;
        cachedResult.staleCache = true;
        cachedResult.error = "Online fetch failed: " + result.error + " Showing the latest in-memory quote.";
        return cachedResult;
    }

    std::string cacheError;
    std::optional<MarketQuote> cached = cacheRepository_.findBySymbol(normalizedSymbol, cacheError);
    if (cached.has_value()) {
        MarketQuoteResult cachedResult;
        cachedResult.success = true;
        cachedResult.quote = *cached;
        cachedResult.fromCache = true;
        cachedResult.staleCache = true;
        cachedResult.error = "Online fetch failed: " + result.error + " Showing the latest cached quote.";
        memoryCache_[normalizedSymbol] = *cached;
        return cachedResult;
    }

    if (!cacheError.empty()) {
        result.error += " Cache lookup also failed: " + cacheError;
    }
    return result;
}

std::optional<MarketQuote> MarketDataService::cachedQuote(const std::string& symbol, std::string& error) const
{
    return cacheRepository_.findBySymbol(normalizeSymbol(symbol), error);
}

std::string MarketDataService::normalizeSymbol(std::string symbol)
{
    symbol.erase(symbol.begin(), std::find_if(symbol.begin(), symbol.end(), [](unsigned char character) {
        return std::isspace(character) == 0;
    }));
    symbol.erase(std::find_if(symbol.rbegin(), symbol.rend(), [](unsigned char character) {
        return std::isspace(character) == 0;
    }).base(), symbol.end());
    std::transform(symbol.begin(), symbol.end(), symbol.begin(), [](unsigned char character) {
        return static_cast<char>(std::toupper(character));
    });
    return symbol;
}
