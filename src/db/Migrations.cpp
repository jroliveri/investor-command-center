// SPDX-License-Identifier: MIT
#include "db/Migrations.hpp"

#include "db/Database.hpp"

#include <sqlite3.h>

namespace {

bool hasMigration(Database& database, int version, std::string& error)
{
    sqlite3_stmt* statement = nullptr;
    if (!database.prepare("SELECT COUNT(*) FROM schema_migrations WHERE version = ?;", &statement)) {
        error = database.lastError();
        return false;
    }

    sqlite3_bind_int(statement, 1, version);
    const int result = sqlite3_step(statement);
    if (result != SQLITE_ROW) {
        error = sqlite3_errmsg(database.handle());
        sqlite3_finalize(statement);
        return false;
    }

    const bool exists = sqlite3_column_int(statement, 0) > 0;
    sqlite3_finalize(statement);
    return exists;
}

bool executeMigration(Database& database, int version, const char* name, const char* sql, std::string& error)
{
    const bool alreadyApplied = hasMigration(database, version, error);
    if (!error.empty()) {
        return false;
    }

    if (alreadyApplied) {
        return true;
    }

    if (!database.execute("BEGIN TRANSACTION;")) {
        error = database.lastError();
        return false;
    }

    if (!database.execute(sql)) {
        error = database.lastError();
        database.execute("ROLLBACK;");
        return false;
    }

    sqlite3_stmt* statement = nullptr;
    if (!database.prepare(
            "INSERT INTO schema_migrations(version, name, applied_at) VALUES (?, ?, datetime('now'));",
            &statement)) {
        error = database.lastError();
        database.execute("ROLLBACK;");
        return false;
    }

    sqlite3_bind_int(statement, 1, version);
    sqlite3_bind_text(statement, 2, name, -1, SQLITE_TRANSIENT);

    if (sqlite3_step(statement) != SQLITE_DONE) {
        error = sqlite3_errmsg(database.handle());
        sqlite3_finalize(statement);
        database.execute("ROLLBACK;");
        return false;
    }

    sqlite3_finalize(statement);

    if (!database.execute("COMMIT;")) {
        error = database.lastError();
        return false;
    }

    return true;
}

}

namespace Migrations {

bool run(Database& database, std::string& error)
{
    error.clear();

    const char* migrationsTable = R"sql(
CREATE TABLE IF NOT EXISTS schema_migrations (
    version INTEGER PRIMARY KEY,
    name TEXT NOT NULL,
    applied_at TEXT NOT NULL
);
)sql";

    if (!database.execute(migrationsTable)) {
        error = database.lastError();
        return false;
    }

    const char* accountsMigration = R"sql(
CREATE TABLE IF NOT EXISTS accounts (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    account_name TEXT NOT NULL,
    account_type TEXT NOT NULL,
    institution_name TEXT,
    current_balance REAL DEFAULT 0,
    cash_balance REAL DEFAULT 0,
    notes TEXT,
    status TEXT DEFAULT 'Active',
    created_at TEXT NOT NULL,
    updated_at TEXT NOT NULL
);

CREATE INDEX IF NOT EXISTS idx_accounts_status ON accounts(status);
)sql";

    if (!executeMigration(database, 1, "create_accounts", accountsMigration, error)) {
        return false;
    }

    const char* holdingsMigration = R"sql(
CREATE TABLE IF NOT EXISTS holdings (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    account_id INTEGER NOT NULL,
    ticker TEXT NOT NULL,
    asset_name TEXT NOT NULL,
    asset_type TEXT NOT NULL,
    shares REAL DEFAULT 0,
    average_cost REAL DEFAULT 0,
    current_price REAL DEFAULT 0,
    notes TEXT,
    created_at TEXT NOT NULL,
    updated_at TEXT NOT NULL,
    FOREIGN KEY(account_id) REFERENCES accounts(id)
);

CREATE INDEX IF NOT EXISTS idx_holdings_account_id ON holdings(account_id);
CREATE INDEX IF NOT EXISTS idx_holdings_ticker ON holdings(ticker);
)sql";

    if (!executeMigration(database, 2, "create_holdings", holdingsMigration, error)) {
        return false;
    }

    const char* transactionsMigration = R"sql(
CREATE TABLE IF NOT EXISTS transactions (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    account_id INTEGER,
    ticker TEXT,
    asset_name TEXT,
    transaction_type TEXT NOT NULL,
    transaction_date TEXT NOT NULL,
    quantity REAL DEFAULT 0,
    price REAL DEFAULT 0,
    total_amount REAL DEFAULT 0,
    notes TEXT,
    created_at TEXT NOT NULL,
    updated_at TEXT NOT NULL
);

CREATE INDEX IF NOT EXISTS idx_transactions_account_id ON transactions(account_id);
CREATE INDEX IF NOT EXISTS idx_transactions_ticker ON transactions(ticker);
CREATE INDEX IF NOT EXISTS idx_transactions_date ON transactions(transaction_date);
)sql";

    if (!executeMigration(database, 3, "create_transactions", transactionsMigration, error)) {
        return false;
    }

    const char* dividendsMigration = R"sql(
CREATE TABLE IF NOT EXISTS dividends (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    account_id INTEGER,
    ticker TEXT NOT NULL,
    asset_name TEXT,
    date_received TEXT NOT NULL,
    amount_received REAL DEFAULT 0,
    reinvested INTEGER DEFAULT 0,
    notes TEXT,
    created_at TEXT NOT NULL,
    updated_at TEXT NOT NULL
);

CREATE INDEX IF NOT EXISTS idx_dividends_account_id ON dividends(account_id);
CREATE INDEX IF NOT EXISTS idx_dividends_ticker ON dividends(ticker);
CREATE INDEX IF NOT EXISTS idx_dividends_date_received ON dividends(date_received);
)sql";

    if (!executeMigration(database, 4, "create_dividends", dividendsMigration, error)) {
        return false;
    }

    const char* goalsMigration = R"sql(
CREATE TABLE IF NOT EXISTS goals (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    goal_name TEXT NOT NULL,
    target_amount REAL DEFAULT 0,
    current_amount REAL DEFAULT 0,
    target_date TEXT,
    category TEXT,
    notes TEXT,
    created_at TEXT NOT NULL,
    updated_at TEXT NOT NULL
);

CREATE INDEX IF NOT EXISTS idx_goals_category ON goals(category);
CREATE INDEX IF NOT EXISTS idx_goals_target_date ON goals(target_date);
)sql";

    if (!executeMigration(database, 5, "create_goals", goalsMigration, error)) {
        return false;
    }

    const char* watchlistMigration = R"sql(
CREATE TABLE IF NOT EXISTS watchlist (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    ticker TEXT NOT NULL,
    asset_name TEXT NOT NULL,
    asset_type TEXT,
    target_buy_price REAL DEFAULT 0,
    current_price REAL DEFAULT 0,
    reason_watching TEXT,
    risk_notes TEXT,
    priority TEXT DEFAULT 'Medium',
    created_at TEXT NOT NULL,
    updated_at TEXT NOT NULL
);

CREATE INDEX IF NOT EXISTS idx_watchlist_ticker ON watchlist(ticker);
CREATE INDEX IF NOT EXISTS idx_watchlist_priority ON watchlist(priority);
)sql";

    return executeMigration(database, 6, "create_watchlist", watchlistMigration, error);
}

}
