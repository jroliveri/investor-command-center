// SPDX-License-Identifier: MIT
#include "repositories/WatchlistRepository.hpp"

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

}

WatchlistRepository::WatchlistRepository(Database& database)
    : database_(database)
{
}

std::vector<WatchlistItem> WatchlistRepository::listAll(std::string& error) const
{
    error.clear();
    std::vector<WatchlistItem> items;

    sqlite3_stmt* statement = nullptr;
    if (!database_.prepare(
            "SELECT id, ticker, asset_name, asset_type, target_buy_price, buy_signal_price, sell_signal_price, "
            "current_price, last_price_refresh_at, price_source, signal_status, reason_watching, "
            "risk_notes, priority, created_at, updated_at "
            "FROM watchlist ORDER BY "
            "CASE priority WHEN 'High' THEN 0 WHEN 'Medium' THEN 1 WHEN 'Low' THEN 2 ELSE 3 END, "
            "ticker COLLATE NOCASE;",
            &statement)) {
        error = database_.lastError();
        return items;
    }

    while (sqlite3_step(statement) == SQLITE_ROW) {
        WatchlistItem item;
        item.id = sqlite3_column_int(statement, 0);
        item.ticker = textColumn(statement, 1);
        item.assetName = textColumn(statement, 2);
        item.assetType = textColumn(statement, 3);
        item.targetBuyPrice = sqlite3_column_double(statement, 4);
        item.buySignalPrice = sqlite3_column_double(statement, 5);
        item.sellSignalPrice = sqlite3_column_double(statement, 6);
        item.currentPrice = sqlite3_column_double(statement, 7);
        item.lastPriceRefreshAt = textColumn(statement, 8);
        item.priceSource = textColumn(statement, 9);
        item.signalStatus = textColumn(statement, 10);
        item.reasonWatching = textColumn(statement, 11);
        item.riskNotes = textColumn(statement, 12);
        item.priority = textColumn(statement, 13);
        item.createdAt = textColumn(statement, 14);
        item.updatedAt = textColumn(statement, 15);
        if (item.buySignalPrice <= 0.0 && item.targetBuyPrice > 0.0) {
            item.buySignalPrice = item.targetBuyPrice;
        }
        if (item.signalStatus.empty()) {
            item.signalStatus = "None";
        }
        items.push_back(item);
    }

    sqlite3_finalize(statement);
    return items;
}

