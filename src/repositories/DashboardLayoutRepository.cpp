// SPDX-License-Identifier: MIT
#include "repositories/DashboardLayoutRepository.hpp"

#include "db/Database.hpp"
#include "services/DashboardLayoutService.hpp"
#include "util/Date.hpp"

#include <sqlite3.h>

#include <algorithm>

namespace {

std::string textColumn(sqlite3_stmt* statement, int column)
{
    const unsigned char* text = sqlite3_column_text(statement, column);
    return text == nullptr ? std::string() : reinterpret_cast<const char*>(text);
}

DashboardWidget readWidget(sqlite3_stmt* statement)
{
    DashboardWidget widget;
    widget.id = sqlite3_column_int(statement, 0);
    widget.widgetKey = textColumn(statement, 1);
    widget.displayName = textColumn(statement, 2);
    widget.sortOrder = sqlite3_column_int(statement, 3);
    widget.isVisible = sqlite3_column_int(statement, 4) != 0;
    widget.createdAt = textColumn(statement, 5);
    widget.updatedAt = textColumn(statement, 6);
    return widget;
}

}

DashboardLayoutRepository::DashboardLayoutRepository(Database& database)
    : database_(database)
{
}

std::vector<DashboardWidget> DashboardLayoutRepository::listAll(std::string& error) const
{
    error.clear();
    std::vector<DashboardWidget> widgets;

    sqlite3_stmt* statement = nullptr;
    if (!database_.prepare(
            "SELECT id, widget_key, display_name, sort_order, is_visible, created_at, updated_at "
            "FROM dashboard_widgets ORDER BY sort_order ASC, id ASC;",
            &statement)) {
        error = database_.lastError();
        return widgets;
    }

    while (sqlite3_step(statement) == SQLITE_ROW) {
        widgets.push_back(readWidget(statement));
    }

    sqlite3_finalize(statement);
    return widgets;
}

bool DashboardLayoutRepository::ensureDefaults(std::string& error) const
{
    error.clear();

    std::vector<DashboardWidget> widgets = listAll(error);
    if (!error.empty()) {
        return false;
    }

    if (widgets.empty()) {
        return resetToDefaults(error);
    }

    int nextSortOrder = 1;
    for (const DashboardWidget& widget : widgets) {
        nextSortOrder = std::max(nextSortOrder, widget.sortOrder + 1);
    }

    bool hasMissingDefaults = false;
    const std::vector<DashboardWidgetDefinition>& definitions = DashboardLayoutService::defaultDefinitions();
    for (const DashboardWidgetDefinition& definition : definitions) {
        const bool exists = std::any_of(widgets.begin(), widgets.end(), [&definition](const DashboardWidget& widget) {
            return widget.widgetKey == definition.widgetKey;
        });
        if (!exists) {
            DashboardWidget widget;
            widget.widgetKey = definition.widgetKey;
            widget.displayName = definition.displayName;
            widget.sortOrder = nextSortOrder++;
            widget.isVisible = definition.isVisibleByDefault;
            widgets.push_back(widget);
            hasMissingDefaults = true;
        }
    }

    if (!hasMissingDefaults) {
        return true;
    }

    return saveAll(widgets, error);
}

bool DashboardLayoutRepository::saveAll(std::vector<DashboardWidget> widgets, std::string& error) const
{
    error.clear();
    DashboardLayoutService::normalizeSortOrder(widgets);

    if (!database_.execute("BEGIN TRANSACTION;")) {
        error = database_.lastError();
        return false;
    }

    if (!database_.execute("DELETE FROM dashboard_widgets;")) {
        error = database_.lastError();
        database_.execute("ROLLBACK;");
        return false;
    }

    if (!insertWidgets(widgets, error)) {
        database_.execute("ROLLBACK;");
        return false;
    }

    if (!database_.execute("COMMIT;")) {
        error = database_.lastError();
        return false;
    }

    return true;
}

bool DashboardLayoutRepository::resetToDefaults(std::string& error) const
{
    return saveAll(DashboardLayoutService::defaultWidgets(), error);
}

bool DashboardLayoutRepository::insertWidgets(const std::vector<DashboardWidget>& widgets, std::string& error) const
{
    const std::string timestamp = Date::nowIso8601();

    sqlite3_stmt* statement = nullptr;
    if (!database_.prepare(
            "INSERT INTO dashboard_widgets(widget_key, display_name, sort_order, is_visible, created_at, updated_at) "
            "VALUES (?, ?, ?, ?, ?, ?);",
            &statement)) {
        error = database_.lastError();
        return false;
    }

    for (const DashboardWidget& widget : widgets) {
        if (!DashboardLayoutService::isKnownWidgetKey(widget.widgetKey)) {
            continue;
        }

        sqlite3_reset(statement);
        sqlite3_clear_bindings(statement);
        sqlite3_bind_text(statement, 1, widget.widgetKey.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(statement, 2, widget.displayName.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_int(statement, 3, widget.sortOrder);
        sqlite3_bind_int(statement, 4, widget.isVisible ? 1 : 0);
        sqlite3_bind_text(statement, 5, timestamp.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(statement, 6, timestamp.c_str(), -1, SQLITE_TRANSIENT);

        if (sqlite3_step(statement) != SQLITE_DONE) {
            error = sqlite3_errmsg(database_.handle());
            sqlite3_finalize(statement);
            return false;
        }
    }

    sqlite3_finalize(statement);
    return true;
}
