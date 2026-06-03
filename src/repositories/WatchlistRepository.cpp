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

std::string normalizeSignalStatus(const std::string& signalStatus)
{
    if (signalStatus == "Buy" || signalStatus == "Buy Signal") {
        return "Buy";
    }
    if (signalStatus == "Sell" || signalStatus == "Sell Signal") {
        return "Sell";
    }
    return "Hold";
}

Watchlist mapWatchlist(sqlite3_stmt* statement)
{
    Watchlist watchlist;
    watchlist.id = sqlite3_column_int(statement, 0);
    watchlist.name = textColumn(statement, 1);
    watchlist.description = textColumn(statement, 2);
    watchlist.sortOrder = sqlite3_column_int(statement, 3);
    watchlist.isActive = sqlite3_column_int(statement, 4) != 0;
    watchlist.showInSidebar = sqlite3_column_int(statement, 5) != 0;
    watchlist.sidebarSlot = sqlite3_column_int(statement, 6);
    watchlist.createdAt = textColumn(statement, 7);
    watchlist.updatedAt = textColumn(statement, 8);
    return watchlist;
}

WatchlistItem mapWatchlistItem(sqlite3_stmt* statement)
{
    WatchlistItem item;
    item.id = sqlite3_column_int(statement, 0);
    item.watchlistId = sqlite3_column_int(statement, 1);
    item.ticker = textColumn(statement, 2);
    item.assetName = textColumn(statement, 3);
    item.assetType = textColumn(statement, 4);
    item.targetBuyPrice = sqlite3_column_double(statement, 5);
    item.buySignalPrice = sqlite3_column_double(statement, 6);
    item.sellSignalPrice = sqlite3_column_double(statement, 7);
    item.currentPrice = sqlite3_column_double(statement, 8);
    item.lastPriceRefreshAt = textColumn(statement, 9);
    item.priceSource = textColumn(statement, 10);
    item.signalStatus = textColumn(statement, 11);
    item.reasonWatching = textColumn(statement, 12);
    item.riskNotes = textColumn(statement, 13);
    item.priority = textColumn(statement, 14);
    item.createdAt = textColumn(statement, 15);
    item.updatedAt = textColumn(statement, 16);

    if (item.buySignalPrice <= 0.0 && item.targetBuyPrice > 0.0) {
        item.buySignalPrice = item.targetBuyPrice;
    }
    item.signalStatus = normalizeSignalStatus(item.signalStatus);
    return item;
}

constexpr const char* WatchlistItemSelectSql = R"sql(
SELECT w.id, COALESCE(w.watchlist_id, 0), w.ticker, w.asset_name, w.asset_type, w.target_buy_price,
       w.buy_signal_price, w.sell_signal_price, w.current_price, w.last_price_refresh_at,
       w.price_source, w.signal_status, w.reason_watching, w.risk_notes, w.priority,
       w.created_at, w.updated_at
FROM watchlist w
)sql";

constexpr const char* WatchlistItemOrderSql = R"sql(
ORDER BY CASE w.priority WHEN 'High' THEN 0 WHEN 'Medium' THEN 1 WHEN 'Low' THEN 2 ELSE 3 END,
         w.ticker COLLATE NOCASE
)sql";

}

WatchlistRepository::WatchlistRepository(Database& database)
    : database_(database)
{
}

std::vector<Watchlist> WatchlistRepository::listWatchlists(bool includeInactive, std::string& error) const
{
    error.clear();
    std::vector<Watchlist> watchlists;

    sqlite3_stmt* statement = nullptr;
    const char* sql = includeInactive
        ? "SELECT id, name, description, sort_order, is_active, show_in_sidebar, sidebar_slot, created_at, updated_at "
          "FROM watchlists ORDER BY is_active DESC, sort_order, id;"
        : "SELECT id, name, description, sort_order, is_active, show_in_sidebar, sidebar_slot, created_at, updated_at "
          "FROM watchlists WHERE is_active = 1 ORDER BY sort_order, id;";
    if (!database_.prepare(sql, &statement)) {
        error = database_.lastError();
        return watchlists;
    }

    while (sqlite3_step(statement) == SQLITE_ROW) {
        watchlists.push_back(mapWatchlist(statement));
    }

    sqlite3_finalize(statement);
    return watchlists;
}

