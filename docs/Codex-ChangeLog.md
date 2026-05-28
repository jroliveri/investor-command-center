# Codex Change Log

## 2026-05-28 License and Local Data Safety

### Changed Files

- `.gitignore`
- `LICENSE`
- `README.md`
- `docs/Privacy-And-Local-Data.md`
- `docs/Codex-ChangeLog.md`
- `CMakeLists.txt`
- `src/**/*.hpp`
- `src/**/*.cpp`

### Behavior Added

- Added the MIT License with copyright holder Joseph R. Oliveri.
- Added SPDX license identifiers to project-owned C++ source/header files and `CMakeLists.txt`.
- Expanded `.gitignore` protections for local SQLite databases, journals, WAL/SHM files, exports, backups, logs, spreadsheets, brokerage export formats, local user data, secrets, `.env` files, IDE files, and generated build output.
- Kept `data/.gitkeep` explicitly trackable while ignoring local database files under `data/`.
- Added privacy/local-data documentation that distinguishes public source code from private local investing data.
- Updated the README with MIT license and local data privacy guidance.

### Completed Validation

- Ran `git status --short --ignored`.
- Confirmed `LICENSE` exists.
- Confirmed `.gitignore` contains local data protections.
- Confirmed no local database, export, backup, log, or spreadsheet files are tracked by Git.
- Confirmed `data/.gitkeep` is not ignored and remains available to track.
- Confirmed the local SQLite database remains on disk but ignored by Git.
- Built the Debug `InvestorCommandCenter` target successfully after adding SPDX headers.

### Tracked Private Files Removed

- None. Only `.gitattributes` was tracked before this safety pass, so no `git rm --cached` action was needed.

## 2026-05-28 Dashboard Polish and Settings

### Changed Files

- `CMakeLists.txt`
- `docs/Codex-ChangeLog.md`
- `src/app/App.hpp`
- `src/app/App.cpp`
- `src/repositories/TransactionRepository.cpp`
- `src/repositories/DividendRepository.cpp`
- `src/repositories/GoalRepository.cpp`
- `src/ui/UiTheme.hpp`
- `src/ui/UiTheme.cpp`
- `src/ui/DashboardView.cpp`
- `src/ui/AccountsView.cpp`
- `src/ui/HoldingsView.cpp`
- `src/ui/TransactionsView.cpp`
- `src/ui/DividendsView.cpp`
- `src/ui/GoalsView.cpp`
- `src/ui/WatchlistView.cpp`
- `src/ui/SettingsView.hpp`
- `src/ui/SettingsView.cpp`
- `src/util/Money.hpp`
- `src/util/Money.cpp`
- `src/util/Date.hpp`
- `src/util/Date.cpp`

### Behavior Added

- Reworked the dashboard into a denser desktop overview with stronger metric cards and calmer spacing.
- Added chart panels for portfolio allocation, dividends by month, and portfolio value over time.
- Added a real Settings page with app version, absolute database location, data privacy note, CSV export placeholders, and backup reminder.
- Reorganized the sidebar into Overview, Records, Planning, and System groups.
- Added app version labeling in the sidebar and Settings page.
- Improved empty states across record pages.
- Improved table readability with reorderable/hideable columns and consistent spacing.
- Added reusable UI helpers for metric cards, empty states, badges, and form errors.
- Improved number, currency, quantity, and signed percentage formatting.
- Added clearer date validation messages for transaction, dividend, and goal dates.

### Completed Validation

- Built the Debug `InvestorCommandCenter` target successfully after the polish pass.
- Smoke-launched the app from the project root and confirmed the database file exists.

### Known Issues

- CSV export controls are placeholders only.
- Portfolio value over time is a visual placeholder until snapshot history is added.
- Reports remains a placeholder.
- No external APIs, brokerage integrations, or cloud sync were added.

## 2026-05-28 Broad Tracker Expansion

### Changed Files

- `.gitignore`
- `CMakeLists.txt`
- `README.md`
- `docs/Product-Brief.md`
- `docs/Data-Model.md`
- `docs/Roadmap.md`
- `docs/Codex-ChangeLog.md`
- `src/app/App.hpp`
- `src/app/App.cpp`
- `src/app/AppState.hpp`
- `src/db/Migrations.cpp`
- `src/models/Transaction.hpp`
- `src/models/Dividend.hpp`
- `src/models/Goal.hpp`
- `src/models/WatchlistItem.hpp`
- `src/repositories/TransactionRepository.hpp`
- `src/repositories/TransactionRepository.cpp`
- `src/repositories/DividendRepository.hpp`
- `src/repositories/DividendRepository.cpp`
- `src/repositories/GoalRepository.hpp`
- `src/repositories/GoalRepository.cpp`
- `src/repositories/WatchlistRepository.hpp`
- `src/repositories/WatchlistRepository.cpp`
- `src/services/PortfolioCalculator.hpp`
- `src/services/PortfolioCalculator.cpp`
- `src/ui/AccountsView.hpp`
- `src/ui/AccountsView.cpp`
- `src/ui/HoldingsView.hpp`
- `src/ui/HoldingsView.cpp`
- `src/ui/DashboardView.cpp`
- `src/ui/TransactionsView.hpp`
- `src/ui/TransactionsView.cpp`
- `src/ui/DividendsView.hpp`
- `src/ui/DividendsView.cpp`
- `src/ui/GoalsView.hpp`
- `src/ui/GoalsView.cpp`
- `src/ui/WatchlistView.hpp`
- `src/ui/WatchlistView.cpp`
- `src/util/Date.hpp`
- `src/util/Date.cpp`

