// SPDX-License-Identifier: MIT
#pragma once

#include <string>

struct DashboardWidget {
    int id = 0;
    std::string widgetKey;
    std::string displayName;
    int sortOrder = 0;
    bool isVisible = true;
    std::string createdAt;
    std::string updatedAt;
};
