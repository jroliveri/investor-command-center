# Data Model

Database file:

```text
data/investor_command_center.db
```

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
    created_at TEXT NOT NULL,
    updated_at TEXT NOT NULL,
    FOREIGN KEY(account_id) REFERENCES accounts(id)
);
```

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
    total_amount REAL DEFAULT 0,
    notes TEXT,
    created_at TEXT NOT NULL,
    updated_at TEXT NOT NULL
);
```

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
    target_date TEXT,
    category TEXT,
    notes TEXT,
    created_at TEXT NOT NULL,
    updated_at TEXT NOT NULL
);
```

## watchlist

```sql
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
```

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

- Dividend totals are calculated in C++ from `dividends.date_received` using `YYYY-MM` and `YYYY` prefixes.
- Goal progress is calculated in C++ as `current_amount / target_amount * 100`, clamped between `0` and `100`.
- Watchlist priority counts are calculated in C++ from the `priority` field.
- Recent transactions are loaded from `transactions` ordered by `transaction_date DESC, id DESC`.
