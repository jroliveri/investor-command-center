// SPDX-License-Identifier: MIT
#pragma once

#include "models/MarketPriceHistory.hpp"
#include "models/TechnicalIndicatorSnapshot.hpp"

#include <optional>
#include <string>
#include <vector>

struct WatchlistMomentum7D {
    double latestClose = 0.0;
    double comparisonClose = 0.0;
    double percent = 0.0;
    bool rising = false;
    bool flat = false;
};

namespace WatchlistIndicatorDisplay {
std::optional<WatchlistMomentum7D> calculateMomentum(const std::vector<MarketPriceHistoryRow>& rows, int lookbackSessions);
std::optional<WatchlistMomentum7D> calculateMomentum7D(const std::vector<MarketPriceHistoryRow>& rows);
std::string rsiDisplayText(const std::optional<TechnicalIndicatorSnapshot>& snapshot);
std::string macdSignalDisplayText(const std::optional<TechnicalIndicatorSnapshot>& snapshot);
std::string macdLineHistogramDisplayText(const std::optional<TechnicalIndicatorSnapshot>& snapshot);
std::string momentumDisplayText(const std::optional<WatchlistMomentum7D>& momentum);
}
