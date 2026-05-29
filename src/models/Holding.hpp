// SPDX-License-Identifier: MIT
#pragma once

#include <string>

struct Holding {
    int id = 0;
    int accountId = 0;
    std::string ticker;
    std::string assetName;
    std::string assetType = "Stock";
    double shares = 0.0;
    double averageCost = 0.0;
    double currentPrice = 0.0;
    std::string notes;
    std::string status = "Active";
    int lastImportBatchId = 0;
    std::string lastSeenAt;
    std::string createdAt;
    std::string updatedAt;
};
