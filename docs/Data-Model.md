# Data Model

Database file:

```text
data/investor_command_center.db
```

Dates are stored as `TEXT` in `YYYY-MM-DD` format. UI date picker controls and repository validation share the same date utility logic so invalid calendar dates are rejected before saving.

## Migrations

The app creates a `schema_migrations` table and records applied migrations by integer version.

## accounts

```sql
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
```

`current_balance` is retained for backward compatibility with early local databases. It is not the source of truth for displayed balances.

Displayed account balance is calculated in C++:

```text
accountBalance = sum(marketValue for holdings in account) + cashBalance
marketValue = shares * currentPrice
```

Users should enter `cash_balance`; holdings provide the market value portion.

## holdings

```sql
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
    status TEXT DEFAULT 'Active',
    last_import_batch_id INTEGER,
    last_seen_at TEXT,
    created_at TEXT NOT NULL,
    updated_at TEXT NOT NULL,
    FOREIGN KEY(account_id) REFERENCES accounts(id)
);
```

`status` is used for soft deletion and repeated CSV imports. Active holdings count toward account balances and dashboard totals. Inactive holdings remain in the local database but are excluded from portfolio value calculations.

`last_import_batch_id` and `last_seen_at` record the most recent CSV import that saw or changed the holding.

## transactions

```sql
CREATE TABLE IF NOT EXISTS transactions (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    account_id INTEGER,
    ticker TEXT,
    asset_name TEXT,
    transaction_type TEXT NOT NULL,
    transaction_date TEXT NOT NULL,
    quantity REAL DEFAULT 0,
    price REAL DEFAULT 0,
    fees REAL DEFAULT 0,
    total_amount REAL DEFAULT 0,
    sold_quantity REAL DEFAULT 0,
    sale_price REAL DEFAULT 0,
    sale_proceeds REAL DEFAULT 0,
    cost_basis_used REAL DEFAULT 0,
    realized_gain_dollar REAL DEFAULT 0,
    realized_gain_percent REAL DEFAULT 0,
    is_adjustment INTEGER DEFAULT 0,
    notes TEXT,
    created_at TEXT NOT NULL,
    updated_at TEXT NOT NULL
);
```

Supported transaction types:

- `Buy`
- `Sell`
- `Dividend`
- `Contribution`
- `Withdrawal`
- `Transfer`
- `Fee`
- `Interest`
- `Other`

Holdings show what is currently owned. Transactions explain how those holdings got there.

For the first pass, buy and sell transactions use average cost basis:

```text
saleProceeds = soldQuantity * salePrice - fees
costBasisUsed = soldQuantity * averageCost
realizedGainDollar = saleProceeds - costBasisUsed
realizedGainPercent = realizedGainDollar / costBasisUsed * 100
```

When `costBasisUsed` is zero, realized gain percent is reported as `0`.

New buy transactions can increase matching holding shares and update average cost. New sell transactions can reduce matching holding shares and calculate realized gain/loss. Full historical reconciliation for edited or deleted transactions is planned for a later pass.

Realized gain/loss is separate from unrealized gain/loss. Realized gain/loss comes from sell transactions. Unrealized gain/loss comes from current holdings market value compared with current holdings cost basis.

## portfolio_snapshots

```sql
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
    account_id INTEGER DEFAULT 0,
    import_batch_id INTEGER,
    source TEXT DEFAULT 'Manual',
    notes TEXT,
    created_at TEXT NOT NULL,
    updated_at TEXT NOT NULL
);
```

Snapshots store local portfolio totals for a date. CSV imports create snapshots automatically with `source = 'CSV Import'` and `account_id` set to the imported account. Manual snapshots use `source = 'Manual'` and `account_id = 0`.

Dashboard daily, monthly, and yearly movement compares the current portfolio value against earlier snapshots:

```text
dailyGainLoss = currentPortfolioValue - mostRecentPreviousSnapshot.portfolioValue
monthlyGainLoss = currentPortfolioValue - firstSnapshotThisMonth.portfolioValue
yearlyGainLoss = currentPortfolioValue - firstSnapshotThisYear.portfolioValue
```

Snapshot performance is a simple dollar movement first pass. It may include deposits and withdrawals. Contribution-adjusted returns are planned for a future update.

## import_batches

```sql
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
```

Import batches track CSV import metadata only. The app does not copy source CSV files into the repository or store the full CSV file in SQLite.

## dashboard_widgets

```sql
CREATE TABLE IF NOT EXISTS dashboard_widgets (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    widget_key TEXT NOT NULL UNIQUE,
    display_name TEXT NOT NULL,
    sort_order INTEGER NOT NULL,
    is_visible INTEGER DEFAULT 1,
    created_at TEXT NOT NULL,
    updated_at TEXT NOT NULL
);
```

