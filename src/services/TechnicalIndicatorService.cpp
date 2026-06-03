// SPDX-License-Identifier: MIT
#include "services/TechnicalIndicatorService.hpp"

#include "models/MarketPriceHistory.hpp"
#include "repositories/MarketPriceHistoryRepository.hpp"
#include "repositories/TechnicalIndicatorCacheRepository.hpp"
#include "util/Date.hpp"

#include <algorithm>
#include <cctype>
#include <numeric>
#include <optional>
#include <vector>

namespace {

constexpr int RsiPeriod = 14;
constexpr int MacdFastPeriod = 12;
constexpr int MacdSlowPeriod = 26;
constexpr int MacdSignalPeriod = 9;

struct MacdCalculation {
    std::optional<double> line;
    std::optional<double> signal;
    std::optional<double> histogram;
};

std::optional<double> calculateRsi(const std::vector<double>& closes)
{
    if (static_cast<int>(closes.size()) <= RsiPeriod) {
        return std::nullopt;
    }

    double averageGain = 0.0;
    double averageLoss = 0.0;
    for (int index = 1; index <= RsiPeriod; ++index) {
        const double change = closes[static_cast<std::size_t>(index)] - closes[static_cast<std::size_t>(index - 1)];
        if (change >= 0.0) {
            averageGain += change;
        } else {
            averageLoss += -change;
        }
    }

    averageGain /= static_cast<double>(RsiPeriod);
    averageLoss /= static_cast<double>(RsiPeriod);

    for (std::size_t index = static_cast<std::size_t>(RsiPeriod + 1); index < closes.size(); ++index) {
        const double change = closes[index] - closes[index - 1];
        const double gain = change > 0.0 ? change : 0.0;
        const double loss = change < 0.0 ? -change : 0.0;
        averageGain = ((averageGain * static_cast<double>(RsiPeriod - 1)) + gain) / static_cast<double>(RsiPeriod);
        averageLoss = ((averageLoss * static_cast<double>(RsiPeriod - 1)) + loss) / static_cast<double>(RsiPeriod);
    }

    if (averageGain == 0.0 && averageLoss == 0.0) {
        return 50.0;
    }
    if (averageLoss == 0.0) {
        return 100.0;
    }

    const double relativeStrength = averageGain / averageLoss;
    return 100.0 - (100.0 / (1.0 + relativeStrength));
}

std::vector<std::optional<double>> calculateEmaSeries(const std::vector<double>& values, int period)
{
    std::vector<std::optional<double>> series(values.size(), std::nullopt);
    if (period <= 0 || static_cast<int>(values.size()) < period) {
        return series;
    }

    const auto seedEnd = values.begin() + period;
    double ema = std::accumulate(values.begin(), seedEnd, 0.0) / static_cast<double>(period);
    series[static_cast<std::size_t>(period - 1)] = ema;

    const double multiplier = 2.0 / (static_cast<double>(period) + 1.0);
    for (std::size_t index = static_cast<std::size_t>(period); index < values.size(); ++index) {
        ema = (values[index] - ema) * multiplier + ema;
        series[index] = ema;
    }

    return series;
}

MacdCalculation calculateMacd(const std::vector<double>& closes)
{
    MacdCalculation calculation;
    if (static_cast<int>(closes.size()) < MacdSlowPeriod + MacdSignalPeriod - 1) {
        return calculation;
    }

    const std::vector<std::optional<double>> fastEma = calculateEmaSeries(closes, MacdFastPeriod);
    const std::vector<std::optional<double>> slowEma = calculateEmaSeries(closes, MacdSlowPeriod);

    std::vector<double> macdValues;
    for (std::size_t index = 0; index < closes.size(); ++index) {
        if (fastEma[index].has_value() && slowEma[index].has_value()) {
            macdValues.push_back(*fastEma[index] - *slowEma[index]);
        }
    }

    if (static_cast<int>(macdValues.size()) < MacdSignalPeriod) {
        return calculation;
    }

    const std::vector<std::optional<double>> signalSeries = calculateEmaSeries(macdValues, MacdSignalPeriod);
    const std::optional<double>& latestSignal = signalSeries.back();
    if (!latestSignal.has_value()) {
        return calculation;
    }

    calculation.line = macdValues.back();
    calculation.signal = *latestSignal;
    calculation.histogram = *calculation.line - *calculation.signal;
    return calculation;
}

std::optional<double> averageTrailing(const std::vector<double>& values, int period)
{
    if (period <= 0 || static_cast<int>(values.size()) < period) {
        return std::nullopt;
    }

    const auto begin = values.end() - period;
    return std::accumulate(begin, values.end(), 0.0) / static_cast<double>(period);
}

TechnicalIndicatorSnapshot calculateSnapshot(const std::string& symbol, const std::string& provider, const std::vector<MarketPriceHistoryRow>& rows)
{
    TechnicalIndicatorSnapshot snapshot;
    snapshot.symbol = symbol;
    snapshot.provider = provider.empty() ? "Yahoo Finance" : provider;
    snapshot.sourceHistoryRows = static_cast<int>(rows.size());
    snapshot.calculatedAt = Date::nowIso8601();
    if (!rows.empty()) {
        snapshot.calculatedForDate = rows.back().priceDate;
    }

    std::vector<double> closes;
    std::vector<double> volumes;
    closes.reserve(rows.size());
    volumes.reserve(rows.size());

    for (const MarketPriceHistoryRow& row : rows) {
        if (row.closePrice.has_value()) {
            closes.push_back(*row.closePrice);
        }
        if (row.volume.has_value()) {
            volumes.push_back(*row.volume);
        }
    }

    snapshot.rsi14 = calculateRsi(closes);
    const MacdCalculation macd = calculateMacd(closes);
    snapshot.macdLine = macd.line;
    snapshot.macdSignal = macd.signal;
    snapshot.macdHistogram = macd.histogram;

    if (!volumes.empty()) {
        snapshot.latestVolume = volumes.back();
    }
    snapshot.avgVolume20 = averageTrailing(volumes, 20);
    snapshot.avgVolume50 = averageTrailing(volumes, 50);
    if (snapshot.latestVolume.has_value() && snapshot.avgVolume20.has_value() && *snapshot.avgVolume20 != 0.0) {
        snapshot.volumeVsAvg20Percent = ((*snapshot.latestVolume - *snapshot.avgVolume20) / *snapshot.avgVolume20) * 100.0;
    }

    return snapshot;
}

} // namespace

