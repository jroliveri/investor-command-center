// SPDX-License-Identifier: MIT
#pragma once

#include <string>

struct Account {
    int id = 0;
    std::string accountName;
    std::string accountType = "Brokerage";
    std::string institutionName;
    double currentBalance = 0.0;
    double cashBalance = 0.0;
    std::string notes;
    std::string status = "Active";
    std::string createdAt;
    std::string updatedAt;
};
