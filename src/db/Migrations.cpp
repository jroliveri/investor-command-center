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

    if (!executeMigration(database, 6, "create_watchlist", watchlistMigration, error)) {
        return false;
    }

    const char* transactionGainMigration = R"sql(
ALTER TABLE transactions ADD COLUMN fees REAL DEFAULT 0;
ALTER TABLE transactions ADD COLUMN sold_quantity REAL DEFAULT 0;
ALTER TABLE transactions ADD COLUMN sale_price REAL DEFAULT 0;
ALTER TABLE transactions ADD COLUMN sale_proceeds REAL DEFAULT 0;
ALTER TABLE transactions ADD COLUMN cost_basis_used REAL DEFAULT 0;
ALTER TABLE transactions ADD COLUMN realized_gain_dollar REAL DEFAULT 0;
ALTER TABLE transactions ADD COLUMN realized_gain_percent REAL DEFAULT 0;
ALTER TABLE transactions ADD COLUMN is_adjustment INTEGER DEFAULT 0;

CREATE INDEX IF NOT EXISTS idx_transactions_type ON transactions(transaction_type);
)sql";

    if (!executeMigration(database, 7, "add_transaction_realized_gain_fields", transactionGainMigration, error)) {
        return false;
    }

    const char* portfolioSnapshotsMigration = R"sql(
CREATE TABLE IF NOT EXISTS portfolio_snapshots (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    snapshot_date TEXT NOT NULL,
    portfolio_value REAL DEFAULT 0,
    holdings_value REAL DEFAULT 0,
    cash_balance REAL DEFAULT 0,
    cost_basis REAL DEFAULT 0,
    unrealized_gain REAL DEFAULT 0,
    realized_gain_day REAL DEFAULT 0,
    realized_gain_month REAL DEFAULT 0,
    realized_gain_year REAL DEFAULT 0,
    notes TEXT,
    created_at TEXT NOT NULL,
    updated_at TEXT NOT NULL
);

CREATE UNIQUE INDEX IF NOT EXISTS idx_portfolio_snapshots_date ON portfolio_snapshots(snapshot_date);
)sql";

    if (!executeMigration(database, 8, "create_portfolio_snapshots", portfolioSnapshotsMigration, error)) {
        return false;
    }

    const char* csvImportWorkflowMigration = R"sql(
ALTER TABLE holdings ADD COLUMN status TEXT DEFAULT 'Active';
ALTER TABLE holdings ADD COLUMN last_import_batch_id INTEGER;
ALTER TABLE holdings ADD COLUMN last_seen_at TEXT;

CREATE INDEX IF NOT EXISTS idx_holdings_status ON holdings(status);
CREATE INDEX IF NOT EXISTS idx_holdings_account_ticker ON holdings(account_id, ticker);

CREATE TABLE IF NOT EXISTS import_batches (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    account_id INTEGER NOT NULL,
    import_date TEXT NOT NULL,
    source_type TEXT DEFAULT 'CSV',
    source_name TEXT,
    total_rows INTEGER DEFAULT 0,
    imported_rows INTEGER DEFAULT 0,
    updated_holdings INTEGER DEFAULT 0,
    added_holdings INTEGER DEFAULT 0,
    skipped_rows INTEGER DEFAULT 0,
    warning_count INTEGER DEFAULT 0,
    error_count INTEGER DEFAULT 0,
    missing_holdings INTEGER DEFAULT 0,
    notes TEXT,
    created_at TEXT NOT NULL,
    updated_at TEXT NOT NULL
);

CREATE INDEX IF NOT EXISTS idx_import_batches_account_date ON import_batches(account_id, import_date);

ALTER TABLE portfolio_snapshots ADD COLUMN account_id INTEGER DEFAULT 0;
ALTER TABLE portfolio_snapshots ADD COLUMN import_batch_id INTEGER;
ALTER TABLE portfolio_snapshots ADD COLUMN source TEXT DEFAULT 'Manual';

DROP INDEX IF EXISTS idx_portfolio_snapshots_date;
CREATE UNIQUE INDEX IF NOT EXISTS idx_portfolio_snapshots_account_date_source
    ON portfolio_snapshots(account_id, snapshot_date, source);
)sql";

    if (!executeMigration(database, 9, "csv_import_batches_and_snapshot_sources", csvImportWorkflowMigration, error)) {
        return false;
    }

    const char* dashboardWidgetsMigration = R"sql(
CREATE TABLE IF NOT EXISTS dashboard_widgets (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    widget_key TEXT NOT NULL UNIQUE,
    display_name TEXT NOT NULL,
    sort_order INTEGER NOT NULL,
    is_visible INTEGER DEFAULT 1,
    created_at TEXT NOT NULL,
    updated_at TEXT NOT NULL
);

