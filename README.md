# Investor Command Center

Investor Command Center is a personal Windows desktop investing tracker written in C++20. It is designed as a private, local-first cockpit for manually tracking accounts, holdings, and long-term portfolio progress.

Subtitle: Long-term progress beats short-term noise.

This app does not connect to brokerage accounts, does not sync to the cloud, does not call stock price APIs, and does not provide financial advice or buy/sell recommendations.

## Current App

- Native Win32 desktop shell using Dear ImGui with a DirectX 11 renderer
- Sidebar navigation for Dashboard, Accounts, Holdings, Transactions, Dividends, Goals, Watchlist, Reports, and Settings
- Dashboard summary cards based on local account, holding, transaction, dividend, goal, and watchlist records
- Account create, edit, and delete workflow
- Holding create, edit, and delete workflow
- Transaction create, edit, delete, and search workflow
- Dividend create, edit, delete, and search workflow with month, year, and lifetime totals
- Goal create, edit, delete, and search workflow with progress bars
- Watchlist create, edit, delete, and search workflow with priority badges
- SQLite database initialization at `data/investor_command_center.db`
- SQLite migrations for `accounts`, `holdings`, `transactions`, `dividends`, `goals`, and `watchlist`
- Basic validation and delete confirmation dialogs
- C++ portfolio calculations for cost basis, market value, and gain/loss

## Build Requirements

- Windows 10 or Windows 11
- CMake 3.24 or newer
- Visual Studio 2022 or newer, or Visual Studio Build Tools with the Desktop development with C++ workload
- Git
- Internet access for the first configure step so CMake can fetch Dear ImGui and the SQLite amalgamation

## Setup

From the project root:

```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Debug
```

With Visual Studio 2026, use `-G "Visual Studio 18 2026"` instead.

If you prefer Ninja and have a configured MSVC developer shell:

```powershell
cmake -S . -B build -G Ninja
cmake --build build
```

## Run

From the project root after building:

```powershell
.\build\Debug\InvestorCommandCenter.exe
```

The database is created automatically at:

```text
data/investor_command_center.db
```

Visual Studio debug sessions are configured to use the project root as the working directory.

## Privacy And Local Data

Investor Command Center is local-first. Personal investing records are stored in a local SQLite database at:

```text
data/investor_command_center.db
```

The database file is intentionally ignored by Git. Do not commit real account balances, holdings, transactions, dividend records, brokerage exports, backup files, logs, spreadsheets, or any other personal financial data.

This public repository is for source code and documentation only. Review `git status` before every commit and confirm that local databases, exports, backups, and logs are not staged.

See `docs/Privacy-And-Local-Data.md` for the full data-safety guidance.

## License

Investor Command Center is released under the MIT License. See `LICENSE`.

## Current Limitations

- Prices are manual fields; there are no market data APIs.
- Reports is a placeholder section.
- CSV export buttons are placeholders.
- Account deletion is blocked by SQLite foreign keys when holdings still reference that account.
- Transactions and dividends can optionally reference an account, but do not enforce account foreign keys.
- No authentication, no cloud sync, and no brokerage integration.
- The first configure step downloads third-party source dependencies.
