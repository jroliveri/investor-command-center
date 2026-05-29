// SPDX-License-Identifier: MIT
#include "repositories/PortfolioSnapshotRepository.hpp"

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

PortfolioSnapshot readSnapshot(sqlite3_stmt* statement)
{
    PortfolioSnapshot snapshot;
    snapshot.id = sqlite3_column_int(statement, 0);
    snapshot.accountId = sqlite3_column_int(statement, 1);
    snapshot.importBatchId = sqlite3_column_type(statement, 2) == SQLITE_NULL ? 0 : sqlite3_column_int(statement, 2);
    snapshot.snapshotDate = textColumn(statement, 3);
    snapshot.source = textColumn(statement, 4);
    if (snapshot.source.empty()) {
        snapshot.source = "Manual";
    }
    snapshot.portfolioValue = sqlite3_column_double(statement, 5);
    snapshot.holdingsValue = sqlite3_column_double(statement, 6);
    snapshot.cashBalance = sqlite3_column_double(statement, 7);
    snapshot.costBasis = sqlite3_column_double(statement, 8);
    snapshot.unrealizedGain = sqlite3_column_double(statement, 9);
    snapshot.realizedGainDay = sqlite3_column_double(statement, 10);
    snapshot.realizedGainMonth = sqlite3_column_double(statement, 11);
    snapshot.realizedGainYear = sqlite3_column_double(statement, 12);
    snapshot.notes = textColumn(statement, 13);
    snapshot.createdAt = textColumn(statement, 14);
    snapshot.updatedAt = textColumn(statement, 15);
    return snapshot;
}

