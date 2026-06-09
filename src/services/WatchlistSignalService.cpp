// SPDX-License-Identifier: MIT
#include "services/WatchlistSignalService.hpp"

#include "repositories/WatchlistRepository.hpp"
#include "services/MarketDataService.hpp"
#include "services/TechnicalIndicatorService.hpp"
#include "util/Date.hpp"

#include <algorithm>
#include <cctype>
#include <iomanip>
#include <map>
#include <set>
#include <sstream>
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

std::string formatNumber(double value, int decimals = 1)
{
    std::ostringstream stream;
    stream << std::fixed << std::setprecision(decimals) << value;
    return stream.str();
}

std::string waitingForTechnicalReason(const WatchlistSignalResult& result)
{
    if (!result.hasRsi && !result.hasMacd) {
        return "Hold: price target reached, waiting for RSI/MACD data.";
    }
    if (!result.hasRsi) {
        return "Hold: price target reached, waiting for RSI data.";
    }
    if (!result.hasMacd) {
        return "Hold: price target reached, waiting for MACD data.";
    }
    if (!result.rsiConditionMet) {
        return "Hold: price target reached, waiting for RSI 40-60.";
    }
    if (!result.macdConditionMet) {
        return "Hold: price target reached, RSI OK, MACD histogram not positive.";
    }
    return "Hold: price target reached, waiting for RSI/MACD confirmation.";
}

}

WatchlistSignalResult WatchlistSignalService::calculateSignal(
    double currentPrice,
    double buySignalPrice,
    double sellSignalPrice,
    const std::optional<TechnicalIndicatorSnapshot>& technicalIndicators)
{
    WatchlistSignalResult result;
    result.hasCurrentPrice = currentPrice > 0.0;
    result.priceConditionMet = result.hasCurrentPrice && buySignalPrice > 0.0 && currentPrice <= buySignalPrice;
    result.sellConditionMet = result.hasCurrentPrice && sellSignalPrice > 0.0 && currentPrice >= sellSignalPrice;
    result.hasRsi = technicalIndicators.has_value() && technicalIndicators->rsi14.has_value();
    result.hasMacd = technicalIndicators.has_value() && technicalIndicators->macdHistogram.has_value();
    result.rsiConditionMet = result.hasRsi && *technicalIndicators->rsi14 >= 40.0 && *technicalIndicators->rsi14 <= 60.0;
    result.macdConditionMet = result.hasMacd && *technicalIndicators->macdHistogram > 0.0;

    if (currentPrice <= 0.0) {
        result.reasonText = "Hold: current price unavailable.";
        return result;
    }

    if (result.priceConditionMet && result.rsiConditionMet && result.macdConditionMet) {
        result.signal = "Buy";
        result.reasonText = "Buy: price target reached, RSI " + formatNumber(*technicalIndicators->rsi14) +
            ", MACD histogram positive.";
        return result;
    }

    if (result.sellConditionMet) {
        result.signal = "Sell";
        result.reasonText = "Sell: sell signal price reached.";
        return result;
    }

    if (result.priceConditionMet) {
        result.reasonText = waitingForTechnicalReason(result);
        return result;
    }

    if (buySignalPrice <= 0.0) {
        result.reasonText = "Hold: buy signal price is not set.";
    } else {
        result.reasonText = "Hold: buy price target not reached.";
    }
    return result;
}

WatchlistSignalResult WatchlistSignalService::calculateSignal(const WatchlistItem& item, const std::optional<TechnicalIndicatorSnapshot>& technicalIndicators)
{
    return calculateSignal(item.currentPrice, item.buySignalPrice, item.sellSignalPrice, technicalIndicators);
}

std::string WatchlistSignalService::calculateSignalStatus(double currentPrice, double buySignalPrice, double sellSignalPrice)
{
    return calculateSignal(currentPrice, buySignalPrice, sellSignalPrice, std::nullopt).signal;
}

std::string WatchlistSignalService::calculateSignalStatus(const WatchlistItem& item, const std::optional<TechnicalIndicatorSnapshot>& technicalIndicators)
{
    return calculateSignal(item, technicalIndicators).signal;
}

int WatchlistSignalService::signalSortRank(const std::string& signalStatus)
{
    if (signalStatus == "Buy" || signalStatus == "Buy Signal") {
        return 0;
    }
    if (signalStatus == "Sell" || signalStatus == "Sell Signal") {
        return 1;
    }
    return 2;
}

int WatchlistSignalService::signalSortRank(const WatchlistItem& item)
{
    return signalSortRank(calculateSignalStatus(item.currentPrice, item.buySignalPrice, item.sellSignalPrice));
}

int WatchlistSignalService::signalSortRank(const WatchlistItem& item, const std::optional<TechnicalIndicatorSnapshot>& technicalIndicators)
{
    return signalSortRank(calculateSignalStatus(item, technicalIndicators));
}

bool WatchlistSignalService::hasSignalWarning(double currentPrice, double buySignalPrice, double sellSignalPrice)
{
    if (currentPrice <= 0.0) {
        return false;
    }

    const bool buyTriggered = buySignalPrice > 0.0 && currentPrice <= buySignalPrice;
    const bool sellTriggered = sellSignalPrice > 0.0 && currentPrice >= sellSignalPrice;
    return buyTriggered && sellTriggered;
}

bool WatchlistSignalService::hasSignalWarning(const WatchlistItem& item)
{
    return hasSignalWarning(item.currentPrice, item.buySignalPrice, item.sellSignalPrice);
}

WatchlistPriceRefreshStatus WatchlistSignalService::refreshPrices(
    const std::vector<WatchlistItem>& items,
    MarketDataService& marketDataService,
    TechnicalIndicatorService& technicalIndicatorService,
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
        std::string indicatorError;
        const std::optional<TechnicalIndicatorSnapshot> indicators = technicalIndicatorService.cachedSnapshot(symbol, result.quote.provider.empty() ? marketDataService.providerName() : result.quote.provider, indicatorError);
        updated.signalStatus = calculateSignalStatus(updated, indicators);

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