Dashboard widget records store local layout preferences only. They control the order and visibility of dashboard sections and do not affect portfolio calculations.

Stable widget keys include:

- `portfolio_summary`
- `performance_movement`
- `holdings_table`
- `realized_gains`
- `dividend_summary`
- `recent_transactions`
- `snapshot_status`
- `allocation_panel`
- `performance_panel`
- `income_gains_panel`
- `top_gainers_losers`
- `holdings_value`
- `cash_balance`
- `cost_basis`
- `unrealized_gain_loss`

If no widget records exist, the app seeds the default dashboard layout. `Reset Dashboard Layout` restores the default order and visibility.

Legacy chart widget keys such as `asset_allocation`, `account_allocation`, `dividends_by_month`, and `portfolio_value_history` are ignored by the current dashboard. Their chart behavior is consolidated into the controlled dashboard chart panels.

## dashboard_chart_settings

```sql
CREATE TABLE IF NOT EXISTS dashboard_chart_settings (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    chart_key TEXT NOT NULL UNIQUE,
    data_mode TEXT NOT NULL,
    time_range TEXT NOT NULL,
    chart_type TEXT NOT NULL,
    created_at TEXT NOT NULL,
    updated_at TEXT NOT NULL
);
```

Dashboard chart settings store local display preferences only. Supported chart keys are:

- `allocation_panel`
- `performance_panel`
- `income_gains_panel`

The Allocation panel can display current allocation by asset type, account, or ticker/holding. The Performance panel uses local portfolio snapshots for portfolio value, holdings value, cash balance, and unrealized gain/loss over time. The Income/Gains panel groups local dividend records or realized gain records by month.

Unknown or incomplete chart settings fall back to safe defaults. Chart preferences do not affect portfolio calculations or stored financial records.

## market_quote_cache

```sql
CREATE TABLE IF NOT EXISTS market_quote_cache (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    symbol TEXT NOT NULL UNIQUE,
    provider TEXT NOT NULL,
    company_name TEXT,
    current_price REAL,
    price_change REAL,
    price_change_percent REAL,
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
```

Market quote cache records store the latest user-requested Stock Research or explicit Watchlist price refresh quote by symbol. The first provider is Yahoo Finance. Cached quotes are convenience metadata only; they are not brokerage-verified account records and do not update holdings, account balances, dashboard totals, or CSV-imported data.

If an online research fetch fails and a cached quote exists, the app may show the cached quote with a warning. Missing Yahoo Finance fields are displayed as `N/A`.

`price_change` and `price_change_percent` are optional display fields used by the Stock Research quote summary when Yahoo Finance returns them. If unavailable, the UI displays `N/A` instead of treating the data as broken.

## app_settings

```sql
CREATE TABLE IF NOT EXISTS app_settings (
    setting_key TEXT PRIMARY KEY,
    setting_value TEXT NOT NULL,
    created_at TEXT NOT NULL,
    updated_at TEXT NOT NULL
);
```

App settings store local UI preferences only. The current first setting is `theme`, with supported values:

- `dark_command_center`
- `light_trading_terminal`

Theme preferences are local display settings and do not affect calculations or stored investing records.

## capital_gain_allocation_rules

```sql
CREATE TABLE IF NOT EXISTS capital_gain_allocation_rules (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    name TEXT NOT NULL,
    percentage REAL DEFAULT 0,
    sort_order INTEGER DEFAULT 0,
    is_active INTEGER DEFAULT 1,
    created_at TEXT NOT NULL,
    updated_at TEXT NOT NULL
);
```

Capital gains allocation rules are local user-defined display preferences. They are used by the Transactions page to show an allocation helper for positive realized gains on `Sell` transactions.

The app calculates allocation rows in C++:

```text
allocationAmount = realizedGainDollar * (percentage / 100.0)
```

Active percentages are allowed to total less than or greater than `100%`; the UI shows a warning and reports unallocated or overallocated amounts. The app does not move money, create transfers, provide tax advice, or provide financial advice.

## dividends

```sql
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
```

## goals

```sql
CREATE TABLE IF NOT EXISTS goals (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    goal_name TEXT NOT NULL,
    target_amount REAL DEFAULT 0,
    current_amount REAL DEFAULT 0,
    use_account_value INTEGER DEFAULT 0,
    linked_account_id INTEGER,
    target_date TEXT,
    category TEXT,
    notes TEXT,
    created_at TEXT NOT NULL,
    updated_at TEXT NOT NULL
);
```

`current_amount` remains the stored manual amount for manual goals.

When `use_account_value = 1`, the app treats `linked_account_id` as an optional link to an account and calculates the effective current amount at runtime:

```text
effectiveCurrentAmount = active holdings market value for linked account + linked account cash balance
```

