# Investor Command Center

Investor Command Center is a personal Windows desktop investing tracker written in C++20. It is designed as a private, local-first cockpit for manually tracking accounts, holdings, and long-term portfolio progress.

Subtitle: Long-term progress beats short-term noise.

This app does not connect to brokerage accounts, does not sync to the cloud, and does not provide financial advice or buy/sell recommendations. Stock Research can fetch informational Yahoo Finance data only when explicitly requested; CSV import remains the primary portfolio update workflow.

## Project Intent

Investor Command Center is a personal desktop investing tracker built for local-first manual tracking. The source code is public and open source, but the app follows my personal investing workflow first.

I am building this to unwind and relax in the evening - AI Zombie time and flow coding. The spirit of the app is: "Everything is made up, and the points don't matter" - except they kind of do, because I am making this to track my investments my way.

Bug reports are welcome. Feature requests may be considered, but this is not intended to be a commercial finance product. This app is not financial advice, does not connect to brokerage accounts, and users are responsible for their own financial decisions.

## Current App

- Native Win32 desktop shell using Dear ImGui with a DirectX 11 renderer
- Trading-terminal inspired desktop shell with compact top menu navigation, morning snapshot sidebar, and modular workspace panels
- Top menu access for Dashboard, Accounts, Holdings, Transactions, Dividends, Goals, Watchlist, Stock Research, Reports, Import CSV, and Settings
- Active page shown in the workspace header while the sidebar focuses on daily account/watchlist context
- Dashboard performance panels based on local accounts, holdings, transactions, dividends, and portfolio snapshots
- Customizable Dashboard layout with local reorder, hide/show, and reset controls
- Controlled Dashboard chart panels for allocation, performance, and income/gains with local data, time-range, and chart-type preferences
- Theme support for Dark Command Center and Light Trading Terminal
- Account create, edit, and delete workflow
- Holding create, edit, and delete workflow
- Transaction create, edit, delete, and search workflow with buy/sell fields, fees, and realized gain/loss tracking
- Capital gains allocation helper for Sell transactions, driven by local user-defined percentages in Settings
- Dividend create, edit, delete, and search workflow with month, year, and lifetime totals
- Goal create, edit, delete, and search workflow with progress bars and optional linked account value tracking
- Multiple named watchlists with manager controls, sidebar assignment, selected-list item CRUD, search, and user-defined buy/sell price signals
- Research menu with a Stock Research page backed by a Yahoo Finance market data provider abstraction
- Local market quote cache for user-requested research lookups
- Polished Stock Research panels with quote summary, key metrics, fallback/cache status, and watchlist shortcut
- Manual Dashboard current-price refresh that uses market data quotes as a session-only display overlay
- Reusable calendar picker controls for transaction, dividend, and goal date entry
- Holdings CSV import with local file selection, column mapping, preview, validation, repeated upsert imports, and import summaries
- Import batch tracking for local CSV import metadata
- Portfolio snapshots for local daily/monthly/yearly movement comparisons, created automatically after CSV imports
- Local SQLite database backup button with configurable backup folder and local reminder settings
- Configurable SQLite database location with copy/verify/switch-on-restart behavior
- SQLite database initialization at `data/investor_command_center.db`
- SQLite migrations for `accounts`, `holdings`, `transactions`, `dividends`, `goals`, `watchlists`, `watchlist`, `import_batches`, `portfolio_snapshots`, and `dashboard_widgets`
- Basic validation and delete confirmation dialogs
- C++ portfolio calculations for cost basis, market value, and gain/loss

## Tracking Model

CSV import is the normal update workflow. Import a brokerage positions CSV, review the preview, confirm import, and the app updates holdings for the selected account by `account_id + ticker`.

Repeated imports are expected. Existing holdings are updated, new tickers are added, and active holdings missing from the latest CSV for that account are marked inactive instead of being hard deleted.

Each successful CSV import stores import metadata in SQLite and automatically creates or updates a portfolio snapshot for the import date. Manual snapshots remain available as an optional advanced tool.

