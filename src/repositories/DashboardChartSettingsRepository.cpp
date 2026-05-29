// SPDX-License-Identifier: MIT
#include "repositories/DashboardChartSettingsRepository.hpp"

#include "db/Database.hpp"
#include "util/Date.hpp"

#include <sqlite3.h>

#include <algorithm>

namespace {

std::string textColumn(sqlite3_stmt* statement, int column)
{
    const unsigned char* text = sqlite3_column_text(statement, column);
    return text == nullptr ? std::string() : reinterpret_cast<const char*>(text);
}

DashboardChartSetting readSetting(sqlite3_stmt* statement)
{
    DashboardChartSetting setting;
    setting.id = sqlite3_column_int(statement, 0);
    setting.chartKey = textColumn(statement, 1);
    setting.dataMode = textColumn(statement, 2);
    setting.timeRange = textColumn(statement, 3);
    setting.chartType = textColumn(statement, 4);
    setting.createdAt = textColumn(statement, 5);
    setting.updatedAt = textColumn(statement, 6);
    return setting;
}

const std::vector<DashboardChartSetting>& defaultSettings()
{
    static const std::vector<DashboardChartSetting> settings = {
        DashboardChartSetting { 0, "allocation_panel", "Asset Type", "Latest", "Allocation Bars" },
        DashboardChartSetting { 0, "performance_panel", "Portfolio Value", "3M", "Line Chart" },
        DashboardChartSetting { 0, "income_gains_panel", "Dividends", "YTD", "Bar Chart" },
    };
    return settings;
}

}

DashboardChartSettingsRepository::DashboardChartSettingsRepository(Database& database)
    : database_(database)
{
}

std::vector<DashboardChartSetting> DashboardChartSettingsRepository::listAll(std::string& error) const
{
    error.clear();
    std::vector<DashboardChartSetting> settings;

    sqlite3_stmt* statement = nullptr;
    if (!database_.prepare(
            "SELECT id, chart_key, data_mode, time_range, chart_type, created_at, updated_at "
            "FROM dashboard_chart_settings ORDER BY chart_key COLLATE NOCASE;",
            &statement)) {
        error = database_.lastError();
        return settings;
    }

    while (sqlite3_step(statement) == SQLITE_ROW) {
        settings.push_back(readSetting(statement));
    }

    sqlite3_finalize(statement);
    return settings;
}

bool DashboardChartSettingsRepository::ensureDefaults(std::string& error) const
{
    error.clear();

    std::vector<DashboardChartSetting> settings = listAll(error);
    if (!error.empty()) {
        return false;
    }

    for (const DashboardChartSetting& defaultSetting : defaultSettings()) {
        const bool exists = std::any_of(settings.begin(), settings.end(), [&defaultSetting](const DashboardChartSetting& setting) {
            return setting.chartKey == defaultSetting.chartKey;
        });
        if (!exists && !save(defaultSetting, error)) {
            return false;
        }
    }

    return true;
}

bool DashboardChartSettingsRepository::save(const DashboardChartSetting& setting, std::string& error) const
{
    error.clear();
    if (setting.chartKey.empty() || setting.dataMode.empty() || setting.timeRange.empty() || setting.chartType.empty()) {
        error = "Dashboard chart setting is incomplete.";
        return false;
    }

    const std::string timestamp = Date::nowIso8601();
    sqlite3_stmt* statement = nullptr;
    if (!database_.prepare(
            "INSERT INTO dashboard_chart_settings(chart_key, data_mode, time_range, chart_type, created_at, updated_at) "
            "VALUES (?, ?, ?, ?, ?, ?) "
            "ON CONFLICT(chart_key) DO UPDATE SET "
            "data_mode = excluded.data_mode, "
            "time_range = excluded.time_range, "
            "chart_type = excluded.chart_type, "
            "updated_at = excluded.updated_at;",
            &statement)) {
        error = database_.lastError();
        return false;
    }

    sqlite3_bind_text(statement, 1, setting.chartKey.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(statement, 2, setting.dataMode.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(statement, 3, setting.timeRange.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(statement, 4, setting.chartType.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(statement, 5, timestamp.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(statement, 6, timestamp.c_str(), -1, SQLITE_TRANSIENT);

    if (sqlite3_step(statement) != SQLITE_DONE) {
        error = sqlite3_errmsg(database_.handle());
        sqlite3_finalize(statement);
        return false;
    }

    sqlite3_finalize(statement);
    return true;
}
