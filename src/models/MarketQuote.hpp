// SPDX-License-Identifier: MIT
#pragma once

#include <optional>
#include <string>

struct MarketQuote {
    int id = 0;
    std::string symbol;
    std::string provider = "Yahoo Finance";
    std::string companyName;
    std::optional<double> currentPrice;
    std::optional<double> priceChangeDollar;
    std::optional<double> priceChangePercent;
    std::optional<double> previousClose;
    std::optional<double> openPrice;
    std::optional<double> dayHigh;
    std::optional<double> dayLow;
    std::optional<double> fiftyTwoWeekHigh;
    std::optional<double> fiftyTwoWeekLow;
    std::optional<double> marketCap;
    std::optional<double> volume;
    std::optional<double> averageVolume;
    std::optional<double> peRatio;
    std::optional<double> eps;
    std::optional<double> dividendYield;
    std::optional<double> beta;
    std::string currency;
    std::string exchangeName;
    std::string quoteTime;
    std::string fetchedAt;
    std::string rawStatus;
    std::string createdAt;
    std::string updatedAt;
};

struct MarketQuoteResult {
    bool success = false;
    MarketQuote quote;
    std::string error;
    bool fromCache = false;
    bool staleCache = false;
};
