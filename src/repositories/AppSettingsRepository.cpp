// SPDX-License-Identifier: MIT
#include "repositories/AppSettingsRepository.hpp"

#include "db/Database.hpp"
#include "util/Date.hpp"

#include <sqlite3.h>

AppSettingsRepository::AppSettingsRepository(Database& database)
    : database_(database)
{
}

std::string AppSettingsRepository::getString(const std::string& key, const std::string& fallback, std::string& error) const
{
    error.clear();

    sqlite3_stmt* statement = nullptr;
    if (!database_.prepare("SELECT setting_value FROM app_settings WHERE setting_key = ? LIMIT 1;", &statement)) {
        error = database_.lastError();
        return fallback;
    }

    sqlite3_bind_text(statement, 1, key.c_str(), -1, SQLITE_TRANSIENT);
    const int result = sqlite3_step(statement);
    if (result == SQLITE_ROW) {
        const unsigned char* text = sqlite3_column_text(statement, 0);
        std::string value = text == nullptr ? fallback : reinterpret_cast<const char*>(text);
        sqlite3_finalize(statement);
        return value;
    }

    if (result != SQLITE_DONE) {
        error = sqlite3_errmsg(database_.handle());
    }

    sqlite3_finalize(statement);
    return fallback;
}

bool AppSettingsRepository::setString(const std::string& key, const std::string& value, std::string& error) const
{
    error.clear();
    const std::string timestamp = Date::nowIso8601();

    sqlite3_stmt* statement = nullptr;
    if (!database_.prepare(
            "INSERT INTO app_settings(setting_key, setting_value, created_at, updated_at) "
            "VALUES (?, ?, ?, ?) "
            "ON CONFLICT(setting_key) DO UPDATE SET setting_value = excluded.setting_value, updated_at = excluded.updated_at;",
            &statement)) {
        error = database_.lastError();
        return false;
    }

    sqlite3_bind_text(statement, 1, key.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(statement, 2, value.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(statement, 3, timestamp.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(statement, 4, timestamp.c_str(), -1, SQLITE_TRANSIENT);

    if (sqlite3_step(statement) != SQLITE_DONE) {
        error = sqlite3_errmsg(database_.handle());
        sqlite3_finalize(statement);
        return false;
    }

    sqlite3_finalize(statement);
    return true;
}