TechnicalIndicatorService::TechnicalIndicatorService(MarketPriceHistoryRepository& historyRepository, TechnicalIndicatorCacheRepository& cacheRepository)
    : historyRepository_(historyRepository)
    , cacheRepository_(cacheRepository)
{
}

TechnicalIndicatorResult TechnicalIndicatorService::calculateAndCache(const std::string& symbol, const std::string& provider) const
{
    TechnicalIndicatorResult result;
    const std::string normalizedSymbol = normalizeSymbol(symbol);
    const std::string normalizedProvider = provider.empty() ? "Yahoo Finance" : provider;
    if (normalizedSymbol.empty()) {
        result.error = "Ticker is required.";
        return result;
    }

    std::string historyError;
    const std::vector<MarketPriceHistoryRow> historyRows = historyRepository_.listBySymbol(normalizedSymbol, normalizedProvider, historyError);
    if (historyRows.empty()) {
        std::string cacheError;
        std::optional<TechnicalIndicatorSnapshot> cached = cacheRepository_.findLatestBySymbol(normalizedSymbol, normalizedProvider, cacheError);
        if (cached.has_value()) {
            result.success = true;
            result.snapshot = *cached;
            result.fromCache = true;
            result.staleCache = true;
            result.error = "No local historical rows were available. Showing cached technical indicators.";
            return result;
        }

        result.error = historyError.empty()
            ? "No local historical rows are available for " + normalizedSymbol + ". Refresh history first."
            : historyError;
        if (!cacheError.empty()) {
            result.error += " Cache lookup also failed: " + cacheError;
        }
        return result;
    }

    result.snapshot = calculateSnapshot(normalizedSymbol, normalizedProvider, historyRows);
    if (result.snapshot.calculatedForDate.empty()) {
        result.error = "Historical rows did not include a usable price date for " + normalizedSymbol + ".";
        return result;
    }

    std::string cacheError;
    if (!cacheRepository_.upsert(result.snapshot, cacheError)) {
        result.error = "Technical indicators calculated, but cache could not be updated: " + cacheError;
        return result;
    }

    result.success = true;
    return result;
}

std::optional<TechnicalIndicatorSnapshot> TechnicalIndicatorService::cachedSnapshot(const std::string& symbol, const std::string& provider, std::string& error) const
{
    return cacheRepository_.findLatestBySymbol(normalizeSymbol(symbol), provider.empty() ? "Yahoo Finance" : provider, error);
}

std::string TechnicalIndicatorService::normalizeSymbol(std::string symbol)
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
