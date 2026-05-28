// SPDX-License-Identifier: MIT
#include "db/Database.hpp"

#include <filesystem>

Database::~Database()
{
    close();
}

bool Database::open(const std::string& path)
{
    close();

    const std::filesystem::path databasePath(path);
    if (databasePath.has_parent_path()) {
        std::error_code error;
        std::filesystem::create_directories(databasePath.parent_path(), error);
        if (error) {
            lastError_ = "Could not create database directory: " + error.message();
            return false;
        }
    }

    if (sqlite3_open(path.c_str(), &db_) != SQLITE_OK) {
        setLastError("Could not open SQLite database.");
        close();
        return false;
    }

    if (!execute("PRAGMA foreign_keys = ON;")) {
        close();
        return false;
    }

    return true;
}

void Database::close()
{
    if (db_ != nullptr) {
        sqlite3_close(db_);
        db_ = nullptr;
    }
}

bool Database::execute(const std::string& sql)
{
    char* errorMessage = nullptr;
    const int result = sqlite3_exec(db_, sql.c_str(), nullptr, nullptr, &errorMessage);
    if (result != SQLITE_OK) {
        lastError_ = errorMessage != nullptr ? errorMessage : "SQLite execution failed.";
        sqlite3_free(errorMessage);
        return false;
    }
    return true;
}

bool Database::prepare(const std::string& sql, sqlite3_stmt** statement)
{
    if (sqlite3_prepare_v2(db_, sql.c_str(), -1, statement, nullptr) != SQLITE_OK) {
        setLastError("Could not prepare SQLite statement.");
        return false;
    }
    return true;
}

sqlite3_int64 Database::lastInsertRowId() const
{
    return sqlite3_last_insert_rowid(db_);
}

void Database::setLastError(const std::string& fallback)
{
    if (db_ != nullptr && sqlite3_errmsg(db_) != nullptr) {
        lastError_ = sqlite3_errmsg(db_);
        return;
    }

    lastError_ = fallback;
}
