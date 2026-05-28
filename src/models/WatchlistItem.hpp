// SPDX-License-Identifier: MIT
#pragma once

#include <string>

struct WatchlistItem {
    int id = 0;
    std::string ticker;
    std::string assetName;
    std::string assetType = "Stock";
    double targetBuyPrice = 0.0;
    double currentPrice = 0.0;
    std::string reasonWatching;
    std::string riskNotes;
    std::string priority = "Medium";
    std::string createdAt;
    std::string updatedAt;
};