If the linked account is missing or invalid, the effective current amount is treated as `0.00` and the UI shows a warning. Linked account goals use local calculated balances only and do not change account balance logic.

## watchlist

```sql
CREATE TABLE IF NOT EXISTS watchlist (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    ticker TEXT NOT NULL,
    asset_name TEXT NOT NULL,
    asset_type TEXT,
    target_buy_price REAL DEFAULT 0,
    buy_signal_price REAL DEFAULT 0,
    sell_signal_price REAL DEFAULT 0,
    current_price REAL DEFAULT 0,
    last_price_refresh_at TEXT,
    price_source TEXT,
    signal_status TEXT DEFAULT 'None',
    reason_watching TEXT,
    risk_notes TEXT,
    priority TEXT DEFAULT 'Medium',
    created_at TEXT NOT NULL,
    updated_at TEXT NOT NULL
);
```

`target_buy_price` remains for backward compatibility. New UI and signal logic use `buy_signal_price` and `sell_signal_price`.

Watchlist signal status is calculated from the locally saved price levels and the refreshed current price:

```text
Buy Signal = currentPrice <= buySignalPrice, when buySignalPrice > 0
Sell Signal = currentPrice >= sellSignalPrice, when sellSignalPrice > 0
Check Signals = both saved levels trigger
No Price = currentPrice is missing or zero
No Signal = no saved level is currently triggered
```

Watchlist price refreshes use `MarketDataService` and the Yahoo Finance provider abstraction already used by Stock Research. Refreshes update only the watchlist record's current price, last refresh timestamp, source label, and calculated signal status. They do not update holdings, transactions, snapshots, brokerage records, or account balances.

## Holding Calculations

Holding calculations are performed in C++:

```text
costBasis = shares * averageCost
marketValue = shares * currentPrice
gainLossDollar = marketValue - costBasis
gainLossPercent = gainLossDollar / costBasis * 100
```

When `costBasis` is zero, `gainLossPercent` is reported as `0` to avoid division by zero.

## Dashboard Calculations

- Account balances are calculated from holdings market value plus account cash balance.
- Inactive holdings are excluded from dashboard portfolio totals.
- Dashboard layout preferences are loaded from `dashboard_widgets` and only affect display order/visibility.
- Dashboard chart preferences are loaded from `dashboard_chart_settings` and only affect chart display mode, range, and chart type.
- Theme preference is loaded from `app_settings` and only affects local UI styling.
- Capital gain allocation rules are loaded from `capital_gain_allocation_rules` and only affect the local transaction allocation helper.
- Market quote cache records are used by Stock Research and explicit Watchlist price refreshes only; they do not affect portfolio calculations.
- Dividend totals are calculated in C++ from `dividends.date_received` using `YYYY-MM` and `YYYY` prefixes.
- Realized gain/loss totals are calculated from sell transactions.
- Daily, monthly, and yearly gain/loss are calculated from portfolio snapshots.
- Goal progress is calculated in C++ from the effective current amount divided by target amount, clamped between `0` and `100`.
- Watchlist priority counts are calculated in C++ from the `priority` field.
- Watchlist price signal badges are calculated in C++ from `current_price`, `buy_signal_price`, and `sell_signal_price`.
- Recent transactions are loaded from `transactions` ordered by `transaction_date DESC, id DESC`.

## Dashboard Current Price Refresh

Dashboard current-price refresh is an in-memory display overlay. It collects active holding tickers, fetches current quote prices through `MarketDataService`, and temporarily applies those prices to dashboard calculations for:

- Current holdings market value
- Current portfolio value
- Current unrealized gain/loss
- Allocation and top gainers/losers display

The overlay is not a SQLite table and does not overwrite `holdings.current_price`. It does not change shares, average cost, cost basis, transactions, import batches, or portfolio snapshots.

Price source labels are displayed as:

- `Live Quote`
- `Cached Quote`
- `CSV Import`
- `Manual`

Cached quote data comes from `market_quote_cache` only when an online fetch fails and a cached quote is available. Cached data is labeled and should not be treated as brokerage-verified data.

## CSV Import

Holdings CSV import is the normal update workflow. Valid rows are upserted by `account_id + ticker`. Existing active holdings for the same account that are missing from the latest import are marked `Inactive`, not hard deleted. A successful CSV import creates an `import_batches` record and automatically creates or updates a CSV import portfolio snapshot for the import date.

Source CSV files remain local and are not copied into the repository by default.

## Stock Research

Stock Research is informational and currently uses Yahoo Finance as the first market data source behind a provider abstraction. The UI calls `MarketDataService`; network calls are isolated in `YahooFinanceProvider` and `HttpClient`.

Yahoo Finance data may be delayed, unavailable, rate-limited, incomplete, or changed without notice. Research data does not provide financial advice, tax advice, trading recommendations, brokerage sync, or automatic portfolio price updates.