void bindSnapshot(sqlite3_stmt* statement, const PortfolioSnapshot& snapshot)
{
    sqlite3_bind_int(statement, 1, snapshot.accountId);
    if (snapshot.importBatchId > 0) {
        sqlite3_bind_int(statement, 2, snapshot.importBatchId);
    } else {
        sqlite3_bind_null(statement, 2);
    }
    sqlite3_bind_text(statement, 3, snapshot.snapshotDate.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(statement, 4, snapshot.source.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_double(statement, 5, snapshot.portfolioValue);
    sqlite3_bind_double(statement, 6, snapshot.holdingsValue);
    sqlite3_bind_double(statement, 7, snapshot.cashBalance);
    sqlite3_bind_double(statement, 8, snapshot.costBasis);
    sqlite3_bind_double(statement, 9, snapshot.unrealizedGain);
    sqlite3_bind_double(statement, 10, snapshot.realizedGainDay);
    sqlite3_bind_double(statement, 11, snapshot.realizedGainMonth);
    sqlite3_bind_double(statement, 12, snapshot.realizedGainYear);
    sqlite3_bind_text(statement, 13, snapshot.notes.c_str(), -1, SQLITE_TRANSIENT);
}

}

PortfolioSnapshotRepository::PortfolioSnapshotRepository(Database& database)
    : database_(database)
{
}

std::vector<PortfolioSnapshot> PortfolioSnapshotRepository::listAll(std::string& error) const
{
    error.clear();
    std::vector<PortfolioSnapshot> snapshots;

    sqlite3_stmt* statement = nullptr;
    if (!database_.prepare(
            "SELECT id, account_id, import_batch_id, snapshot_date, source, portfolio_value, holdings_value, "
            "cash_balance, cost_basis, unrealized_gain, realized_gain_day, realized_gain_month, realized_gain_year, notes, "
            "created_at, updated_at "
            "FROM portfolio_snapshots ORDER BY snapshot_date DESC, id DESC;",
            &statement)) {
        error = database_.lastError();
        return snapshots;
    }

    while (sqlite3_step(statement) == SQLITE_ROW) {
        snapshots.push_back(readSnapshot(statement));
    }

    sqlite3_finalize(statement);
    return snapshots;
}

bool PortfolioSnapshotRepository::create(PortfolioSnapshot& snapshot, std::string& error) const
{
    error.clear();
    if (!validate(snapshot, error)) {
        return false;
    }

    const std::string timestamp = Date::nowIso8601();
    snapshot.createdAt = timestamp;
    snapshot.updatedAt = timestamp;

    sqlite3_stmt* statement = nullptr;
    if (!database_.prepare(
            "INSERT INTO portfolio_snapshots(account_id, import_batch_id, snapshot_date, source, portfolio_value, holdings_value, cash_balance, "
            "cost_basis, unrealized_gain, realized_gain_day, realized_gain_month, realized_gain_year, "
            "notes, created_at, updated_at) "
            "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?);",
            &statement)) {
        error = database_.lastError();
        return false;
    }

    bindSnapshot(statement, snapshot);
    sqlite3_bind_text(statement, 14, snapshot.createdAt.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(statement, 15, snapshot.updatedAt.c_str(), -1, SQLITE_TRANSIENT);

    if (sqlite3_step(statement) != SQLITE_DONE) {
        error = sqlite3_errmsg(database_.handle());
        sqlite3_finalize(statement);
        return false;
    }

    sqlite3_finalize(statement);
    snapshot.id = static_cast<int>(database_.lastInsertRowId());
    return true;
}

bool PortfolioSnapshotRepository::update(const PortfolioSnapshot& snapshot, std::string& error) const
{
    error.clear();
    if (snapshot.id <= 0) {
        error = "Cannot update a snapshot without a database id.";
        return false;
    }

    if (!validate(snapshot, error)) {
        return false;
    }

    const std::string timestamp = Date::nowIso8601();

    sqlite3_stmt* statement = nullptr;
    if (!database_.prepare(
            "UPDATE portfolio_snapshots SET account_id = ?, import_batch_id = ?, snapshot_date = ?, source = ?, "
            "portfolio_value = ?, holdings_value = ?, cash_balance = ?, cost_basis = ?, unrealized_gain = ?, realized_gain_day = ?, "
            "realized_gain_month = ?, realized_gain_year = ?, notes = ?, updated_at = ? "
            "WHERE id = ?;",
            &statement)) {
        error = database_.lastError();
        return false;
    }

    bindSnapshot(statement, snapshot);
    sqlite3_bind_text(statement, 14, timestamp.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(statement, 15, snapshot.id);

    if (sqlite3_step(statement) != SQLITE_DONE) {
        error = sqlite3_errmsg(database_.handle());
        sqlite3_finalize(statement);
        return false;
    }

    sqlite3_finalize(statement);
    return true;
}

bool PortfolioSnapshotRepository::upsertForDate(PortfolioSnapshot& snapshot, std::string& error) const
{
    snapshot.accountId = 0;
    snapshot.importBatchId = 0;
    snapshot.source = "Manual";
    PortfolioSnapshot existing;
    if (!findByDate(snapshot.snapshotDate, existing, error)) {
        if (!error.empty()) {
            return false;
        }

        return create(snapshot, error);
    }

    snapshot.id = existing.id;
    snapshot.createdAt = existing.createdAt;
    return update(snapshot, error);
}

bool PortfolioSnapshotRepository::upsertForAccountDateSource(PortfolioSnapshot& snapshot, std::string& error) const
{
    PortfolioSnapshot existing;
    if (!findByAccountDateSource(snapshot.accountId, snapshot.snapshotDate, snapshot.source, existing, error)) {
        if (!error.empty()) {
            return false;
        }

        return create(snapshot, error);
    }

    snapshot.id = existing.id;
    snapshot.createdAt = existing.createdAt;
    return update(snapshot, error);
}

bool PortfolioSnapshotRepository::existsForAccountDateSource(int accountId, const std::string& snapshotDate, const std::string& source, std::string& error) const
{
    PortfolioSnapshot existing;
    return findByAccountDateSource(accountId, snapshotDate, source, existing, error);
}

bool PortfolioSnapshotRepository::validate(const PortfolioSnapshot& snapshot, std::string& error)
{
    error.clear();

    if (snapshot.snapshotDate.empty() || isBlank(snapshot.snapshotDate)) {
        error = "Snapshot date is required.";
        return false;
    }

    if (!Date::isIsoDate(snapshot.snapshotDate)) {
        error = "Snapshot date must use YYYY-MM-DD.";
        return false;
    }

    if (snapshot.source.empty() || isBlank(snapshot.source)) {
        error = "Snapshot source is required.";
        return false;
    }

    return true;
}

bool PortfolioSnapshotRepository::findByDate(const std::string& snapshotDate, PortfolioSnapshot& snapshot, std::string& error) const
{
    error.clear();

    sqlite3_stmt* statement = nullptr;
    if (!database_.prepare(
            "SELECT id, account_id, import_batch_id, snapshot_date, source, portfolio_value, holdings_value, "
            "cash_balance, cost_basis, unrealized_gain, realized_gain_day, realized_gain_month, realized_gain_year, notes, "
            "created_at, updated_at "
            "FROM portfolio_snapshots WHERE snapshot_date = ? AND account_id = 0 AND source = 'Manual' LIMIT 1;",
            &statement)) {
        error = database_.lastError();
        return false;
    }

    sqlite3_bind_text(statement, 1, snapshotDate.c_str(), -1, SQLITE_TRANSIENT);

    if (sqlite3_step(statement) == SQLITE_ROW) {
        snapshot = readSnapshot(statement);
        sqlite3_finalize(statement);
        return true;
    }

    sqlite3_finalize(statement);
    return false;
}

bool PortfolioSnapshotRepository::findByAccountDateSource(int accountId, const std::string& snapshotDate, const std::string& source, PortfolioSnapshot& snapshot, std::string& error) const
{
    error.clear();

    sqlite3_stmt* statement = nullptr;
    if (!database_.prepare(
            "SELECT id, account_id, import_batch_id, snapshot_date, source, portfolio_value, holdings_value, "
            "cash_balance, cost_basis, unrealized_gain, realized_gain_day, realized_gain_month, realized_gain_year, notes, "
            "created_at, updated_at "
            "FROM portfolio_snapshots WHERE account_id = ? AND snapshot_date = ? AND source = ? LIMIT 1;",
            &statement)) {
        error = database_.lastError();
        return false;
    }

    sqlite3_bind_int(statement, 1, accountId);
    sqlite3_bind_text(statement, 2, snapshotDate.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(statement, 3, source.c_str(), -1, SQLITE_TRANSIENT);

    if (sqlite3_step(statement) == SQLITE_ROW) {
        snapshot = readSnapshot(statement);
        sqlite3_finalize(statement);
        return true;
    }

    sqlite3_finalize(statement);
    return false;
}