bool WatchlistRepository::createWatchlist(Watchlist& watchlist, std::string& error) const
{
    error.clear();
    if (!validateWatchlist(watchlist, error)) {
        return false;
    }

    const std::string timestamp = Date::nowIso8601();
    watchlist.createdAt = timestamp;
    watchlist.updatedAt = timestamp;
    if (!watchlist.isActive || !watchlist.showInSidebar || watchlist.sidebarSlot < 1 || watchlist.sidebarSlot > 2) {
        watchlist.showInSidebar = false;
        watchlist.sidebarSlot = 0;
    }

    sqlite3_stmt* statement = nullptr;
    if (!database_.prepare(
            "INSERT INTO watchlists(name, description, sort_order, is_active, show_in_sidebar, sidebar_slot, created_at, updated_at) "
            "VALUES (?, ?, ?, ?, ?, ?, ?, ?);",
            &statement)) {
        error = database_.lastError();
        return false;
    }

    sqlite3_bind_text(statement, 1, watchlist.name.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(statement, 2, watchlist.description.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(statement, 3, watchlist.sortOrder);
    sqlite3_bind_int(statement, 4, watchlist.isActive ? 1 : 0);
    sqlite3_bind_int(statement, 5, watchlist.showInSidebar ? 1 : 0);
    sqlite3_bind_int(statement, 6, watchlist.sidebarSlot);
    sqlite3_bind_text(statement, 7, watchlist.createdAt.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(statement, 8, watchlist.updatedAt.c_str(), -1, SQLITE_TRANSIENT);

    if (sqlite3_step(statement) != SQLITE_DONE) {
        error = sqlite3_errmsg(database_.handle());
        sqlite3_finalize(statement);
        return false;
    }

    sqlite3_finalize(statement);
    watchlist.id = static_cast<int>(database_.lastInsertRowId());
    if (watchlist.showInSidebar) {
        sqlite3_stmt* clearStatement = nullptr;
        if (!database_.prepare(
                "UPDATE watchlists SET show_in_sidebar = 0, sidebar_slot = 0, updated_at = ? "
                "WHERE id <> ? AND sidebar_slot = ?;",
                &clearStatement)) {
            error = database_.lastError();
            return false;
        }

        sqlite3_bind_text(clearStatement, 1, timestamp.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_int(clearStatement, 2, watchlist.id);
        sqlite3_bind_int(clearStatement, 3, watchlist.sidebarSlot);
        if (sqlite3_step(clearStatement) != SQLITE_DONE) {
            error = sqlite3_errmsg(database_.handle());
            sqlite3_finalize(clearStatement);
            return false;
        }
        sqlite3_finalize(clearStatement);
    }
    return true;
}

bool WatchlistRepository::updateWatchlist(const Watchlist& watchlist, std::string& error) const
{
    error.clear();
    if (watchlist.id <= 0) {
        error = "Cannot update a watchlist without a database id.";
        return false;
    }
    if (!validateWatchlist(watchlist, error)) {
        return false;
    }

    Watchlist normalized = watchlist;
    if (!normalized.isActive || !normalized.showInSidebar || normalized.sidebarSlot < 1 || normalized.sidebarSlot > 2) {
        normalized.showInSidebar = false;
        normalized.sidebarSlot = 0;
    }

    const std::string timestamp = Date::nowIso8601();
    sqlite3_stmt* statement = nullptr;
    if (!database_.prepare(
            "UPDATE watchlists SET name = ?, description = ?, sort_order = ?, is_active = ?, "
            "show_in_sidebar = ?, sidebar_slot = ?, updated_at = ? "
            "WHERE id = ?;",
            &statement)) {
        error = database_.lastError();
        return false;
    }

    sqlite3_bind_text(statement, 1, normalized.name.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(statement, 2, normalized.description.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(statement, 3, normalized.sortOrder);
    sqlite3_bind_int(statement, 4, normalized.isActive ? 1 : 0);
    sqlite3_bind_int(statement, 5, normalized.showInSidebar ? 1 : 0);
    sqlite3_bind_int(statement, 6, normalized.sidebarSlot);
    sqlite3_bind_text(statement, 7, timestamp.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(statement, 8, normalized.id);

    if (sqlite3_step(statement) != SQLITE_DONE) {
        error = sqlite3_errmsg(database_.handle());
        sqlite3_finalize(statement);
        return false;
    }

    sqlite3_finalize(statement);
    if (normalized.showInSidebar) {
        sqlite3_stmt* clearStatement = nullptr;
        if (!database_.prepare(
                "UPDATE watchlists SET show_in_sidebar = 0, sidebar_slot = 0, updated_at = ? "
                "WHERE id <> ? AND sidebar_slot = ?;",
                &clearStatement)) {
            error = database_.lastError();
            return false;
        }

        sqlite3_bind_text(clearStatement, 1, timestamp.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_int(clearStatement, 2, normalized.id);
        sqlite3_bind_int(clearStatement, 3, normalized.sidebarSlot);
        if (sqlite3_step(clearStatement) != SQLITE_DONE) {
            error = sqlite3_errmsg(database_.handle());
            sqlite3_finalize(clearStatement);
            return false;
        }
        sqlite3_finalize(clearStatement);
    }
    return true;
}

bool WatchlistRepository::deactivateWatchlist(int id, std::string& error) const
{
    error.clear();
    if (id <= 0) {
        error = "Cannot deactivate a watchlist without a database id.";
        return false;
    }

    sqlite3_stmt* statement = nullptr;
    if (!database_.prepare("UPDATE watchlists SET is_active = 0, show_in_sidebar = 0, sidebar_slot = 0, updated_at = ? WHERE id = ?;", &statement)) {
        error = database_.lastError();
        return false;
    }

    const std::string timestamp = Date::nowIso8601();
    sqlite3_bind_text(statement, 1, timestamp.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(statement, 2, id);

    if (sqlite3_step(statement) != SQLITE_DONE) {
        error = sqlite3_errmsg(database_.handle());
        sqlite3_finalize(statement);
        return false;
    }

    sqlite3_finalize(statement);
    return true;
}

bool WatchlistRepository::saveWatchlistOrder(const std::vector<Watchlist>& watchlists, std::string& error) const
{
    error.clear();
    if (!database_.execute("BEGIN TRANSACTION;")) {
        error = database_.lastError();
        return false;
    }

    sqlite3_stmt* statement = nullptr;
    if (!database_.prepare("UPDATE watchlists SET sort_order = ?, updated_at = ? WHERE id = ?;", &statement)) {
        error = database_.lastError();
        database_.execute("ROLLBACK;");
        return false;
    }

    const std::string timestamp = Date::nowIso8601();
    int sortOrder = 0;
    for (const Watchlist& watchlist : watchlists) {
        if (watchlist.id <= 0) {
            continue;
        }

        sqlite3_bind_int(statement, 1, sortOrder++);
        sqlite3_bind_text(statement, 2, timestamp.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_int(statement, 3, watchlist.id);
        if (sqlite3_step(statement) != SQLITE_DONE) {
            error = sqlite3_errmsg(database_.handle());
            sqlite3_finalize(statement);
            database_.execute("ROLLBACK;");
            return false;
        }
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

int WatchlistRepository::itemCountForWatchlist(int watchlistId, std::string& error) const
{
    error.clear();
    if (watchlistId <= 0) {
        return 0;
    }

    sqlite3_stmt* statement = nullptr;
    if (!database_.prepare("SELECT COUNT(*) FROM watchlist WHERE watchlist_id = ?;", &statement)) {
        error = database_.lastError();
        return 0;
    }

    sqlite3_bind_int(statement, 1, watchlistId);
    const int result = sqlite3_step(statement);
    if (result != SQLITE_ROW) {
        error = sqlite3_errmsg(database_.handle());
        sqlite3_finalize(statement);
        return 0;
    }

    const int count = sqlite3_column_int(statement, 0);
    sqlite3_finalize(statement);
    return count;
}

std::vector<WatchlistItem> WatchlistRepository::listAll(std::string& error) const
{
    error.clear();
    std::vector<WatchlistItem> items;

    sqlite3_stmt* statement = nullptr;
    const std::string sql = std::string(WatchlistItemSelectSql) +
        "LEFT JOIN watchlists wl ON wl.id = w.watchlist_id "
        "WHERE COALESCE(wl.is_active, 1) = 1 "
        "ORDER BY COALESCE(wl.sort_order, 0), "
        "CASE w.priority WHEN 'High' THEN 0 WHEN 'Medium' THEN 1 WHEN 'Low' THEN 2 ELSE 3 END, "
        "w.ticker COLLATE NOCASE;";
    if (!database_.prepare(sql.c_str(), &statement)) {
        error = database_.lastError();
        return items;
    }

    while (sqlite3_step(statement) == SQLITE_ROW) {
        items.push_back(mapWatchlistItem(statement));
    }

    sqlite3_finalize(statement);
    return items;
}

std::vector<WatchlistItem> WatchlistRepository::listByWatchlist(int watchlistId, std::string& error) const
{
    error.clear();
    std::vector<WatchlistItem> items;

    sqlite3_stmt* statement = nullptr;
    const std::string sql = std::string(WatchlistItemSelectSql) + "WHERE w.watchlist_id = ? " + WatchlistItemOrderSql + ";";
    if (!database_.prepare(sql.c_str(), &statement)) {
        error = database_.lastError();
        return items;
    }

    sqlite3_bind_int(statement, 1, watchlistId);
    while (sqlite3_step(statement) == SQLITE_ROW) {
        items.push_back(mapWatchlistItem(statement));
    }

    sqlite3_finalize(statement);
    return items;
}

bool WatchlistRepository::create(WatchlistItem& item, std::string& error) const
{
    error.clear();
    normalizeTicker(item.ticker);
    if (item.watchlistId <= 0 && !defaultWatchlistId(item.watchlistId, error)) {
        return false;
    }
    if (item.buySignalPrice <= 0.0 && item.targetBuyPrice > 0.0) {
        item.buySignalPrice = item.targetBuyPrice;
    }
    item.targetBuyPrice = item.buySignalPrice;
    item.signalStatus = normalizeSignalStatus(item.signalStatus);
    if (!validate(item, error)) {
        return false;
    }

    const std::string timestamp = Date::nowIso8601();
    item.createdAt = timestamp;
    item.updatedAt = timestamp;

    sqlite3_stmt* statement = nullptr;
    if (!database_.prepare(
            "INSERT INTO watchlist(watchlist_id, ticker, asset_name, asset_type, target_buy_price, buy_signal_price, sell_signal_price, "
            "current_price, last_price_refresh_at, price_source, signal_status, reason_watching, risk_notes, priority, "
            "created_at, updated_at) "
            "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?);",
            &statement)) {
        error = database_.lastError();
        return false;
    }

    sqlite3_bind_int(statement, 1, item.watchlistId);
    sqlite3_bind_text(statement, 2, item.ticker.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(statement, 3, item.assetName.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(statement, 4, item.assetType.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_double(statement, 5, item.targetBuyPrice);
    sqlite3_bind_double(statement, 6, item.buySignalPrice);
    sqlite3_bind_double(statement, 7, item.sellSignalPrice);
    sqlite3_bind_double(statement, 8, item.currentPrice);
    sqlite3_bind_text(statement, 9, item.lastPriceRefreshAt.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(statement, 10, item.priceSource.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(statement, 11, item.signalStatus.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(statement, 12, item.reasonWatching.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(statement, 13, item.riskNotes.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(statement, 14, item.priority.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(statement, 15, item.createdAt.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(statement, 16, item.updatedAt.c_str(), -1, SQLITE_TRANSIENT);

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
    if (normalized.watchlistId <= 0 && !defaultWatchlistId(normalized.watchlistId, error)) {
        return false;
    }
    if (normalized.buySignalPrice <= 0.0 && normalized.targetBuyPrice > 0.0) {
        normalized.buySignalPrice = normalized.targetBuyPrice;
    }
    normalized.targetBuyPrice = normalized.buySignalPrice;
    normalized.signalStatus = normalizeSignalStatus(normalized.signalStatus);
    if (!validate(normalized, error)) {
        return false;
    }

    const std::string timestamp = Date::nowIso8601();

    sqlite3_stmt* statement = nullptr;
    if (!database_.prepare(
            "UPDATE watchlist SET watchlist_id = ?, ticker = ?, asset_name = ?, asset_type = ?, target_buy_price = ?, "
            "buy_signal_price = ?, sell_signal_price = ?, current_price = ?, last_price_refresh_at = ?, "
            "price_source = ?, signal_status = ?, reason_watching = ?, risk_notes = ?, priority = ?, updated_at = ? "
            "WHERE id = ?;",
            &statement)) {
        error = database_.lastError();
        return false;
    }

    sqlite3_bind_int(statement, 1, normalized.watchlistId);
    sqlite3_bind_text(statement, 2, normalized.ticker.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(statement, 3, normalized.assetName.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(statement, 4, normalized.assetType.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_double(statement, 5, normalized.targetBuyPrice);
    sqlite3_bind_double(statement, 6, normalized.buySignalPrice);
    sqlite3_bind_double(statement, 7, normalized.sellSignalPrice);
    sqlite3_bind_double(statement, 8, normalized.currentPrice);
    sqlite3_bind_text(statement, 9, normalized.lastPriceRefreshAt.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(statement, 10, normalized.priceSource.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(statement, 11, normalized.signalStatus.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(statement, 12, normalized.reasonWatching.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(statement, 13, normalized.riskNotes.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(statement, 14, normalized.priority.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(statement, 15, timestamp.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(statement, 16, normalized.id);

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

bool WatchlistRepository::defaultWatchlistId(int& watchlistId, std::string& error) const
{
    error.clear();
    sqlite3_stmt* statement = nullptr;
    if (!database_.prepare("SELECT id FROM watchlists WHERE name = 'Main Watchlist' ORDER BY id LIMIT 1;", &statement)) {
        error = database_.lastError();
        return false;
    }

    if (sqlite3_step(statement) == SQLITE_ROW) {
        watchlistId = sqlite3_column_int(statement, 0);
        sqlite3_finalize(statement);
        return true;
    }
    sqlite3_finalize(statement);

    Watchlist mainWatchlist;
    mainWatchlist.name = "Main Watchlist";
    mainWatchlist.description = "Default watchlist for existing items.";
    mainWatchlist.sortOrder = 0;
    mainWatchlist.isActive = true;
    if (!createWatchlist(mainWatchlist, error)) {
        return false;
    }

    watchlistId = mainWatchlist.id;
    return true;
}

bool WatchlistRepository::validateWatchlist(const Watchlist& watchlist, std::string& error)
{
    error.clear();
    if (watchlist.name.empty() || isBlank(watchlist.name)) {
        error = "Watchlist name is required.";
        return false;
    }

    if (watchlist.showInSidebar && (watchlist.sidebarSlot < 1 || watchlist.sidebarSlot > 2)) {
        error = "Sidebar slot must be Sidebar Watchlist 1 or Sidebar Watchlist 2.";
        return false;
    }

    return true;
}

bool WatchlistRepository::validate(const WatchlistItem& item, std::string& error)
{
    error.clear();

    if (item.watchlistId <= 0) {
        error = "Choose a watchlist before saving the item.";
        return false;
    }

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