CREATE INDEX IF NOT EXISTS idx_dashboard_widgets_sort_order ON dashboard_widgets(sort_order);
)sql";

    if (!executeMigration(database, 10, "create_dashboard_widgets", dashboardWidgetsMigration, error)) {
        return false;
    }

    const char* linkedGoalAccountMigration = R"sql(
ALTER TABLE goals ADD COLUMN use_account_value INTEGER DEFAULT 0;
ALTER TABLE goals ADD COLUMN linked_account_id INTEGER;

CREATE INDEX IF NOT EXISTS idx_goals_linked_account_id ON goals(linked_account_id);
)sql";

    if (!executeMigration(database, 11, "add_goal_account_value_link", linkedGoalAccountMigration, error)) {
        return false;
    }

    const char* appSettingsMigration = R"sql(
CREATE TABLE IF NOT EXISTS app_settings (
    setting_key TEXT PRIMARY KEY,
    setting_value TEXT NOT NULL,
    created_at TEXT NOT NULL,
    updated_at TEXT NOT NULL
);
)sql";

    if (!executeMigration(database, 12, "create_app_settings", appSettingsMigration, error)) {
        return false;
    }

    const char* capitalGainAllocationMigration = R"sql(
CREATE TABLE IF NOT EXISTS capital_gain_allocation_rules (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    name TEXT NOT NULL,
    percentage REAL DEFAULT 0,
    sort_order INTEGER DEFAULT 0,
    is_active INTEGER DEFAULT 1,
    created_at TEXT NOT NULL,
    updated_at TEXT NOT NULL
);

CREATE INDEX IF NOT EXISTS idx_capital_gain_allocation_rules_order
    ON capital_gain_allocation_rules(sort_order, id);
CREATE INDEX IF NOT EXISTS idx_capital_gain_allocation_rules_active
    ON capital_gain_allocation_rules(is_active, sort_order);
)sql";

    if (!executeMigration(database, 13, "create_capital_gain_allocation_rules", capitalGainAllocationMigration, error)) {
        return false;
    }

    const char* dashboardChartSettingsMigration = R"sql(
CREATE TABLE IF NOT EXISTS dashboard_chart_settings (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    chart_key TEXT NOT NULL UNIQUE,
    data_mode TEXT NOT NULL,
    time_range TEXT NOT NULL,
    chart_type TEXT NOT NULL,
    created_at TEXT NOT NULL,
    updated_at TEXT NOT NULL
);
)sql";

    if (!executeMigration(database, 14, "create_dashboard_chart_settings", dashboardChartSettingsMigration, error)) {
        return false;
    }

    const char* marketQuoteCacheMigration = R"sql(
CREATE TABLE IF NOT EXISTS market_quote_cache (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    symbol TEXT NOT NULL UNIQUE,
    provider TEXT NOT NULL,
    company_name TEXT,
    current_price REAL,
    previous_close REAL,
    open_price REAL,
    day_high REAL,
    day_low REAL,
    fifty_two_week_high REAL,
    fifty_two_week_low REAL,
    market_cap REAL,
    volume REAL,
    average_volume REAL,
    pe_ratio REAL,
    eps REAL,
    dividend_yield REAL,
    beta REAL,
    currency TEXT,
    exchange_name TEXT,
    quote_time TEXT,
    fetched_at TEXT NOT NULL,
    raw_status TEXT,
    created_at TEXT NOT NULL,
    updated_at TEXT NOT NULL
);

CREATE INDEX IF NOT EXISTS idx_market_quote_cache_provider_symbol
    ON market_quote_cache(provider, symbol);
)sql";

    if (!executeMigration(database, 15, "create_market_quote_cache", marketQuoteCacheMigration, error)) {
        return false;
    }

    const char* marketQuoteChangeMigration = R"sql(
ALTER TABLE market_quote_cache ADD COLUMN price_change REAL;
ALTER TABLE market_quote_cache ADD COLUMN price_change_percent REAL;
)sql";

    if (!executeMigration(database, 16, "add_market_quote_change_fields", marketQuoteChangeMigration, error)) {
        return false;
    }

    const char* watchlistSignalsMigration = R"sql(
ALTER TABLE watchlist ADD COLUMN buy_signal_price REAL DEFAULT 0;
ALTER TABLE watchlist ADD COLUMN sell_signal_price REAL DEFAULT 0;
ALTER TABLE watchlist ADD COLUMN last_price_refresh_at TEXT;
ALTER TABLE watchlist ADD COLUMN price_source TEXT;
ALTER TABLE watchlist ADD COLUMN signal_status TEXT DEFAULT 'None';

