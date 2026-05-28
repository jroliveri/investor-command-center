// SPDX-License-Identifier: MIT
#pragma once

#include <sqlite3.h>

#include <string>

class Database {
public:
    Database() = default;
    ~Database();

    Database(const Database&) = delete;
    Database& operator=(const Database&) = delete;

    bool open(const std::string& path);
    void close();

    bool execute(const std::string& sql);
    bool prepare(const std::string& sql, sqlite3_stmt** statement);

    sqlite3* handle() const { return db_; }
    const std::string& lastError() const { return lastError_; }
    sqlite3_int64 lastInsertRowId() const;

private:
    void setLastError(const std::string& fallback);

    sqlite3* db_ = nullptr;
    std::string lastError_;
};
