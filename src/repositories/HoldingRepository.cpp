// SPDX-License-Identifier: MIT
#include "repositories/HoldingRepository.hpp"

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

void normalizeTicker(std::string& ticker)
{
    std::transform(ticker.begin(), ticker.end(), ticker.begin(), [](unsigned char value) {
        return static_cast<char>(std::toupper(value));
    });
}

bool isBlank(const std::string& value)
{
    return std::all_of(value.begin(), value.end(), [](unsigned char character) {
        return std::isspace(character) != 0;
    });
}

}

HoldingRepository::HoldingRepository(Database& database)
    : database_(database)
{
}

std::vector<Holding> HoldingRepository::listAll(std::string& error) const
{
    error.clear();
    std::vector<Holding> holdings;

    sqlite3_stmt* statement = nullptr;
    if (!database_.prepare(
            "SELECT id, account_id, ticker, asset_name, asset_type, shares, average_cost, current_price, "
            "notes, created_at, updated_at "
            "FROM holdings ORDER BY ticker COLLATE NOCASE, asset_name COLLATE NOCASE;",
            &statement)) {
        error = database_.lastError();
        return holdings;
    }

    while (sqlite3_step(statement) == SQLITE_ROW) {
        Holding holding;
        holding.id = sqlite3_column_int(statement, 0);
        holding.accountId = sqlite3_column_int(statement, 1);
        holding.ticker = textColumn(statement, 2);
        holding.assetName = textColumn(statement, 3);
        holding.assetType = textColumn(statement, 4);
        holding.shares = sqlite3_column_double(statement, 5);
        holding.averageCost = sqlite3_column_double(statement, 6);
        holding.currentPrice = sqlite3_column_double(statement, 7);
        holding.notes = textColumn(statement, 8);
        holding.createdAt = textColumn(statement, 9);
        holding.updatedAt = textColumn(statement, 10);
        holdings.push_back(holding);
    }

    sqlite3_finalize(statement);
    return holdings;
}

bool HoldingRepository::create(Holding& holding, std::string& error) const
{
    error.clear();
    normalizeTicker(holding.ticker);

    if (!validate(holding, error)) {
        return false;
    }

    const std::string timestamp = Date::nowIso8601();
    holding.createdAt = timestamp;
    holding.updatedAt = timestamp;

    sqlite3_stmt* statement = nullptr;
    if (!database_.prepare(
            "INSERT INTO holdings(account_id, ticker, asset_name, asset_type, shares, average_cost, "
            "current_price, notes, created_at, updated_at) "
            "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?);",
            &statement)) {
        error = database_.lastError();
        return false;
    }

    sqlite3_bind_int(statement, 1, holding.accountId);
    sqlite3_bind_text(statement, 2, holding.ticker.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(statement, 3, holding.assetName.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(statement, 4, holding.assetType.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_double(statement, 5, holding.shares);
    sqlite3_bind_double(statement, 6, holding.averageCost);
    sqlite3_bind_double(statement, 7, holding.currentPrice);
    sqlite3_bind_text(statement, 8, holding.notes.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(statement, 9, holding.createdAt.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(statement, 10, holding.updatedAt.c_str(), -1, SQLITE_TRANSIENT);

    if (sqlite3_step(statement) != SQLITE_DONE) {
        error = sqlite3_errmsg(database_.handle());
        sqlite3_finalize(statement);
        return false;
    }

    sqlite3_finalize(statement);
    holding.id = static_cast<int>(database_.lastInsertRowId());
    return true;
}

bool HoldingRepository::update(const Holding& holding, std::string& error) const
{
    error.clear();
    if (holding.id <= 0) {
        error = "Cannot update a holding without a database id.";
        return false;
    }

    Holding normalizedHolding = holding;
    normalizeTicker(normalizedHolding.ticker);

    if (!validate(normalizedHolding, error)) {
        return false;
    }

    const std::string timestamp = Date::nowIso8601();

    sqlite3_stmt* statement = nullptr;
    if (!database_.prepare(
            "UPDATE holdings SET account_id = ?, ticker = ?, asset_name = ?, asset_type = ?, shares = ?, "
            "average_cost = ?, current_price = ?, notes = ?, updated_at = ? "
            "WHERE id = ?;",
            &statement)) {
        error = database_.lastError();
        return false;
    }

    sqlite3_bind_int(statement, 1, normalizedHolding.accountId);
    sqlite3_bind_text(statement, 2, normalizedHolding.ticker.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(statement, 3, normalizedHolding.assetName.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(statement, 4, normalizedHolding.assetType.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_double(statement, 5, normalizedHolding.shares);
    sqlite3_bind_double(statement, 6, normalizedHolding.averageCost);
    sqlite3_bind_double(statement, 7, normalizedHolding.currentPrice);
    sqlite3_bind_text(statement, 8, normalizedHolding.notes.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(statement, 9, timestamp.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(statement, 10, normalizedHolding.id);

    if (sqlite3_step(statement) != SQLITE_DONE) {
        error = sqlite3_errmsg(database_.handle());
        sqlite3_finalize(statement);
        return false;
    }

    sqlite3_finalize(statement);
    return true;
}

bool HoldingRepository::remove(int id, std::string& error) const
{
    error.clear();

    sqlite3_stmt* statement = nullptr;
    if (!database_.prepare("DELETE FROM holdings WHERE id = ?;", &statement)) {
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

bool HoldingRepository::validate(const Holding& holding, std::string& error)
{
    error.clear();

    if (holding.accountId <= 0) {
        error = "A holding must be assigned to an account.";
        return false;
    }

    if (holding.ticker.empty() || isBlank(holding.ticker)) {
        error = "Ticker is required.";
        return false;
    }

    if (holding.assetName.empty() || isBlank(holding.assetName)) {
        error = "Asset name is required.";
        return false;
    }

    if (holding.assetType.empty() || isBlank(holding.assetType)) {
        error = "Asset type is required.";
        return false;
    }

    if (holding.shares < 0.0 || holding.averageCost < 0.0 || holding.currentPrice < 0.0) {
        error = "Shares, average cost, and current price cannot be negative.";
        return false;
    }

    return true;
}
