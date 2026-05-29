// SPDX-License-Identifier: MIT
#pragma once

#include <string>

struct Transaction {
    int id = 0;
    int accountId = 0;
    std::string ticker;
    std::string assetName;
    std::string transactionType = "Buy";
    std::string transactionDate;
    double quantity = 0.0;
    double price = 0.0;
    double fees = 0.0;
    double totalAmount = 0.0;
    double soldQuantity = 0.0;
    double salePrice = 0.0;
    double saleProceeds = 0.0;
    double costBasisUsed = 0.0;
    double realizedGainDollar = 0.0;
    double realizedGainPercent = 0.0;
    bool isAdjustment = false;
    std::string notes;
    std::string createdAt;
    std::string updatedAt;
};