### Behavior Added

- Added SQLite migrations for `transactions`, `dividends`, `goals`, and `watchlist`.
- Added repository classes and validation for transactions, dividends, goals, and watchlist items.
- Added CRUD UI for Transactions, Dividends, Goals, and Watchlist.
- Added search/filter inputs for Accounts, Holdings, Transactions, Dividends, Goals, and Watchlist tables.
- Added delete confirmation dialogs for all new CRUD sections.
- Expanded dashboard data to include recent transactions, dividend totals, goal progress, and watchlist priority counts.
- Added dividend totals for current month, current year, and lifetime.
- Added goal progress bars.
- Added watchlist priority badges.

### Completed Validation

- Configured with Visual Studio bundled CMake 4.2.3 using the `Visual Studio 18 2026` generator.
- Built the Debug `InvestorCommandCenter` target successfully.
- Smoke-launched the app from the project root and confirmed the SQLite database file exists.

### Recommended Manual Validation

- Add, edit, search, and delete a transaction.
- Add, edit, search, and delete a dividend.
- Add, edit, search, and delete a goal.
- Add, edit, search, and delete a watchlist item.
- Confirm dashboard recent transaction feed updates.
- Confirm dividend month, year, and lifetime totals update.
- Confirm goal progress bars update.
- Confirm watchlist priority counts and badges update.

### Known Issues

- Date fields are text inputs and expect consistent `YYYY-MM-DD` values for dashboard month/year grouping.
- Reports and Settings are still placeholders.
- Market prices remain manual fields.
- ImPlot is not wired yet; charting remains a placeholder.

## 2026-05-27 MVP Scaffold

### Changed Files

- `CMakeLists.txt`
- `.gitignore`
- `README.md`
- `docs/Product-Brief.md`
- `docs/Data-Model.md`
- `docs/Roadmap.md`
- `docs/Codex-ChangeLog.md`
- `data/.gitkeep`
- `src/main.cpp`
- `src/app/App.hpp`
- `src/app/App.cpp`
- `src/app/AppState.hpp`
- `src/app/AppState.cpp`
- `src/db/Database.hpp`
- `src/db/Database.cpp`
- `src/db/Migrations.hpp`
- `src/db/Migrations.cpp`
- `src/models/Account.hpp`
- `src/models/Holding.hpp`
- `src/repositories/AccountRepository.hpp`
- `src/repositories/AccountRepository.cpp`
- `src/repositories/HoldingRepository.hpp`
- `src/repositories/HoldingRepository.cpp`
- `src/services/PortfolioCalculator.hpp`
- `src/services/PortfolioCalculator.cpp`
- `src/ui/DashboardView.hpp`
- `src/ui/DashboardView.cpp`
- `src/ui/AccountsView.hpp`
- `src/ui/AccountsView.cpp`
- `src/ui/HoldingsView.hpp`
- `src/ui/HoldingsView.cpp`
- `src/ui/UiTheme.hpp`
- `src/ui/UiTheme.cpp`
- `src/util/Money.hpp`
- `src/util/Money.cpp`
- `src/util/Date.hpp`
- `src/util/Date.cpp`

### Behavior Added

- Added a C++20 CMake project for a Windows-first native desktop application.
- Added Dear ImGui Win32/DX11 UI scaffolding.
- Added SQLite database creation at `data/investor_command_center.db`.
- Added migration tracking and migrations for `accounts` and `holdings`.
- Added repository classes for account and holding CRUD.
- Added dashboard summary calculations based on local records.
- Added account and holding forms with basic validation.
- Added delete confirmation dialogs.
- Added placeholder pages for Transactions, Dividends, Goals, Watchlist, Reports, and Settings.

### Completed Validation

- Configured with Visual Studio bundled CMake 4.2.3 using the `Visual Studio 18 2026` generator.
- Built the Debug `InvestorCommandCenter` target.
- Smoke-launched the app from the project root and closed it after startup.
- Confirmed the database file was created under `data/`.

### Recommended Manual Validation

- Add, edit, and delete an account.
- Add, edit, and delete a holding.
- Confirm dashboard cards update from local records.
- Confirm account deletion is blocked when holdings reference that account.

### Known Issues

- CMake was not available on the current shell PATH during initial local inspection.
- Market prices are manual fields.
- ImPlot is not wired yet; charting remains a placeholder.
- Non-MVP sections are placeholders.
