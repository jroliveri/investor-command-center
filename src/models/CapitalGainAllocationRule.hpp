// SPDX-License-Identifier: MIT
#pragma once

#include <string>

struct CapitalGainAllocationRule {
    int id = 0;
    std::string name;
    double percentage = 0.0;
    int sortOrder = 0;
    bool isActive = true;
    std::string createdAt;
    std::string updatedAt;
};
