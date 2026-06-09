// SPDX-License-Identifier: MIT
#include "services/WatchlistIndicatorDisplay.hpp"

#include "util/Money.hpp"

#include <cmath>
#include <vector>

namespace {

constexpr int MomentumLookbackTradingSessions = 7;
constexpr double FlatMomentumEpsilon = 0.000001;

std::vector<double> closingPrices(const std::vector<MarketPriceHistoryRow>& rows)
{
    std::vector<double> closes;
    closes.reserve(rows.size());
    for (const MarketPriceHistoryRow& row : rows) {
        if (row.closePrice.has_value() && *row.closePrice > 0.0) {
            closes.push_back(*row.closePrice);
        }
    }
    return closes;
}

} // namespace

namespace WatchlistIndicatorDisplay {

std::optional<WatchlistMomentum7D> calculateMomentum(const std::vector<MarketPriceHistoryRow>& rows, int lookbackSessions)
{
    if (lookbackSessions <= 0) {
        return std::nullopt;
    }

    const std::vector<double> closes = closingPrices(rows);
    if (static_cast<int>(closes.size()) <= lookbackSessions) {
        return std::nullopt;
    }

    const double latestClose = closes.back();
    const double comparisonClose = closes[closes.size() - 1 - static_cast<std::size_t>(lookbackSessions)];
    if (comparisonClose <= 0.0) {
        return std::nullopt;
    }

    WatchlistMomentum7D momentum;
    momentum.latestClose = latestClose;
    momentum.comparisonClose = comparisonClose;
    momentum.percent = ((latestClose - comparisonClose) / comparisonClose) * 100.0;
    momentum.flat = std::fabs(latestClose - comparisonClose) <= FlatMomentumEpsilon;
    momentum.rising = !momentum.flat && latestClose > comparisonClose;
    return momentum;
}

std::optional<WatchlistMomentum7D> calculateMomentum7D(const std::vector<MarketPriceHistoryRow>& rows)
{
    return calculateMomentum(rows, MomentumLookbackTradingSessions);
}

std::string rsiDisplayText(const std::optional<TechnicalIndicatorSnapshot>& snapshot)
{
    if (!snapshot.has_value() || !snapshot->rsi14.has_value()) {
        return "N/A";
    }
    return Money::formatNumber(*snapshot->rsi14, 1);
}

std::string macdSignalDisplayText(const std::optional<TechnicalIndicatorSnapshot>& snapshot)
{
    if (!snapshot.has_value() || !snapshot->macdSignal.has_value()) {
        return "N/A";
    }
    return Money::formatNumber(*snapshot->macdSignal, 4);
}

std::string macdLineHistogramDisplayText(const std::optional<TechnicalIndicatorSnapshot>& snapshot)
{
    if (!snapshot.has_value() || !snapshot->macdLine.has_value() || !snapshot->macdHistogram.has_value()) {
        return "N/A";
    }

    return "Line " + Money::formatNumber(*snapshot->macdLine, 3) + " / Hist " + Money::formatNumber(*snapshot->macdHistogram, 3);
}

std::string momentumDisplayText(const std::optional<WatchlistMomentum7D>& momentum)
{
    if (!momentum.has_value()) {
        return "—";
    }

    const char* label = momentum->flat ? "Flat" : (momentum->rising ? "Rising" : "Falling");
    return std::string(label) + " " + Money::formatPercent(momentum->percent, true);
}

}
