// SPDX-License-Identifier: MIT
#pragma once

#include <string>

class Database;

class AppSettingsRepository {
public:
    explicit AppSettingsRepository(Database& database);

    std::string getString(const std::string& key, const std::string& fallback, std::string& error) const;
    bool setString(const std::string& key, const std::string& value, std::string& error) const;

private:
    Database& database_;
};