Holdings show what is currently owned. Transactions explain how those holdings got there.

Buy and sell transactions support a simple average-cost first pass. New buy transactions can increase matching holding shares and update average cost. New sell transactions can reduce matching holding shares and calculate realized gain/loss separately from unrealized holding gain/loss.

Dashboard daily, monthly, and yearly movement is based on local portfolio snapshots. This first pass shows simple dollar movement and may include deposits or withdrawals; contribution-adjusted returns are planned for a future update.

Dashboard layout preferences are stored locally in SQLite. Reordering or hiding dashboard sections changes only the view; it does not change portfolio calculations or stored financial records. `Reset Dashboard Layout` restores the default view.

The Dashboard uses compact terminal-style panels for portfolio review, movement, holdings, recent activity, realized gains, dividends, snapshot history, and allocation views. It remains a personal tracking dashboard and does not provide trading recommendations.

Navigation is handled from the top menu bar. The left rail is a morning snapshot panel for portfolio totals, account summaries, two compact watchlist slices, and local version/database information. It is not a page navigation menu.

Dashboard chart preferences are stored locally in SQLite. The chart panels can switch between common investing views: allocation bars for asset/account/holding allocation, line charts for snapshot-based performance, and monthly bar charts for dividends or realized gains. These charts are for personal review only and do not provide financial advice.

Theme preference is stored locally in SQLite. The Light Trading Terminal theme uses light gray panels, thin borders, subtle blue highlights, and green/red financial movement colors.

Date fields use compact calendar picker controls and are stored in SQLite as `YYYY-MM-DD` text. Manual date entry remains available, but invalid dates are rejected before saving.

Goals can use either a manually entered current amount or a linked account value. Linked goal progress is calculated at runtime from the selected account's local calculated balance: active holdings market value plus account cash balance. This is a tracking convenience only and is not financial advice.

Capital gains allocation rules are user-defined Settings records. For a Sell transaction with a positive realized gain, the Transactions page can show an allocation plan based on those saved percentages. This is a display/calculation helper only; it does not move money, create transfers, provide tax advice, or provide financial advice.

Stock Research is informational only. The Research menu can fetch a quote for a ticker from Yahoo Finance, display available quote/metric fields, clearly label live/fallback/cached/error status, and cache the latest fetched result locally for convenience. Research data may be delayed, unavailable, rate-limited, or incomplete. The Stock Research page itself does not update local holdings.

Watchlists can be organized into named local groups. Existing watchlist items migrate into `Main Watchlist`, and new items belong to the selected active watchlist on the Watchlist page. Deactivating a watchlist hides the list and its items from normal views without deleting the item records.

Two active watchlists can be assigned to the morning sidebar as Sidebar Watchlist 1 and Sidebar Watchlist 2. The sidebar uses the assigned watchlist names as section titles and shows up to 10 rows from each selected watchlist. Sidebar assignment is stored locally in SQLite and only affects display.

Watchlist price signals are personal saved levels. The Watchlist page can refresh either the selected watchlist or all active watchlists, and the Research menu can explicitly refresh watchlist prices through the same market data provider used by Stock Research. Refreshes update local watchlist price metadata and show simplified `Buy`, `Sell`, or `Hold` user signals. Buy and Sell rows sort above Hold rows on the Watchlist page and in sidebar watchlists. These signals are not recommendations, trading advice, brokerage actions, or money movement.

Database backups are local-only. Configure a backup folder in Settings, then use `Back Up Now` from Settings or the sidebar database area to create a timestamped SQLite backup file. Backup reminders are stored locally and shown inside the app; there are no OS notifications, cloud backup, or sync features.

The database location can be changed in Settings. The app copies the current SQLite database to the selected folder, verifies the copy, saves the configured `database.path`, and uses the new database on the next launch. The old database is not deleted automatically.

Dashboard current-price refresh is optional and manual. It collects active holding tickers, fetches current quote prices through the market data provider, and recalculates dashboard display values using a session-only overlay. It does not change shares, cost basis, imported holdings, transactions, or snapshots. Cost basis remains local data from manual entry, transactions, or CSV import.