bool WatchlistRepository::create(WatchlistItem& item, std::string& error) const
{
    error.clear();
    normalizeTicker(item.ticker);
    if (item.buySignalPrice <= 0.0 && item.targetBuyPrice > 0.0) {
        item.buySignalPrice = item.targetBuyPrice;
    }
    item.targetBuyPrice = item.buySignalPrice;
    if (item.signalStatus.empty()) {
        item.signalStatus = "None";
    }
    if (!validate(item, error)) {
        return false;
    }

    const std::string timestamp = Date::nowIso8601();
    item.createdAt = timestamp;
    item.updatedAt = timestamp;

    sqlite3_stmt* statement = nullptr;
    if (!database_.prepare(
            "INSERT INTO watchlist(ticker, asset_name, asset_type, target_buy_price, buy_signal_price, sell_signal_price, "
            "current_price, last_price_refresh_at, price_source, signal_status, reason_watching, risk_notes, priority, "
            "created_at, updated_at) "
            "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?);",
            &statement)) {
        error = database_.lastError();
        return false;
    }

    sqlite3_bind_text(statement, 1, item.ticker.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(statement, 2, item.assetName.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(statement, 3, item.assetType.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_double(statement, 4, item.targetBuyPrice);
    sqlite3_bind_double(statement, 5, item.buySignalPrice);
    sqlite3_bind_double(statement, 6, item.sellSignalPrice);
    sqlite3_bind_double(statement, 7, item.currentPrice);
    sqlite3_bind_text(statement, 8, item.lastPriceRefreshAt.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(statement, 9, item.priceSource.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(statement, 10, item.signalStatus.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(statement, 11, item.reasonWatching.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(statement, 12, item.riskNotes.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(statement, 13, item.priority.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(statement, 14, item.createdAt.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(statement, 15, item.updatedAt.c_str(), -1, SQLITE_TRANSIENT);

    if (sqlite3_step(statement) != SQLITE_DONE) {
        error = sqlite3_errmsg(database_.handle());
        sqlite3_finalize(statement);
        return false;
    }

    sqlite3_finalize(statement);
    item.id = static_cast<int>(database_.lastInsertRowId());
    return true;
}

bool WatchlistRepository::update(const WatchlistItem& item, std::string& error) const
{
    error.clear();
    if (item.id <= 0) {
        error = "Cannot update a watchlist item without a database id.";
        return false;
    }

    WatchlistItem normalized = item;
    normalizeTicker(normalized.ticker);
    if (normalized.buySignalPrice <= 0.0 && normalized.targetBuyPrice > 0.0) {
        normalized.buySignalPrice = normalized.targetBuyPrice;
    }
    normalized.targetBuyPrice = normalized.buySignalPrice;
    if (normalized.signalStatus.empty()) {
        normalized.signalStatus = "None";
    }
    if (!validate(normalized, error)) {
        return false;
    }

    const std::string timestamp = Date::nowIso8601();

    sqlite3_stmt* statement = nullptr;
    if (!database_.prepare(
            "UPDATE watchlist SET ticker = ?, asset_name = ?, asset_type = ?, target_buy_price = ?, "
            "buy_signal_price = ?, sell_signal_price = ?, current_price = ?, last_price_refresh_at = ?, "
            "price_source = ?, signal_status = ?, reason_watching = ?, risk_notes = ?, priority = ?, updated_at = ? "
            "WHERE id = ?;",
            &statement)) {
        error = database_.lastError();
        return false;
    }

    sqlite3_bind_text(statement, 1, normalized.ticker.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(statement, 2, normalized.assetName.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(statement, 3, normalized.assetType.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_double(statement, 4, normalized.targetBuyPrice);
    sqlite3_bind_double(statement, 5, normalized.buySignalPrice);
    sqlite3_bind_double(statement, 6, normalized.sellSignalPrice);
    sqlite3_bind_double(statement, 7, normalized.currentPrice);
    sqlite3_bind_text(statement, 8, normalized.lastPriceRefreshAt.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(statement, 9, normalized.priceSource.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(statement, 10, normalized.signalStatus.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(statement, 11, normalized.reasonWatching.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(statement, 12, normalized.riskNotes.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(statement, 13, normalized.priority.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(statement, 14, timestamp.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(statement, 15, normalized.id);

    if (sqlite3_step(statement) != SQLITE_DONE) {
        error = sqlite3_errmsg(database_.handle());
        sqlite3_finalize(statement);
        return false;
    }

    sqlite3_finalize(statement);
    return true;
}

bool WatchlistRepository::remove(int id, std::string& error) const
{
    error.clear();

    sqlite3_stmt* statement = nullptr;
    if (!database_.prepare("DELETE FROM watchlist WHERE id = ?;", &statement)) {
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

bool WatchlistRepository::validate(const WatchlistItem& item, std::string& error)
{
    error.clear();

    if (item.ticker.empty() || isBlank(item.ticker)) {
        error = "Ticker is required.";
        return false;
    }

    if (item.assetName.empty() || isBlank(item.assetName)) {
        error = "Asset name is required.";
        return false;
    }

    if (item.priority.empty() || isBlank(item.priority)) {
        error = "Priority is required.";
        return false;
    }

    if (item.targetBuyPrice < 0.0 || item.buySignalPrice < 0.0 || item.sellSignalPrice < 0.0 || item.currentPrice < 0.0) {
        error = "Signal prices and current price cannot be negative.";
        return false;
    }

    return true;
}
