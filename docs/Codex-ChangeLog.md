# Codex Change Log

## 2026-05-28 CSV Mapping UI and Import Preview Alignment

### Changed Files

- `docs/CSV-Import.md`
- `docs/Codex-ChangeLog.md`
- `src/services/HoldingsCsvImport.hpp`
- `src/services/HoldingsCsvImport.cpp`
- `src/ui/ImportCsvView.hpp`
- `src/ui/ImportCsvView.cpp`

### Behavior Added

- Replaced the floating field-to-column dropdowns with a column mapping table.
- Added one mapping row per CSV column with CSV header, sample value, import field dropdown, parsed/calculated preview, and status.
- Added import field options for `Ignore Column`, ticker, asset name, shares, current price, average cost, total cost basis, gain/loss dollar, gain/loss percent, asset type, and notes.
- Added Schwab defaults for `Gain $ (Gain/Loss $)` and `Gain % (Gain/Loss %)`.
- Added an Import Preview table that shows final app fields after mapping, including total cost basis, average cost, market value, gain/loss dollar, gain/loss percent, notes, and row errors/warnings.
- Added manual header row override while keeping automatic header detection.
- Kept Notes isolated to columns explicitly mapped to Notes.

### UI Corrections

- Fixed ambiguous dropdown labels by putting every dropdown in the same row as its CSV column header.
- Fixed the previous visual mismatch where adjacent dropdown labels could appear to describe the wrong field.
- Clarified that Schwab `Cost Basis` maps to total cost basis, not average cost.
- Added section titles for Import Account, File, Header Detection, Column Mapping, Import Preview, and Validation Summary.

### Completed Validation

- Built the Debug preset after the mapping UI redesign.
- Built the Release preset after the mapping UI redesign.
- Ran a fake Schwab positions CSV smoke test outside the repository.
- Confirmed row 3 header detection, manual header row reload, summary row skipping, Schwab default mappings, Notes isolation, total cost basis average-cost calculation, gain/loss parsing, valid-row preview, and invalid-row blocking.
- Confirmed the service model maps CSV columns to import fields instead of relying on unlabeled field dropdowns.
- Confirmed gain/loss columns are parsed for preview/reference but are not stored in holdings.

### Known Issues

- Interactive visual QA should still be performed in the running desktop app with a fake Schwab CSV.
- Transaction CSV import remains future work.

## 2026-05-28 Schwab Positions CSV Import Support

### Changed Files

- `docs/CSV-Import.md`
- `docs/Codex-ChangeLog.md`
- `src/services/HoldingsCsvImport.hpp`
- `src/services/HoldingsCsvImport.cpp`
- `src/ui/ImportCsvView.cpp`

### Behavior Added

- Added Schwab-style positions CSV header detection so metadata/title rows before the actual holdings header are ignored.
- Added tracking for detected header row, skipped metadata rows, skipped blank rows, and skipped summary rows.
- Added skipping for Schwab summary/cash rows: `Futures Cash`, `Futures Positions Market Value`, `Cash & Cash Investments`, and `Positions Total`.
- Added total cost basis mapping using `Cost Basis` / `Total Cost Basis`.
- Added average cost calculation from total cost basis divided by shares when direct average cost is unavailable.
- Added non-blocking warnings when average cost is missing and imported as `0.00`.
- Kept the hard required holding fields to ticker, shares, and current price; missing asset names now fall back to ticker with a warning.
- Added Schwab asset type normalization from `Equity` to `Stock` and `ETFs & Closed End Funds` to `ETF`.
- Improved number parsing for dollar signs, commas, percent signs, blanks, `--`, negative values, and parentheses.
- Updated Import CSV preview stats to show valid, blocked, warning, detected-header, metadata, blank, and summary row counts.

### Completed Validation

- Validated the importer with a fake Schwab-style CSV created outside the repository.
- Confirmed the fake Schwab header row is detected as row 3.
- Confirmed `Symbol`, `Description`, `Qty (Quantity)`, `Price`, `Cost Basis`, and `Asset Type` map correctly.
- Confirmed average cost calculates from `Cost Basis / shares`.
- Confirmed `Equity` maps to `Stock` and `ETFs & Closed End Funds` maps to `ETF`.
- Confirmed summary/cash rows are skipped.
- Confirmed invalid fake rows show errors while valid fake rows preview as importable.
- Built Debug and Release presets successfully after the Schwab importer changes.

### Known Issues

- The importer now supports Schwab-style positions files, but transaction CSV import is still planned future work.
- Real CSV files should remain local and must not be committed.

## 2026-05-28 Calculated Balances and Holdings CSV Import

### Changed Files

