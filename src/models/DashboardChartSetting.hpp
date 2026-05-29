// SPDX-License-Identifier: MIT
#pragma once

#include <string>

struct DashboardChartSetting {
    int id = 0;
    std::string chartKey;
    std::string dataMode;
    std::string timeRange;
    std::string chartType;
    std::string createdAt;
    std::string updatedAt;
};