## Build Requirements

- Windows 10 or Windows 11
- CMake 3.24 or newer
- Visual Studio 2026, or Visual Studio Build Tools with the Desktop development with C++ workload
- CMake tools for Windows
- Windows 11 SDK
- Git
- Internet access for the first configure step so CMake can fetch Dear ImGui and the SQLite amalgamation

## Visual Studio 2026 Setup

1. Install Visual Studio 2026 with `Desktop development with C++`.
2. Include `CMake tools for Windows`.
3. Include the Windows 11 SDK.
4. Open Visual Studio 2026.
5. Choose `Open a local folder`.
6. Select the repository root.
7. In the CMake target/preset dropdown, choose `vs2026-x64-debug`.
8. Build.
9. Run the `InvestorCommandCenter` target.

The repository uses CMake presets and should not require committing generated `.sln`, `.vcxproj`, `.vs`, `out`, or `build` files.

## Command-Line Build

Debug:

```powershell
cmake --preset vs2026-x64-debug
cmake --build --preset vs2026-x64-debug
```

Release:

```powershell
cmake --preset vs2026-x64-release
cmake --build --preset vs2026-x64-release
```

The presets use the Visual Studio 2026 CMake generator with x64 architecture. Generated files are written under `out/`, which is ignored by Git.

## GitHub Actions / CI

CI builds should use the tracked CMake presets:

```powershell
cmake --preset vs2026-x64-release
cmake --build --preset vs2026-x64-release
```

Do not rely on `CMakeUserPresets.json`, `.vs/`, `out/`, or `build/` contents. Those are local/generated files and are intentionally ignored by Git.

## Run

From the project root after building:

```powershell
.\out\build\vs2026-x64-debug\Debug\InvestorCommandCenter.exe
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

The database file and backup files are intentionally ignored by Git. Do not commit real account balances, holdings, transactions, dividend records, brokerage exports, backup files, logs, spreadsheets, or any other personal financial data.

Database files can be kept outside the repository by changing the database location in Settings. If the active database is inside the repository, the app shows a warning in the sidebar and Settings.

This public repository is for source code and documentation only. Review `git status` before every commit and confirm that local databases, exports, backups, and logs are not staged.

See `docs/Privacy-And-Local-Data.md` for the full data-safety guidance.

CSV imports are local-only. Do not store real brokerage CSV files in the repository. See `docs/CSV-Import.md`.

Research quote cache records are stored locally in SQLite and are not brokerage-verified records. They should not be treated as authoritative account data.

## License

Investor Command Center is released under the MIT License. See `LICENSE`.

## Current Limitations

- Dashboard current-price refresh is manual/session-only and does not persist prices back to holdings yet.
- Holdings table prices remain local/manual unless updated through CSV import or direct user entry.
- Yahoo Finance research endpoints are unofficial/public-facing and may fail, rate-limit, or change.
- Reports is a placeholder section.
- CSV export buttons are placeholders.
- CSV import currently targets holdings only; transaction CSV import is planned.
- Dashboard panels use Move Up/Move Down customization; freeform docking is not implemented yet.
- Snapshot-based daily/monthly/yearly performance is not contribution-adjusted yet.
- CSV import upserts by account and ticker; full import review/merge tooling is planned.
- Date picker controls are intentionally simple; locale-specific date display formats are not implemented.
- Full transaction reconciliation for edited or deleted historical transactions is planned future work.
- FIFO/LIFO and tax-lot accounting are not implemented; realized gain/loss currently uses average cost basis.
- Capital gains allocation is a local planning display only and does not create money movement, transfers, or tax guidance.
- Account deletion is blocked by SQLite foreign keys when holdings still reference that account.
- Transactions and dividends can optionally reference an account, but do not enforce account foreign keys.
- No authentication, no cloud sync, and no brokerage integration.
- The first configure step downloads third-party source dependencies.