UPDATE watchlist
SET buy_signal_price = COALESCE(NULLIF(target_buy_price, 0), buy_signal_price)
WHERE buy_signal_price = 0 AND target_buy_price > 0;

CREATE INDEX IF NOT EXISTS idx_watchlist_signal_status ON watchlist(signal_status);
)sql";

    if (!executeMigration(database, 17, "add_watchlist_price_signals", watchlistSignalsMigration, error)) {
        return false;
    }

    const char* multipleWatchlistsMigration = R"sql(
CREATE TABLE IF NOT EXISTS watchlists (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    name TEXT NOT NULL,
    description TEXT,
    sort_order INTEGER DEFAULT 0,
    is_active INTEGER DEFAULT 1,
    created_at TEXT NOT NULL,
    updated_at TEXT NOT NULL
);

INSERT INTO watchlists(name, description, sort_order, is_active, created_at, updated_at)
SELECT 'Main Watchlist', 'Default watchlist for existing items.', 0, 1, datetime('now'), datetime('now')
WHERE NOT EXISTS (
    SELECT 1 FROM watchlists WHERE name = 'Main Watchlist'
);

ALTER TABLE watchlist ADD COLUMN watchlist_id INTEGER;

UPDATE watchlist
SET watchlist_id = (
    SELECT id FROM watchlists WHERE name = 'Main Watchlist' ORDER BY id LIMIT 1
)
WHERE watchlist_id IS NULL OR watchlist_id = 0;

CREATE INDEX IF NOT EXISTS idx_watchlists_active_order ON watchlists(is_active, sort_order, id);
CREATE UNIQUE INDEX IF NOT EXISTS idx_watchlists_active_name_unique
    ON watchlists(name COLLATE NOCASE)
    WHERE is_active = 1;
CREATE INDEX IF NOT EXISTS idx_watchlist_watchlist_id ON watchlist(watchlist_id);
)sql";

    if (!executeMigration(database, 18, "create_multiple_watchlists", multipleWatchlistsMigration, error)) {
        return false;
    }

    const char* watchlistSidebarAssignmentMigration = R"sql(
ALTER TABLE watchlists ADD COLUMN show_in_sidebar INTEGER DEFAULT 0;
ALTER TABLE watchlists ADD COLUMN sidebar_slot INTEGER DEFAULT 0;

UPDATE watchlists
SET show_in_sidebar = 0, sidebar_slot = 0
WHERE is_active = 0;

CREATE INDEX IF NOT EXISTS idx_watchlists_sidebar_slot
    ON watchlists(show_in_sidebar, sidebar_slot, is_active);
)sql";

    if (!executeMigration(database, 19, "add_watchlist_sidebar_assignment", watchlistSidebarAssignmentMigration, error)) {
        return false;
    }

    const char* watchlistSignalSimplificationMigration = R"sql(
UPDATE watchlist
SET signal_status = 'Buy'
WHERE signal_status = 'Buy Signal';

UPDATE watchlist
SET signal_status = 'Sell'
WHERE signal_status = 'Sell Signal';

UPDATE watchlist
SET signal_status = 'Hold'
WHERE signal_status IS NULL
   OR signal_status = ''
   OR signal_status = 'None'
   OR signal_status = 'Watch'
   OR signal_status = 'No Signal'
   OR signal_status = 'No Price'
   OR signal_status = 'Check Signals';
)sql";

    if (!executeMigration(database, 20, "simplify_watchlist_signal_status", watchlistSignalSimplificationMigration, error)) {
        return false;
    }

    const char* marketPriceHistoryMigration = R"sql(
CREATE TABLE IF NOT EXISTS market_price_history (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    symbol TEXT NOT NULL,
    provider TEXT NOT NULL DEFAULT 'Yahoo Finance',
    price_date TEXT NOT NULL,
    open_price REAL,
    high_price REAL,
    low_price REAL,
    close_price REAL,
    adjusted_close_price REAL,
    volume REAL,
    fetched_at TEXT NOT NULL,
    created_at TEXT NOT NULL,
    updated_at TEXT NOT NULL
);

CREATE UNIQUE INDEX IF NOT EXISTS idx_market_price_history_symbol_provider_date
    ON market_price_history(symbol, provider, price_date);
CREATE INDEX IF NOT EXISTS idx_market_price_history_symbol_date
    ON market_price_history(symbol, price_date);
)sql";

    return executeMigration(database, 21, "create_market_price_history", marketPriceHistoryMigration, error);
}

}
