// SPDX-License-Identifier: MIT
#pragma once

#include <optional>
#include <string>

struct TechnicalIndicatorSnapshot {
    int id = 0;
    std::string symbol;
    std::string provider = "Yahoo Finance";
    std::string calculatedForDate;
    std::optional<double> rsi14;
    std::optional<double> macdLine;
    std::optional<double> macdSignal;
    std::optional<double> macdHistogram;
    std::optional<double> latestVolume;
    std::optional<double> avgVolume20;
    std::optional<double> avgVolume50;
    std::optional<double> volumeVsAvg20Percent;
    int sourceHistoryRows = 0;
    std::string calculatedAt;
    std::string createdAt;
    std::string updatedAt;
};

struct TechnicalIndicatorResult {
    bool success = false;
    TechnicalIndicatorSnapshot snapshot;
    std::string error;
    bool fromCache = false;
    bool staleCache = false;
};
