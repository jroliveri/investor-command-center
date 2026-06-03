// SPDX-License-Identifier: MIT
#pragma once

#include <string>

struct WatchlistItem {
    int id = 0;
    int watchlistId = 0;
    std::string ticker;
    std::string assetName;
    std::string assetType = "Stock";
    double targetBuyPrice = 0.0;
    double buySignalPrice = 0.0;
    double sellSignalPrice = 0.0;
    double currentPrice = 0.0;
    std::string lastPriceRefreshAt;
    std::string priceSource;
    std::string signalStatus = "Hold";
    std::string reasonWatching;
    std::string riskNotes;
    std::string priority = "Medium";
    std::string createdAt;
    std::string updatedAt;
};
