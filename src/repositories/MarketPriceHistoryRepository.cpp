// SPDX-License-Identifier: MIT
#include "repositories/MarketPriceHistoryRepository.hpp"

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

std::optional<double> optionalDoubleColumn(sqlite3_stmt* statement, int column)
{
    if (sqlite3_column_type(statement, column) == SQLITE_NULL) {
        return std::nullopt;
    }
    return sqlite3_column_double(statement, column);
}

void bindOptionalDouble(sqlite3_stmt* statement, int index, const std::optional<double>& value)
{
    if (value.has_value()) {
        sqlite3_bind_double(statement, index, *value);
    } else {
        sqlite3_bind_null(statement, index);
    }
}

std::string normalizeSymbol(std::string symbol)
{
    symbol.erase(symbol.begin(), std::find_if(symbol.begin(), symbol.end(), [](unsigned char character) {
        return std::isspace(character) == 0;
    }));
    symbol.erase(std::find_if(symbol.rbegin(), symbol.rend(), [](unsigned char character) {
        return std::isspace(character) == 0;
    }).base(), symbol.end());
    std::transform(symbol.begin(), symbol.end(), symbol.begin(), [](unsigned char character) {
        return static_cast<char>(std::toupper(character));
    });
    return symbol;
}

MarketPriceHistoryRow readRow(sqlite3_stmt* statement)
{
    MarketPriceHistoryRow row;
    row.id = sqlite3_column_int(statement, 0);
    row.symbol = textColumn(statement, 1);
    row.provider = textColumn(statement, 2);
    row.priceDate = textColumn(statement, 3);
    row.openPrice = optionalDoubleColumn(statement, 4);
    row.highPrice = optionalDoubleColumn(statement, 5);
    row.lowPrice = optionalDoubleColumn(statement, 6);
    row.closePrice = optionalDoubleColumn(statement, 7);
    row.adjustedClosePrice = optionalDoubleColumn(statement, 8);
    row.volume = optionalDoubleColumn(statement, 9);
    row.fetchedAt = textColumn(statement, 10);
    row.createdAt = textColumn(statement, 11);
    row.updatedAt = textColumn(statement, 12);
    return row;
}

}

MarketPriceHistoryRepository::MarketPriceHistoryRepository(Database& database)
    : database_(database)
{
}

std::vector<MarketPriceHistoryRow> MarketPriceHistoryRepository::listBySymbol(const std::string& symbol, const std::string& provider, std::string& error) const
{
    error.clear();
    std::vector<MarketPriceHistoryRow> rows;
    const std::string normalizedSymbol = normalizeSymbol(symbol);
    if (normalizedSymbol.empty()) {
        return rows;
    }

    sqlite3_stmt* statement = nullptr;
    if (!database_.prepare(
            "SELECT id, symbol, provider, price_date, open_price, high_price, low_price, close_price, "
            "adjusted_close_price, volume, fetched_at, created_at, updated_at "
            "FROM market_price_history WHERE symbol = ? AND provider = ? ORDER BY price_date ASC;",
            &statement)) {
        error = database_.lastError();
        return rows;
    }

    sqlite3_bind_text(statement, 1, normalizedSymbol.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(statement, 2, provider.c_str(), -1, SQLITE_TRANSIENT);
    while (sqlite3_step(statement) == SQLITE_ROW) {
        rows.push_back(readRow(statement));
    }

    sqlite3_finalize(statement);
    return rows;
}

bool MarketPriceHistoryRepository::upsertMany(const std::vector<MarketPriceHistoryRow>& rows, int& rowsStored, std::string& error) const
{
    error.clear();
    rowsStored = 0;
    if (rows.empty()) {
        return true;
    }

    if (!database_.execute("BEGIN TRANSACTION;")) {
        error = database_.lastError();
        return false;
    }

    sqlite3_stmt* statement = nullptr;
    if (!database_.prepare(
            "INSERT INTO market_price_history(symbol, provider, price_date, open_price, high_price, low_price, "
            "close_price, adjusted_close_price, volume, fetched_at, created_at, updated_at) "
            "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?) "
            "ON CONFLICT(symbol, provider, price_date) DO UPDATE SET "
            "open_price = excluded.open_price, high_price = excluded.high_price, low_price = excluded.low_price, "
            "close_price = excluded.close_price, adjusted_close_price = excluded.adjusted_close_price, "
            "volume = excluded.volume, fetched_at = excluded.fetched_at, updated_at = excluded.updated_at;",
            &statement)) {
        error = database_.lastError();
        database_.execute("ROLLBACK;");
        return false;
    }

    const std::string timestamp = Date::nowIso8601();
    for (const MarketPriceHistoryRow& row : rows) {
        const std::string symbol = normalizeSymbol(row.symbol);
        if (symbol.empty() || row.priceDate.empty()) {
            continue;
        }

        sqlite3_bind_text(statement, 1, symbol.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(statement, 2, row.provider.empty() ? "Yahoo Finance" : row.provider.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(statement, 3, row.priceDate.c_str(), -1, SQLITE_TRANSIENT);
        bindOptionalDouble(statement, 4, row.openPrice);
        bindOptionalDouble(statement, 5, row.highPrice);
        bindOptionalDouble(statement, 6, row.lowPrice);
        bindOptionalDouble(statement, 7, row.closePrice);
        bindOptionalDouble(statement, 8, row.adjustedClosePrice);
        bindOptionalDouble(statement, 9, row.volume);
        sqlite3_bind_text(statement, 10, row.fetchedAt.empty() ? timestamp.c_str() : row.fetchedAt.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(statement, 11, timestamp.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(statement, 12, timestamp.c_str(), -1, SQLITE_TRANSIENT);

        if (sqlite3_step(statement) != SQLITE_DONE) {
            error = sqlite3_errmsg(database_.handle());
            sqlite3_finalize(statement);
            database_.execute("ROLLBACK;");
            return false;
        }

        ++rowsStored;
        sqlite3_reset(statement);
        sqlite3_clear_bindings(statement);
    }

    sqlite3_finalize(statement);
    if (!database_.execute("COMMIT;")) {
        error = database_.lastError();
        return false;
    }

    return true;
}

