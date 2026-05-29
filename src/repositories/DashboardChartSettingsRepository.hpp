// SPDX-License-Identifier: MIT
#pragma once

#include "models/DashboardChartSetting.hpp"

#include <string>
#include <vector>

class Database;

class DashboardChartSettingsRepository {
public:
    explicit DashboardChartSettingsRepository(Database& database);

    std::vector<DashboardChartSetting> listAll(std::string& error) const;
    bool ensureDefaults(std::string& error) const;
    bool save(const DashboardChartSetting& setting, std::string& error) const;

private:
    Database& database_;
};
