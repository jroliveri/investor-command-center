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
    double totalAmount = 0.0;
    std::string notes;
    std::string createdAt;
    std::string updatedAt;
};
