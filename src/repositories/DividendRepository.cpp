// SPDX-License-Identifier: MIT
#include "repositories/DividendRepository.hpp"

#include "db/Database.hpp"
#include "util/Date.hpp"

#include <algorithm>
#include <cctype>
#include <sqlite3.h>

namespace {

std::string textColumn(sqlite3_stmt* statement, int column)
{
    const unsigned char* text = sqlite3_column_text(statement, column);
    return text == nullptr ? std::string() : reinterpret_cast<const char*>(text);
}

int nullableIntColumn(sqlite3_stmt* statement, int column)
{
    return sqlite3_column_type(statement, column) == SQLITE_NULL ? 0 : sqlite3_column_int(statement, column);
}

bool isBlank(const std::string& value)
{
    return std::all_of(value.begin(), value.end(), [](unsigned char character) {
        return std::isspace(character) != 0;
    });
}

void normalizeTicker(std::string& ticker)
{
    std::transform(ticker.begin(), ticker.end(), ticker.begin(), [](unsigned char value) {
        return static_cast<char>(std::toupper(value));
    });
}

void bindNullableAccount(sqlite3_stmt* statement, int index, int accountId)
{
    if (accountId > 0) {
        sqlite3_bind_int(statement, index, accountId);
    } else {
        sqlite3_bind_null(statement, index);
    }
}

}

DividendRepository::DividendRepository(Database& database)
    : database_(database)
{
}

std::vector<Dividend> DividendRepository::listAll(std::string& error) const
{
    error.clear();
    std::vector<Dividend> dividends;

    sqlite3_stmt* statement = nullptr;
    if (!database_.prepare(
            "SELECT id, account_id, ticker, asset_name, date_received, amount_received, reinvested, notes, "
            "created_at, updated_at "
            "FROM dividends ORDER BY date_received DESC, id DESC;",
            &statement)) {
        error = database_.lastError();
        return dividends;
    }

    while (sqlite3_step(statement) == SQLITE_ROW) {
        Dividend dividend;
        dividend.id = sqlite3_column_int(statement, 0);
        dividend.accountId = nullableIntColumn(statement, 1);
        dividend.ticker = textColumn(statement, 2);
        dividend.assetName = textColumn(statement, 3);
        dividend.dateReceived = textColumn(statement, 4);
        dividend.amountReceived = sqlite3_column_double(statement, 5);
        dividend.reinvested = sqlite3_column_int(statement, 6) != 0;
        dividend.notes = textColumn(statement, 7);
        dividend.createdAt = textColumn(statement, 8);
        dividend.updatedAt = textColumn(statement, 9);
        dividends.push_back(dividend);
    }

    sqlite3_finalize(statement);
    return dividends;
}

bool DividendRepository::create(Dividend& dividend, std::string& error) const
{
    error.clear();
    normalizeTicker(dividend.ticker);
    if (!validate(dividend, error)) {
        return false;
    }

    const std::string timestamp = Date::nowIso8601();
    dividend.createdAt = timestamp;
    dividend.updatedAt = timestamp;

    sqlite3_stmt* statement = nullptr;
    if (!database_.prepare(
            "INSERT INTO dividends(account_id, ticker, asset_name, date_received, amount_received, reinvested, "
            "notes, created_at, updated_at) "
            "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?);",
            &statement)) {
        error = database_.lastError();
        return false;
    }

    bindNullableAccount(statement, 1, dividend.accountId);
    sqlite3_bind_text(statement, 2, dividend.ticker.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(statement, 3, dividend.assetName.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(statement, 4, dividend.dateReceived.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_double(statement, 5, dividend.amountReceived);
    sqlite3_bind_int(statement, 6, dividend.reinvested ? 1 : 0);
    sqlite3_bind_text(statement, 7, dividend.notes.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(statement, 8, dividend.createdAt.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(statement, 9, dividend.updatedAt.c_str(), -1, SQLITE_TRANSIENT);

    if (sqlite3_step(statement) != SQLITE_DONE) {
        error = sqlite3_errmsg(database_.handle());
        sqlite3_finalize(statement);
        return false;
    }

    sqlite3_finalize(statement);
    dividend.id = static_cast<int>(database_.lastInsertRowId());
    return true;
}

bool DividendRepository::update(const Dividend& dividend, std::string& error) const
{
    error.clear();
    if (dividend.id <= 0) {
        error = "Cannot update a dividend without a database id.";
        return false;
    }

    Dividend normalized = dividend;
    normalizeTicker(normalized.ticker);
    if (!validate(normalized, error)) {
        return false;
    }

    const std::string timestamp = Date::nowIso8601();

    sqlite3_stmt* statement = nullptr;
    if (!database_.prepare(
            "UPDATE dividends SET account_id = ?, ticker = ?, asset_name = ?, date_received = ?, "
            "amount_received = ?, reinvested = ?, notes = ?, updated_at = ? "
            "WHERE id = ?;",
            &statement)) {
        error = database_.lastError();
        return false;
    }

    bindNullableAccount(statement, 1, normalized.accountId);
    sqlite3_bind_text(statement, 2, normalized.ticker.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(statement, 3, normalized.assetName.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(statement, 4, normalized.dateReceived.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_double(statement, 5, normalized.amountReceived);
    sqlite3_bind_int(statement, 6, normalized.reinvested ? 1 : 0);
    sqlite3_bind_text(statement, 7, normalized.notes.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(statement, 8, timestamp.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(statement, 9, normalized.id);

    if (sqlite3_step(statement) != SQLITE_DONE) {
        error = sqlite3_errmsg(database_.handle());
        sqlite3_finalize(statement);
        return false;
    }

    sqlite3_finalize(statement);
    return true;
}

bool DividendRepository::remove(int id, std::string& error) const
{
    error.clear();

    sqlite3_stmt* statement = nullptr;
    if (!database_.prepare("DELETE FROM dividends WHERE id = ?;", &statement)) {
        error = database_.lastError();
        return false;
    }

    sqlite3_bind_int(statement, 1, id);
    if (sqlite3_step(statement) != SQLITE_DONE) {
        error = sqlite3_errmsg(database_.handle());
        sqlite3_finalize(statement);
        return false;
    }

    sqlite3_finalize(statement);
    return true;
}

bool DividendRepository::validate(const Dividend& dividend, std::string& error)
{
    error.clear();

    if (dividend.ticker.empty() || isBlank(dividend.ticker)) {
        error = "Ticker is required.";
        return false;
    }

    if (dividend.dateReceived.empty() || isBlank(dividend.dateReceived)) {
        error = "Date received is required.";
        return false;
    }

    if (!Date::isIsoDate(dividend.dateReceived)) {
        error = "Date received must use YYYY-MM-DD.";
        return false;
    }

    if (dividend.amountReceived < 0.0) {
        error = "Amount received cannot be negative.";
        return false;
    }

    return true;
}
