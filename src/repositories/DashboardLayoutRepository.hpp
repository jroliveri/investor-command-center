// SPDX-License-Identifier: MIT
#pragma once

#include "models/DashboardWidget.hpp"

#include <string>
#include <vector>

class Database;

class DashboardLayoutRepository {
public:
    explicit DashboardLayoutRepository(Database& database);

    std::vector<DashboardWidget> listAll(std::string& error) const;
    bool ensureDefaults(std::string& error) const;
    bool saveAll(std::vector<DashboardWidget> widgets, std::string& error) const;
    bool resetToDefaults(std::string& error) const;

private:
    bool insertWidgets(const std::vector<DashboardWidget>& widgets, std::string& error) const;

    Database& database_;
};
