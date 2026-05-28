// SPDX-License-Identifier: MIT
#pragma once

#include <string>

struct Goal {
    int id = 0;
    std::string goalName;
    double targetAmount = 0.0;
    double currentAmount = 0.0;
    std::string targetDate;
    std::string category;
    std::string notes;
    std::string createdAt;
    std::string updatedAt;
};