- `.gitignore`
- `CMakeLists.txt`
- `README.md`
- `docs/CSV-Import.md`
- `docs/Data-Model.md`
- `docs/Product-Brief.md`
- `docs/Roadmap.md`
- `docs/Privacy-And-Local-Data.md`
- `docs/Codex-ChangeLog.md`
- `src/app/App.hpp`
- `src/app/App.cpp`
- `src/app/AppState.hpp`
- `src/app/AppState.cpp`
- `src/services/PortfolioCalculator.hpp`
- `src/services/PortfolioCalculator.cpp`
- `src/services/HoldingsCsvImport.hpp`
- `src/services/HoldingsCsvImport.cpp`
- `src/ui/AccountsView.cpp`
- `src/ui/ImportCsvView.hpp`
- `src/ui/ImportCsvView.cpp`

### Behavior Added

- Changed account balance display logic so account balance is calculated as holdings market value plus cash balance.
- Kept `cash_balance` as the user-entered account cash field.
- Kept `accounts.current_balance` only for backward compatibility; it is no longer used as the authoritative displayed balance.
- Updated Accounts UI so calculated balance is read-only and the form no longer asks for manual current balance.
- Updated dashboard portfolio value to use centralized calculated account balances.
- Added an Import CSV section under Tools.
- Added first-pass holdings CSV import with local file picker, flexible column mapping, preview, row validation, duplicate blocking, and import of valid rows into SQLite.
- Added `.gitignore` protection for `imports/` and `data/imports/`.
- Added CSV import documentation and updated roadmap priority from CSV export/import later to CSV import now.

### Completed Validation

- Built the Debug `InvestorCommandCenter` target successfully after account balance and CSV import changes.
- Confirmed holdings calculations still use `costBasis = shares * averageCost`, `marketValue = shares * currentPrice`, dollar gain/loss, and percentage gain/loss with zero-cost protection.
- Confirmed CSV source files remain ignored by Git via `*.csv`, `imports/`, and `data/imports/`.

### Known Issues

- Holdings CSV import is the first supported importer; transaction CSV import is planned.
- CSV preview/import was implemented in UI and service code, but interactive file-picker validation still needs a hands-on app pass with fake local CSV files.
- Duplicate handling currently blocks exact `account_id + ticker + asset_name` matches.

## 2026-05-28 Windows Release Workflow Presets

### Changed Files

- `.github/workflows/windows-release.yml`
- `docs/Codex-ChangeLog.md`

### Behavior Added

- Added a Windows release workflow that configures with `cmake --preset vs2026-x64-release`.
- Added release build step using `cmake --build --preset vs2026-x64-release`.
- Packaged `InvestorCommandCenter.exe` from the actual preset output directory: `out/build/vs2026-x64-release/Release`.
- Uploaded a zip artifact containing only the executable, `LICENSE`, `README.md`, and `docs/Privacy-And-Local-Data.md`.
- Added an artifact safety check that fails packaging if database files, exports, spreadsheets, brokerage export files, or logs appear in the package folder.

### Completed Validation

- Confirmed the workflow references the tracked CMake release preset.
- Built the local `vs2026-x64-release` preset successfully.
- Confirmed artifact packaging reads from the actual release preset executable path.
- Confirmed package contents intentionally exclude `data/`, exports, backups, logs, and local/private files.
- Ran a local package-folder validation against the workflow's forbidden private/generated data patterns.
- Confirmed `.vs`, `out`, `build`, and `CMakeUserPresets.json` remain ignored and unstaged locally.

### Known Issues

- The workflow uses `windows-2025-vs2026`, so it depends on GitHub-hosted runner availability for the Visual Studio 2026 image.
- The workflow uploads a build artifact; it does not create a GitHub Release entry yet.

## 2026-05-28 Visual Studio 2026 CMake Presets

### Changed Files

- `CMakePresets.json`
- `README.md`
- `docs/Codex-ChangeLog.md`

### Behavior Added

- Added Visual Studio 2026 CMake configure presets for `vs2026-x64-debug` and `vs2026-x64-release`.
- Added matching CMake build presets for Debug and Release.
- Configured preset output under `out/build/${presetName}` and install output under `out/install/${presetName}`.
- Kept CMake as the build system; no `.sln` or `.vcxproj` project conversion was added.
- Updated README instructions for opening the repository directly in Visual Studio 2026 as a local CMake folder.
- Updated command-line build instructions to use `cmake --preset` and `cmake --build --preset`.

### Completed Validation

- Confirmed `CMakePresets.json` parses as valid JSON.
- Confirmed CMake lists `vs2026-x64-debug` and `vs2026-x64-release`.
- Confirmed Debug preset configures.
- Confirmed Debug preset builds.
- Confirmed Release preset configures.
- Confirmed Release preset builds.
- Confirmed `.vs`, `out`, `build`, and `CMakeUserPresets.json` are ignored and not staged.

### Known Issues

- The presets use the Visual Studio 2026 generator, so GitHub Actions runners need Visual Studio 2026 Build Tools or an equivalent image with the `Visual Studio 18 2026` CMake generator available.
- `CMAKE_EXPORT_COMPILE_COMMANDS` is not enabled because the Visual Studio generator does not support compile command export in the same way Ninja and Makefile generators do.
- Interactive Visual Studio folder opening was not automated from this terminal; preset parsing/listing and configure/build were validated with the Visual Studio 2026 bundled CMake.

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
