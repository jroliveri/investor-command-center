// SPDX-License-Identifier: MIT
#include "services/WatchlistSignalService.hpp"

#include "repositories/WatchlistRepository.hpp"
#include "services/MarketDataService.hpp"
#include "util/Date.hpp"

#include <algorithm>
#include <cctype>
#include <map>
#include <set>
#include <utility>

namespace {

std::string normalizeTicker(std::string ticker)
{
    ticker.erase(ticker.begin(), std::find_if(ticker.begin(), ticker.end(), [](unsigned char character) {
        return std::isspace(character) == 0;
    }));
    ticker.erase(std::find_if(ticker.rbegin(), ticker.rend(), [](unsigned char character) {
        return std::isspace(character) == 0;
    }).base(), ticker.end());
    std::transform(ticker.begin(), ticker.end(), ticker.begin(), [](unsigned char character) {
        return static_cast<char>(std::toupper(character));
    });
    return ticker;
}

std::string summarizeSymbols(const std::vector<std::string>& symbols)
{
    if (symbols.empty()) {
        return {};
    }

    std::string summary = "Failed symbols:";
    const int limit = std::min<int>(6, static_cast<int>(symbols.size()));
    for (int index = 0; index < limit; ++index) {
        summary += index == 0 ? " " : ", ";
        summary += symbols[static_cast<std::size_t>(index)];
    }
    if (static_cast<int>(symbols.size()) > limit) {
        summary += ", ...";
    }
    return summary;
}

void appendWarning(std::string& warning, const std::string& addition)
{
    if (addition.empty()) {
        return;
    }

    if (!warning.empty()) {
        warning += " ";
    }
    warning += addition;
}

}

std::string WatchlistSignalService::calculateSignalStatus(double currentPrice, double buySignalPrice, double sellSignalPrice)
{
    if (currentPrice <= 0.0) {
        return "No Price";
    }

    const bool buyTriggered = buySignalPrice > 0.0 && currentPrice <= buySignalPrice;
    const bool sellTriggered = sellSignalPrice > 0.0 && currentPrice >= sellSignalPrice;
    if (buyTriggered && sellTriggered) {
        return "Check Signals";
    }
    if (buyTriggered) {
        return "Buy Signal";
    }
    if (sellTriggered) {
        return "Sell Signal";
    }
    return "No Signal";
}

WatchlistPriceRefreshStatus WatchlistSignalService::refreshPrices(
    const std::vector<WatchlistItem>& items,
    MarketDataService& marketDataService,
    WatchlistRepository& repository,
    std::string& error)
{
    error.clear();

    WatchlistPriceRefreshStatus status;
    status.hasRun = true;
    status.provider = marketDataService.providerName();
    status.lastRefreshedAt = Date::nowIso8601();

    std::vector<std::string> failedSymbols;
    std::map<std::string, MarketQuoteResult> quoteResults;
    std::set<std::string> requestedSymbols;

    for (const WatchlistItem& item : items) {
        const std::string symbol = normalizeTicker(item.ticker);
        if (!symbol.empty()) {
            requestedSymbols.insert(symbol);
        }
    }

    if (requestedSymbols.empty()) {
        status.warning = "No watchlist tickers were available to refresh.";
        error = status.warning;
        return status;
    }

    for (const std::string& symbol : requestedSymbols) {
        MarketQuoteResult result = marketDataService.fetchQuote(symbol);
        if (!result.success || !result.quote.currentPrice.has_value()) {
            failedSymbols.push_back(symbol);
        }
        quoteResults.emplace(symbol, std::move(result));
    }

    for (const WatchlistItem& item : items) {
        const std::string symbol = normalizeTicker(item.ticker);
        if (symbol.empty()) {
            continue;
        }

        const auto resultIterator = quoteResults.find(symbol);
        if (resultIterator == quoteResults.end()) {
            continue;
        }

        const MarketQuoteResult& result = resultIterator->second;
        if (!result.success || !result.quote.currentPrice.has_value()) {
            continue;
        }

        WatchlistItem updated = item;
        updated.ticker = symbol;
        updated.currentPrice = *result.quote.currentPrice;
        updated.lastPriceRefreshAt = result.quote.fetchedAt.empty() ? status.lastRefreshedAt : result.quote.fetchedAt;
        updated.priceSource = result.fromCache ? "Cached Yahoo Finance" : "Yahoo Finance";
        updated.signalStatus = calculateSignalStatus(updated.currentPrice, updated.buySignalPrice, updated.sellSignalPrice);

        std::string updateError;
        if (repository.update(updated, updateError)) {
            ++status.refreshedSymbols;
            if (result.fromCache) {
                ++status.cachedSymbols;
            }
        } else {
            failedSymbols.push_back(symbol);
            appendWarning(status.warning, "Could not update " + symbol + ": " + updateError);
        }
    }

    status.failedSymbols = static_cast<int>(failedSymbols.size());
    if (status.cachedSymbols > 0) {
        appendWarning(status.warning, "Some watchlist prices are using cached Yahoo Finance quotes because live data was unavailable.");
    }
    appendWarning(status.warning, summarizeSymbols(failedSymbols));

    if (status.refreshedSymbols == 0 && status.failedSymbols > 0) {
        error = status.warning.empty() ? "Watchlist price refresh failed." : status.warning;
    }

    return status;
}
