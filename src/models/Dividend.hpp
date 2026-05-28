// SPDX-License-Identifier: MIT
#pragma once

#include <string>

struct Dividend {
    int id = 0;
    int accountId = 0;
    std::string ticker;
    std::string assetName;
    std::string dateReceived;
    double amountReceived = 0.0;
    bool reinvested = false;
    std::string notes;
    std::string createdAt;
    std::string updatedAt;
};
