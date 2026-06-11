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

struct TechnicalIndicatorEvaluation {
    std::optional<double> rsi;
    std::optional<double> macdLine;
    std::optional<double> macdSignal;
    std::optional<double> macdHistogram;
    std::optional<double> momentumPercent;
    std::string rsiClassification = "N/A";
    std::string macdClassification = "N/A";
    std::string momentumClassification = "N/A";
    std::string unavailableReason;
};

struct TechnicalIndicatorResult {
    bool success = false;
    TechnicalIndicatorSnapshot snapshot;
    TechnicalIndicatorEvaluation evaluation;
    std::string error;
    bool fromCache = false;
    bool staleCache = false;
};
