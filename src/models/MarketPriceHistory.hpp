// SPDX-License-Identifier: MIT
#pragma once

#include <optional>
#include <string>
#include <vector>

struct MarketPriceHistoryRow {
    int id = 0;
    std::string symbol;
    std::string provider = "Yahoo Finance";
    std::string priceDate;
    std::optional<double> openPrice;
    std::optional<double> highPrice;
    std::optional<double> lowPrice;
    std::optional<double> closePrice;
    std::optional<double> adjustedClosePrice;
    std::optional<double> volume;
    std::string fetchedAt;
    std::string createdAt;
    std::string updatedAt;
};

struct HistoricalPriceResult {
    bool success = false;
    std::string symbol;
    std::string provider = "Yahoo Finance";
    std::string range = "1Y";
    std::string interval = "1d";
    std::vector<MarketPriceHistoryRow> rows;
    int rowsStored = 0;
    std::string fetchedAt;
    std::string error;
    bool fromCache = false;
    bool staleCache = false;
};
