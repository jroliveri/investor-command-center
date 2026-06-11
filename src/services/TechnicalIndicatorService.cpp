// SPDX-License-Identifier: MIT
#include "services/TechnicalIndicatorService.hpp"

#include "models/MarketPriceHistory.hpp"
#include "repositories/MarketPriceHistoryRepository.hpp"
#include "repositories/TechnicalIndicatorCacheRepository.hpp"
#include "services/TechnicalIndicatorSettingsService.hpp"
#include "util/Date.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <numeric>
#include <optional>
#include <string>
#include <vector>

namespace {

struct MacdCalculation {
    std::optional<double> line;
    std::optional<double> signal;
    std::optional<double> histogram;
};

bool isUsablePrice(double value)
{
    return std::isfinite(value) && value > 0.0;
}

std::vector<double> closingPrices(const std::vector<MarketPriceHistoryRow>& rows)
{
    std::vector<double> closes;
    closes.reserve(rows.size());
    for (const MarketPriceHistoryRow& row : rows) {
        if (row.closePrice.has_value() && isUsablePrice(*row.closePrice)) {
            closes.push_back(*row.closePrice);
        }
    }
    return closes;
}

std::vector<double> volumeValues(const std::vector<MarketPriceHistoryRow>& rows)
{
    std::vector<double> volumes;
    volumes.reserve(rows.size());
    for (const MarketPriceHistoryRow& row : rows) {
        if (row.volume.has_value() && std::isfinite(*row.volume) && *row.volume >= 0.0) {
            volumes.push_back(*row.volume);
        }
    }
    return volumes;
}

void appendUnavailableReason(std::string& reason, const std::string& addition)
{
    if (addition.empty()) {
        return;
    }
    if (!reason.empty()) {
        reason += " ";
    }
    reason += addition;
}

std::optional<double> calculateRsi(const std::vector<double>& closes, int period)
{
    if (period <= 1 || static_cast<int>(closes.size()) <= period) {
        return std::nullopt;
    }

    double averageGain = 0.0;
    double averageLoss = 0.0;
    for (int index = 1; index <= period; ++index) {
        const double change = closes[static_cast<std::size_t>(index)] - closes[static_cast<std::size_t>(index - 1)];
        if (change >= 0.0) {
            averageGain += change;
        } else {
            averageLoss += -change;
        }
    }

    averageGain /= static_cast<double>(period);
    averageLoss /= static_cast<double>(period);

    for (std::size_t index = static_cast<std::size_t>(period + 1); index < closes.size(); ++index) {
        const double change = closes[index] - closes[index - 1];
        const double gain = change > 0.0 ? change : 0.0;
        const double loss = change < 0.0 ? -change : 0.0;
        averageGain = ((averageGain * static_cast<double>(period - 1)) + gain) / static_cast<double>(period);
        averageLoss = ((averageLoss * static_cast<double>(period - 1)) + loss) / static_cast<double>(period);
    }

    if (averageGain == 0.0 && averageLoss == 0.0) {
        return 50.0;
    }
    if (averageLoss == 0.0) {
        return 100.0;
    }

    const double relativeStrength = averageGain / averageLoss;
    const double rsi = 100.0 - (100.0 / (1.0 + relativeStrength));
    if (!std::isfinite(rsi)) {
        return std::nullopt;
    }
    return std::clamp(rsi, 0.0, 100.0);
}

std::vector<std::optional<double>> calculateEmaSeries(const std::vector<double>& values, int period)
{
    std::vector<std::optional<double>> series(values.size(), std::nullopt);
    if (period <= 1 || static_cast<int>(values.size()) < period) {
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

MacdCalculation calculateMacd(const std::vector<double>& closes, int fastPeriod, int slowPeriod, int signalPeriod)
{
    MacdCalculation calculation;
    if (fastPeriod <= 1 || slowPeriod <= fastPeriod || signalPeriod <= 1 ||
        static_cast<int>(closes.size()) < slowPeriod + signalPeriod - 1) {
        return calculation;
    }

    const std::vector<std::optional<double>> fastEma = calculateEmaSeries(closes, fastPeriod);
    const std::vector<std::optional<double>> slowEma = calculateEmaSeries(closes, slowPeriod);

    std::vector<double> macdValues;
    for (std::size_t index = 0; index < closes.size(); ++index) {
        if (fastEma[index].has_value() && slowEma[index].has_value()) {
            macdValues.push_back(*fastEma[index] - *slowEma[index]);
        }
    }

    if (static_cast<int>(macdValues.size()) < signalPeriod) {
        return calculation;
    }

    const std::vector<std::optional<double>> signalSeries = calculateEmaSeries(macdValues, signalPeriod);
    const std::optional<double>& latestSignal = signalSeries.back();
    if (!latestSignal.has_value()) {
        return calculation;
    }

    const double line = macdValues.back();
    const double signal = *latestSignal;
    const double histogram = line - signal;
    if (std::isfinite(line) && std::isfinite(signal) && std::isfinite(histogram)) {
        calculation.line = line;
        calculation.signal = signal;
        calculation.histogram = histogram;
    }
    return calculation;
}

std::optional<double> calculateMomentumPercent(const std::vector<double>& closes, int lookbackDays)
{
    if (lookbackDays <= 0 || static_cast<int>(closes.size()) <= lookbackDays) {
        return std::nullopt;
    }

    const double latestClose = closes.back();
    const double comparisonClose = closes[closes.size() - 1 - static_cast<std::size_t>(lookbackDays)];
    if (!isUsablePrice(latestClose) || !isUsablePrice(comparisonClose)) {
        return std::nullopt;
    }

    const double momentum = ((latestClose - comparisonClose) / comparisonClose) * 100.0;
    return std::isfinite(momentum) ? std::optional<double> { momentum } : std::nullopt;
}

std::optional<double> averageTrailing(const std::vector<double>& values, int period)
{
    if (period <= 0 || static_cast<int>(values.size()) < period) {
        return std::nullopt;
    }

    const auto begin = values.end() - period;
    return std::accumulate(begin, values.end(), 0.0) / static_cast<double>(period);
}

std::string classifyRsi(const std::optional<double>& rsi, const TechnicalIndicatorSettings& settings)
{
    if (!rsi.has_value()) {
        return "N/A";
    }
    if (*rsi <= settings.rsiOversoldThreshold) {
        return "Oversold";
    }
    if (*rsi >= settings.rsiOverboughtThreshold) {
        return "Overbought";
    }
    return "Neutral";
}

std::string classifyMacd(const std::optional<double>& histogram)
{
    if (!histogram.has_value()) {
        return "N/A";
    }
    if (*histogram > 0.0) {
        return "Bullish";
    }
    if (*histogram < 0.0) {
        return "Bearish";
    }
    return "Neutral";
}

std::string classifyMomentum(const std::optional<double>& momentumPercent, const TechnicalIndicatorSettings& settings)
{
    if (!momentumPercent.has_value()) {
        return "N/A";
    }
    if (*momentumPercent >= settings.momentumPositiveThresholdPercent) {
        return "Rising";
    }
    if (*momentumPercent <= settings.momentumNegativeThresholdPercent) {
        return "Falling";
    }
    return "Flat";
}

TechnicalIndicatorEvaluation calculateEvaluation(const std::vector<MarketPriceHistoryRow>& rows, const TechnicalIndicatorSettings& settings)
{
    TechnicalIndicatorEvaluation evaluation;
    const std::vector<double> closes = closingPrices(rows);
    if (closes.empty()) {
        evaluation.unavailableReason = "No valid historical close prices are available.";
        return evaluation;
    }

    evaluation.rsi = calculateRsi(closes, settings.rsiPeriod);
    const MacdCalculation macd = calculateMacd(closes, settings.macdFastPeriod, settings.macdSlowPeriod, settings.macdSignalPeriod);
    evaluation.macdLine = macd.line;
    evaluation.macdSignal = macd.signal;
    evaluation.macdHistogram = macd.histogram;
    evaluation.momentumPercent = calculateMomentumPercent(closes, settings.momentumLookbackDays);

    evaluation.rsiClassification = classifyRsi(evaluation.rsi, settings);
    evaluation.macdClassification = classifyMacd(evaluation.macdHistogram);
    evaluation.momentumClassification = classifyMomentum(evaluation.momentumPercent, settings);

    if (!evaluation.rsi.has_value()) {
        appendUnavailableReason(evaluation.unavailableReason, "Insufficient history for RSI period " + std::to_string(settings.rsiPeriod) + ".");
    }
    if (!evaluation.macdLine.has_value() || !evaluation.macdSignal.has_value() || !evaluation.macdHistogram.has_value()) {
        appendUnavailableReason(evaluation.unavailableReason,
            "Insufficient history for MACD periods " + std::to_string(settings.macdFastPeriod) + "/" +
                std::to_string(settings.macdSlowPeriod) + "/" + std::to_string(settings.macdSignalPeriod) + ".");
    }
    if (!evaluation.momentumPercent.has_value()) {
        appendUnavailableReason(evaluation.unavailableReason, "Insufficient history for Momentum lookback " + std::to_string(settings.momentumLookbackDays) + ".");
    }

    return evaluation;
}

TechnicalIndicatorEvaluation evaluateSnapshot(const TechnicalIndicatorSnapshot& snapshot, const TechnicalIndicatorSettings& settings)
{
    TechnicalIndicatorEvaluation evaluation;
    evaluation.rsi = snapshot.rsi14;
    evaluation.macdLine = snapshot.macdLine;
    evaluation.macdSignal = snapshot.macdSignal;
    evaluation.macdHistogram = snapshot.macdHistogram;
    evaluation.rsiClassification = classifyRsi(evaluation.rsi, settings);
    evaluation.macdClassification = classifyMacd(evaluation.macdHistogram);
    evaluation.momentumClassification = "N/A";
    evaluation.unavailableReason = "Momentum requires local historical close prices and is not stored in the technical cache.";
    return evaluation;
}

TechnicalIndicatorSnapshot calculateSnapshot(
    const std::string& symbol,
    const std::string& provider,
    const std::vector<MarketPriceHistoryRow>& rows,
    const TechnicalIndicatorEvaluation& evaluation)
{
    TechnicalIndicatorSnapshot snapshot;
    snapshot.symbol = symbol;
    snapshot.provider = provider.empty() ? "Yahoo Finance" : provider;
    snapshot.sourceHistoryRows = static_cast<int>(rows.size());
    snapshot.calculatedAt = Date::nowIso8601();
    if (!rows.empty()) {
        snapshot.calculatedForDate = rows.back().priceDate;
    }

    snapshot.rsi14 = evaluation.rsi;
    snapshot.macdLine = evaluation.macdLine;
    snapshot.macdSignal = evaluation.macdSignal;
    snapshot.macdHistogram = evaluation.macdHistogram;

    const std::vector<double> volumes = volumeValues(rows);
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
    return calculateAndCache(symbol, provider, TechnicalIndicatorSettings {});
}

TechnicalIndicatorResult TechnicalIndicatorService::calculateAndCache(
    const std::string& symbol,
    const std::string& provider,
    const TechnicalIndicatorSettings& settings) const
{
    TechnicalIndicatorSettings effectiveSettings = settings;
    std::string settingsError;
    if (!TechnicalIndicatorSettingsService::validate(effectiveSettings, settingsError)) {
        effectiveSettings = TechnicalIndicatorSettingsService::defaults();
    }

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
            result.evaluation = evaluateSnapshot(*cached, effectiveSettings);
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

    result.evaluation = calculateEvaluation(historyRows, effectiveSettings);
    result.snapshot = calculateSnapshot(normalizedSymbol, normalizedProvider, historyRows, result.evaluation);
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

TechnicalIndicatorEvaluation TechnicalIndicatorService::evaluate(const std::string& symbol, const std::string& provider, const TechnicalIndicatorSettings& settings) const
{
    TechnicalIndicatorSettings effectiveSettings = settings;
    std::string settingsError;
    if (!TechnicalIndicatorSettingsService::validate(effectiveSettings, settingsError)) {
        effectiveSettings = TechnicalIndicatorSettingsService::defaults();
    }

    const std::string normalizedSymbol = normalizeSymbol(symbol);
    if (normalizedSymbol.empty()) {
        TechnicalIndicatorEvaluation evaluation;
        evaluation.unavailableReason = "Ticker is required.";
        return evaluation;
    }

    std::string historyError;
    const std::vector<MarketPriceHistoryRow> historyRows = historyRepository_.listBySymbol(normalizedSymbol, provider.empty() ? "Yahoo Finance" : provider, historyError);
    if (!historyError.empty()) {
        TechnicalIndicatorEvaluation evaluation;
        evaluation.unavailableReason = historyError;
        return evaluation;
    }

    if (historyRows.empty()) {
        TechnicalIndicatorEvaluation evaluation;
        evaluation.unavailableReason = "No local historical rows are available for " + normalizedSymbol + ". Refresh history first.";
        return evaluation;
    }

    return calculateEvaluation(historyRows, effectiveSettings);
}

std::optional<TechnicalIndicatorSnapshot> TechnicalIndicatorService::cachedSnapshot(const std::string& symbol, const std::string& provider, std::string& error) const
{
    return cacheRepository_.findLatestBySymbol(normalizeSymbol(symbol), provider.empty() ? "Yahoo Finance" : provider, error);
}

std::optional<WatchlistMomentum7D> TechnicalIndicatorService::watchlistMomentum7D(const std::string& symbol, const std::string& provider, std::string& error) const
{
    return watchlistMomentum(symbol, provider, TechnicalIndicatorSettingsDefaults::MomentumLookbackDays, error);
}

std::optional<WatchlistMomentum7D> TechnicalIndicatorService::watchlistMomentum(const std::string& symbol, const std::string& provider, int lookbackSessions, std::string& error) const
{
    error.clear();
    const std::vector<MarketPriceHistoryRow> rows = historyRepository_.listBySymbol(normalizeSymbol(symbol), provider.empty() ? "Yahoo Finance" : provider, error);
    if (!error.empty()) {
        return std::nullopt;
    }
    return WatchlistIndicatorDisplay::calculateMomentum(rows, lookbackSessions);
}

std::optional<WatchlistMomentum7D> TechnicalIndicatorService::watchlistMomentum(
    const std::string& symbol,
    const std::string& provider,
    const TechnicalIndicatorSettings& settings,
    std::string& error) const
{
    TechnicalIndicatorSettings effectiveSettings = settings;
    std::string settingsError;
    if (!TechnicalIndicatorSettingsService::validate(effectiveSettings, settingsError)) {
        effectiveSettings = TechnicalIndicatorSettingsService::defaults();
    }
    return watchlistMomentum(symbol, provider, effectiveSettings.momentumLookbackDays, error);
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
