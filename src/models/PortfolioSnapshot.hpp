// SPDX-License-Identifier: MIT
#pragma once

#include <string>

struct PortfolioSnapshot {
    int id = 0;
    int accountId = 0;
    int importBatchId = 0;
    std::string snapshotDate;
    std::string source = "Manual";
    double portfolioValue = 0.0;
    double holdingsValue = 0.0;
    double cashBalance = 0.0;
    double costBasis = 0.0;
    double unrealizedGain = 0.0;
    double realizedGainDay = 0.0;
    double realizedGainMonth = 0.0;
    double realizedGainYear = 0.0;
    std::string notes;
    std::string createdAt;
    std::string updatedAt;
};
