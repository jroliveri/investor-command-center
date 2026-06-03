# Codex Change Log

## 2026-06-03 Watchlist Signal Simplification

### Changed Files

- `README.md`
- `docs/Data-Model.md`
- `docs/Roadmap.md`
- `docs/Codex-ChangeLog.md`
- `src/app/App.cpp`
- `src/db/Migrations.cpp`
- `src/models/WatchlistItem.hpp`
- `src/repositories/WatchlistRepository.cpp`
- `src/services/WatchlistSignalService.hpp`
- `src/services/WatchlistSignalService.cpp`
- `src/ui/WatchlistView.hpp`
- `src/ui/WatchlistView.cpp`

### Behavior Added

- Simplified visible watchlist signal categories to `Buy`, `Sell`, and `Hold`.
- Added centralized watchlist signal sort rank helpers.
- Added migration support to normalize legacy stored signal labels into `Buy`, `Sell`, or `Hold`.
- Sorted Watchlist page rows by signal first: Buy, then Sell, then Hold.
- Sorted sidebar watchlist rows with the same signal-first order so the top 10 favors Buy/Sell user signals.
- Updated signal badges and pop-up wording to reinforce that signals are user-defined thresholds, not financial advice or trade actions.
- Kept overlapping triggered thresholds visible as `Hold` with amber warning styling.

### Validation

- Built the Debug CMake preset with Visual Studio's bundled CMake executable.
- Built the Release CMake preset with Visual Studio's bundled CMake executable.

### Known Issues

- Manual desktop click-through is still recommended to confirm final badge colors and row ordering with local watchlist data.

## 2026-06-02 Sidebar Watchlist Assignment

### Changed Files

- `README.md`
- `docs/Data-Model.md`
- `docs/Roadmap.md`
- `docs/Codex-ChangeLog.md`
- `src/app/App.hpp`
- `src/app/App.cpp`
- `src/db/Migrations.cpp`
- `src/models/Watchlist.hpp`
- `src/repositories/WatchlistRepository.cpp`
- `src/ui/WatchlistView.hpp`
- `src/ui/WatchlistView.cpp`

### Behavior Added

- Added `show_in_sidebar` and `sidebar_slot` fields to `watchlists`.
- Added migration support for two sidebar watchlist slots.
- Updated the Watchlist Manager with a Sidebar dropdown: `Not shown`, `Sidebar Watchlist 1`, and `Sidebar Watchlist 2`.
- Restricted sidebar display to active watchlists.
- Automatically clears a previous sidebar slot assignment when another active watchlist is assigned to the same slot.
- Updated the morning sidebar so Watchlist panels are driven by saved watchlist assignments instead of fixed slices of the combined list.
- Sidebar watchlist panels now use the assigned watchlist name as the section title and show up to 10 rows.
- Added clear sidebar empty states for unassigned slots and assigned watchlists with no items.
- Added `Refresh Selected Watchlist` and `Refresh All Watchlists` actions on the Watchlist page.
- Updated watchlist refresh status messages to include the watchlist scope, refreshed/failed counts, last refresh time, and Yahoo Finance source/cached status.

### Validation

- Built the Debug CMake preset with Visual Studio's bundled CMake executable.
- Built the Release CMake preset with Visual Studio's bundled CMake executable.

### Known Issues

- Manual desktop click-through is still recommended to confirm sidebar assignment persistence, slot replacement behavior, empty states, and refresh summaries against local watchlist data.
- Yahoo Finance data remains informational and may be delayed, cached, unavailable, or incomplete.

## 2026-06-02 Multiple Watchlist Management

### Changed Files

- `CMakeLists.txt`
- `README.md`
- `docs/Data-Model.md`
- `docs/Roadmap.md`
- `docs/Codex-ChangeLog.md`
- `src/app/App.cpp`
- `src/app/AppState.hpp`
- `src/db/Migrations.cpp`
- `src/models/Watchlist.hpp`
- `src/models/WatchlistItem.hpp`
- `src/repositories/WatchlistRepository.hpp`
- `src/repositories/WatchlistRepository.cpp`
- `src/ui/WatchlistView.hpp`
- `src/ui/WatchlistView.cpp`

### Behavior Added

- Added a `watchlists` SQLite table for named local watchlist groups.
- Added a migration that creates `Main Watchlist` and assigns existing watchlist rows to it.
- Added `watchlist_id` to watchlist items while preserving existing signal, price refresh, priority, and notes fields.
- Expanded `WatchlistRepository` with group CRUD, activation/deactivation, item counts, ordering, default-list lookup, and selected-watchlist item loading.
- Updated app state reloads to load watchlist groups and active-group watchlist items.
- Redesigned the Watchlist page with a `Watchlist Manager` section and a selected `Watchlist Items` section.
- Added create, rename, description edit, activate/deactivate, and Move Up/Move Down controls for watchlist groups.
- Added confirmation before deactivating a watchlist, including the message for lists that contain items.
- Scoped Watchlist item tables, add/edit/delete actions, search, priority summaries, and price refreshes to the selected active watchlist.
- Kept watchlist price signals as user-defined tracking levels only; no advice, brokerage sync, cloud sync, or money movement was added.

### Validation

- Built the Debug CMake preset with Visual Studio's bundled CMake executable.
- Built the Release CMake preset with Visual Studio's bundled CMake executable.

### Known Issues

- Manual desktop click-through is still recommended to confirm group creation, deactivation confirmation, selected-list filtering, and persistence against a local database with existing watchlist records.
- Sidebar watchlist assignment controls are not implemented in this pass; the sidebar continues to show the first active watchlist items in the existing compact slices.

## 2026-06-02 Database Location Settings

### Changed Files

- `README.md`
- `docs/Data-Model.md`
- `docs/Roadmap.md`
- `docs/Privacy-And-Local-Data.md`
- `docs/Codex-ChangeLog.md`
- `src/app/App.hpp`
- `src/app/App.cpp`
- `src/services/DatabaseBackupService.hpp`
- `src/services/DatabaseBackupService.cpp`
- `src/ui/SettingsView.hpp`
- `src/ui/SettingsView.cpp`

### Behavior Added

- Added startup support for a configured SQLite database path stored as `database.path`.
- Added fallback behavior: if the configured database path is missing or invalid, the app uses the default local database and shows a warning.
- Added a Settings `Database Location` section with current database path, selected folder path, Choose Database Folder, Move Database Now, and Open Database Folder.
- Added confirmation before moving the database location.
- Implemented copy + SQLite verification + saved-path switching for database relocation.
- Kept the old database file intact; it is not deleted automatically.
- Added repository-folder warnings in Settings and the sidebar database info area.
- Updated the sidebar database info to show the current active database path instead of the hardcoded default path.

### Validation

- Built the Debug CMake preset.
- Built the Release CMake preset.
- Ran `git diff --check`.
- Confirmed no files are staged.
- Confirmed local SQLite databases, CSV/import/export/backup/log folders, `.vs`, `out`, `build`, object files, and `CMakeUserPresets.json` are not staged.

### Known Issues

- Manual desktop click-through is still recommended for folder selection, move confirmation, restart behavior, and repository-folder warning display.

## 2026-06-02 Local Database Backup

### Changed Files

- `.gitignore`
- `CMakeLists.txt`
- `README.md`
- `docs/Data-Model.md`
- `docs/Roadmap.md`
- `docs/Privacy-And-Local-Data.md`
- `docs/Codex-ChangeLog.md`
- `src/app/App.hpp`
- `src/app/App.cpp`
- `src/app/AppState.hpp`
- `src/models/DatabaseBackup.hpp`
- `src/services/DatabaseBackupService.hpp`
- `src/services/DatabaseBackupService.cpp`
- `src/ui/SettingsView.hpp`
- `src/ui/SettingsView.cpp`

### Behavior Added

- Added local SQLite database backup support using the SQLite backup API.
- Added `Back Up Now` in the sidebar database/version area.
- Added a `Tools > Back Up Now` menu action.
- Added a Settings `Database Backup` section with backup folder path, Windows folder picker, Back Up Now, reminder enabled checkbox, frequency selector, last backup timestamp, and due status.
- Stored backup settings locally in `app_settings` under `database.backup_folder`, `database.backup_reminder_enabled`, `database.backup_reminder_frequency`, and `database.last_backup_at`.
- Added local reminder due text in the sidebar backup section.
- Added timestamped backup filenames like `investor_command_center_backup_YYYY-MM-DD_HHMMSS.db`.
- Expanded `.gitignore` to ignore root-level `*.db`, `*.sqlite`, and `*.sqlite3` backup/database files.

### Validation

- Built the Debug CMake preset.
- Built the Release CMake preset.
- Ran `git diff --check`.
- Confirmed no files are staged.
- Confirmed local SQLite databases, CSV/import/export/backup/log folders, `.vs`, `out`, `build`, object files, and `CMakeUserPresets.json` are not staged.

### Known Issues

- Manual desktop click-through is still recommended for the Windows folder picker and Back Up Now button.
- Restore from backup is not implemented yet.

## 2026-06-02 Morning Snapshot Sidebar

### Changed Files

- `README.md`
- `docs/Roadmap.md`
- `docs/Codex-ChangeLog.md`
- `src/app/App.hpp`
- `src/app/App.cpp`

### Behavior Added

- Redesigned the left sidebar as a morning snapshot panel instead of a navigation/status area.
- Removed the program name, version label, current page display, and top-menu navigation hint from the top of the sidebar.
- Kept top menu navigation as the primary navigation path.
- Updated Account Information to show total portfolio value, holdings value, cash balance, unrealized gain/loss, realized gain/loss, daily movement when available, last CSV import, and last price refresh when available.
- Updated the Accounts sidebar panel to show account name, calculated account value, cash balance, and status.
- Split the compact watchlist sidebar into `Watchlist 1` for rows 1-10 and `Watchlist 2` for rows 11-20.
- Added compact watchlist signal labels using the existing watchlist signal calculation logic.
- Moved app version, SQLite connection status, and current database path into a visually separated bottom sidebar footer.

### Validation

- Built the Debug CMake preset.
- Built the Release CMake preset.
- Ran `git diff --check`.
- Confirmed no files are staged.
- Confirmed local SQLite databases, CSV/import/export/backup/log folders, `.vs`, `out`, `build`, object files, and `CMakeUserPresets.json` are not staged.

### Known Issues

- Manual desktop launch/click-through is still recommended to confirm final sidebar spacing on the target monitor and DPI.

## 2026-06-01 Watchlist Price Signals

### Changed Files

- `CMakeLists.txt`
- `README.md`
- `docs/Data-Model.md`
- `docs/Roadmap.md`
- `docs/Codex-ChangeLog.md`
- `src/app/App.hpp`
- `src/app/App.cpp`
- `src/app/AppState.hpp`
- `src/db/Migrations.cpp`
- `src/models/WatchlistItem.hpp`
- `src/models/WatchlistPriceRefresh.hpp`
- `src/repositories/WatchlistRepository.cpp`
- `src/services/WatchlistSignalService.hpp`
- `src/services/WatchlistSignalService.cpp`
- `src/ui/WatchlistView.hpp`
- `src/ui/WatchlistView.cpp`

### Behavior Added

- Added local watchlist buy and sell signal price fields with migration support.
- Kept `target_buy_price` for backward compatibility while using `buy_signal_price` and `sell_signal_price` for new signal logic.
- Added a `WatchlistSignalService` to centralize signal status calculation and explicit watchlist quote refreshes.
- Added `Refresh Watchlist Prices` on the Watchlist page and in the Research menu.
- Updated watchlist refresh to use `MarketDataService`, store current price, last refresh timestamp, source label, and calculated signal status locally.
- Added Watchlist table columns for current price, buy signal, sell signal, signal badge, last refresh, source, and actions.
- Added signal states for `Buy Signal`, `Sell Signal`, `No Signal`, `No Price`, and `Check Signals`.
- Added clickable signal notice pop-ups clarifying that saved signals are not financial advice, trade actions, brokerage actions, or money movement.
- Added editor validation so buy and sell signal prices cannot be negative, and crossed buy/sell levels are blocked before saving.

### Validation

- Built the Debug CMake preset.
- Built the Release CMake preset.
- Ran `git diff --check`.
- Confirmed no files are staged.
- Confirmed local SQLite databases, CSV/import/export/backup/log folders, `.vs`, `out`, `build`, object files, and `CMakeUserPresets.json` are not staged.

### Known Issues

- Yahoo Finance data may be delayed, unavailable, rate-limited, incomplete, or changed without notice.
- Watchlist signal history and desktop notifications are not implemented.
- Manual visual click-through is still recommended for final Watchlist signal refresh validation with local test data.

## 2026-06-01 Stock Research Light Theme Contrast

### Changed Files

- `docs/Codex-ChangeLog.md`
- `src/ui/UiTheme.hpp`
- `src/ui/UiTheme.cpp`
- `src/ui/StockResearchView.cpp`
- `src/ui/widgets/TerminalPanel.cpp`

### Behavior Added

- Added semantic theme text colors for primary, secondary, muted, warning, success, and danger text.
- Added a page-level semantic text color guard around Stock Research content so plain wrapped text inherits readable contrast.
- Updated Stock Research metric labels, values, helper text, warning text, and status text to use semantic theme colors.
- Removed the plain white default value color from Stock Research metrics so values remain readable on the Light Trading Terminal theme.
- Made `N/A` values muted but still readable.
- Darkened the light-theme warning color so cached/fallback warnings have clear contrast on light panels.
- Updated shared theme helpers and terminal metric cells to use semantic text colors.
- Kept green/red usage focused on movement and status accents.

### Validation

- Built the Debug CMake preset.
- Built the Release CMake preset.
- Ran `git diff --check`.
- Confirmed no files are staged.
- Confirmed local SQLite databases, CSV/import/export/backup/log folders, `.vs`, `out`, `build`, object files, and `CMakeUserPresets.json` are not staged.

### Known Issues

- Manual visual click-through in the running desktop app is still recommended for final light/dark theme inspection.

## 2026-06-01 Stock Research UI Polish

### Changed Files

- `README.md`
- `docs/Data-Model.md`
- `docs/Roadmap.md`
- `docs/Codex-ChangeLog.md`
- `src/db/Migrations.cpp`
- `src/models/MarketQuote.hpp`
- `src/repositories/MarketQuoteCacheRepository.cpp`
- `src/services/YahooFinanceProvider.cpp`
- `src/ui/StockResearchView.hpp`
- `src/ui/StockResearchView.cpp`

### Behavior Added

- Redesigned the Stock Research page with a compact ticker toolbar and polished 2x2 terminal panel layout.
- Added visible toolbar actions for Search / Fetch, Add to Watchlist, Refresh, and Clear.
- Uppercased ticker input as the user types.
- Added a research status strip with provider, last fetched time, quote time, and Live/Cached/Fallback/Error status badge.
- Added warning-style messaging when cached or fallback data is shown because live quote data was unavailable or limited.
- Made Current Price prominent in the Quote Summary panel.
- Added price change dollar and price change percent fields when Yahoo Finance returns them.
- Added clean `N/A` formatting for unavailable quote metrics.
- Added compact large-number formatting for market cap, volume, and average volume.
- Improved the Price / History placeholder copy while keeping historical range buttons disabled.
- Improved Notes / Watchlist messaging so it is clear the action only updates the local watchlist and does not create trades, alerts, recommendations, or brokerage actions.
- Added `price_change` and `price_change_percent` to the local market quote cache through a safe migration.

### Validation

- Built the Debug CMake preset.
- Built the Release CMake preset.
- Ran the temporary market research provider/cache smoke test.
- Ran `git diff --check`.
- Confirmed no files are staged.
- Confirmed local SQLite databases, CSV/import/export/backup/log folders, `.vs`, `out`, `build`, object files, and `CMakeUserPresets.json` are not staged.

### Known Issues

- Historical charting remains a placeholder.
- Yahoo Finance public endpoints can be delayed, unavailable, rate-limited, incomplete, or changed without notice.
- Interactive desktop click-through should still be performed with local test symbols.

## 2026-06-01 Dashboard Current Price Refresh Foundation

### Changed Files

- `README.md`
- `docs/Data-Model.md`
- `docs/Roadmap.md`
- `docs/Codex-ChangeLog.md`
- `src/app/App.hpp`
- `src/app/App.cpp`
- `src/app/AppState.hpp`
- `src/services/DashboardService.hpp`
- `src/services/DashboardService.cpp`
- `src/ui/DashboardView.hpp`
- `src/ui/DashboardView.cpp`
- `src/ui/SettingsView.cpp`

### Behavior Added

- Added manual Dashboard current-price refresh using the existing market data provider service.
- Added Dashboard, Research menu, and Tools menu actions for `Refresh Current Prices`.
- Collected active holding tickers from active accounts before requesting quotes.
- Added session-only dashboard price overrides so refreshed prices update dashboard display calculations without writing to `holdings.current_price`.
- Updated dashboard portfolio value, holdings value, unrealized gain/loss, holdings table, allocation, and top gainers/losers to use refreshed display prices when available.
- Added dashboard price source/status UI with provider, last refresh time, refreshed count, failed count, cached count, and warning messages.
- Added a disabled placeholder for `Create Snapshot From Refreshed Prices`; snapshots are not created automatically.
- Added Settings copy showing dashboard price refresh is manual and does not persist refreshed prices to holdings in this pass.

### Validation

- Built the Debug CMake preset.
- Built the Release CMake preset.
- Re-ran the temporary market research provider/cache smoke test.
- Confirmed the dashboard refresh action uses `MarketDataService` and does not call Yahoo Finance directly from UI code.
- Confirmed refreshed prices are kept in memory and do not overwrite shares, average cost, cost basis, transactions, or snapshots.
- Ran `git diff --check`.
- Confirmed no files are staged.
- Confirmed local SQLite databases, CSV/import/export/backup/log folders, `.vs`, `out`, `build`, object files, and `CMakeUserPresets.json` are not staged.

### Known Issues

- Refreshed prices are session-only and are not persisted to holdings yet.
- Creating snapshots from refreshed prices is still a placeholder.
- Yahoo Finance data can be delayed, unavailable, rate-limited, or incomplete.
- Interactive desktop click-through should still be performed with local test holdings.

## 2026-06-01 Research Menu And Yahoo Finance Foundation

### Changed Files

- `CMakeLists.txt`
- `README.md`
- `docs/Data-Model.md`
- `docs/Product-Brief.md`
- `docs/Release-Notes-Guide.md`
- `docs/Roadmap.md`
- `docs/Codex-ChangeLog.md`
- `src/app/App.hpp`
- `src/app/App.cpp`
- `src/app/AppState.hpp`
- `src/app/AppState.cpp`
- `src/db/Migrations.cpp`
- `src/models/MarketQuote.hpp`
- `src/net/HttpClient.hpp`
- `src/net/HttpClient.cpp`
- `src/repositories/MarketQuoteCacheRepository.hpp`
- `src/repositories/MarketQuoteCacheRepository.cpp`
- `src/services/MarketDataProvider.hpp`
- `src/services/MarketDataProvider.cpp`
- `src/services/MarketDataService.hpp`
- `src/services/MarketDataService.cpp`
- `src/services/YahooFinanceProvider.hpp`
- `src/services/YahooFinanceProvider.cpp`
- `src/ui/SettingsView.cpp`
- `src/ui/StockResearchView.hpp`
- `src/ui/StockResearchView.cpp`

### Behavior Added

- Added a top-level `Research` menu with Stock Research, Refresh Selected Symbol, a disabled dashboard price refresh placeholder, Research Settings, and Data Source / Disclaimer.
- Added a Stock Research page with ticker input, fetch action, data source label, last fetched timestamp, quote summary, key metrics, history placeholder, and local watchlist shortcut.
- Added a `MarketDataProvider` abstraction plus `MarketDataService` so UI code does not call Yahoo Finance directly.
- Added a Windows-first WinHTTP client for explicit HTTPS research requests.
- Added `YahooFinanceProvider` as the first market data provider, with a quote endpoint attempt and chart metadata fallback when the public quote endpoint is rejected or rate-limited.
- Added `market_quote_cache` migration and repository for locally cached, user-requested quote metadata.
- Added cached quote fallback messaging when online fetch fails but a local cached quote exists.
- Added Research Settings copy in Settings, keeping dashboard price refresh off/manual-only for this pass.
- Kept CSV import as the primary portfolio update workflow; Stock Research does not update holdings, dashboard prices, or account calculations.

### Validation

- Built the Debug CMake preset.
- Built the Release CMake preset.
- Confirmed Yahoo Finance quote endpoint can reject unauthenticated requests and the chart endpoint can return metadata, which the provider uses as fallback.
- Ran a temporary market research smoke test for Yahoo Finance provider fallback, quote cache persistence, and invalid-symbol error handling.
- Ran `git diff --check`.
- Confirmed no files are staged.
- Confirmed local SQLite databases, CSV/import/export/backup/log folders, `.vs`, `out`, `build`, object files, and `CMakeUserPresets.json` are not staged.

### Known Issues

- Yahoo Finance public endpoints are unofficial and may reject, rate-limit, or change without notice.
- Historical price charting is still a placeholder.
- Stock Research quote data is informational only and does not update local holdings prices.
- Interactive desktop click-through should still be performed with local test symbols.

## 2026-06-01 Accounts Goals Watchlist Row Actions

### Changed Files

- `docs/Codex-ChangeLog.md`
- `src/repositories/AccountRepository.hpp`
- `src/repositories/AccountRepository.cpp`
- `src/services/PortfolioCalculator.cpp`
- `src/ui/AccountsView.hpp`
- `src/ui/AccountsView.cpp`
- `src/ui/GoalsView.hpp`
- `src/ui/GoalsView.cpp`
- `src/ui/WatchlistView.hpp`
- `src/ui/WatchlistView.cpp`

### Behavior Added

- Fixed Accounts row actions by using stable per-record Edit/Delete button IDs and record-specific modal IDs.
- Deferred Accounts edit/delete popup opening until after table row rendering so Dear ImGui row scope no longer swallows the actions.
- Changed Accounts delete behavior to mark the account `Inactive` instead of hard deleting it.
- Added an Accounts delete warning when active holdings are assigned to the selected account.
- Excluded inactive accounts and their holdings from portfolio/dashboard totals.
- Fixed Goals row actions with stable per-record Edit/Delete button IDs and deferred modal opening.
- Preserved manual-current-amount and linked-account goal settings while editing.
- Fixed Watchlist row actions with stable per-record Edit/Delete button IDs and deferred modal opening.
- Kept Goals and Watchlist delete behavior as hard delete because those tables do not currently have inactive/status fields.

### Validation

- Built the Debug CMake preset.
- Built the Release CMake preset.
- Ran a temporary SQLite smoke test for Account update, Account soft delete to `Inactive`, inactive-account total exclusion, manual Goal update/delete, linked-account Goal edit preservation, and Watchlist update/delete.
- Ran `git diff --check`.
- Confirmed no files are staged.
- Confirmed local SQLite databases, CSV/import/export/backup/log folders, `.vs`, `out`, `build`, object files, and `CMakeUserPresets.json` are not staged.

### Known Issues

- Goals and Watchlist records still hard-delete because those tables do not currently have inactive/status fields.
- Interactive desktop click-through should still be performed with local test records.

## 2026-05-29 Dashboard Chart Controls

### Changed Files

- `CMakeLists.txt`
- `README.md`
- `docs/Data-Model.md`
- `docs/Roadmap.md`
- `docs/Codex-ChangeLog.md`
- `src/app/App.hpp`
- `src/app/App.cpp`
- `src/app/AppState.hpp`
- `src/db/Migrations.cpp`
- `src/models/DashboardChartSetting.hpp`
- `src/repositories/DashboardChartSettingsRepository.hpp`
- `src/repositories/DashboardChartSettingsRepository.cpp`
- `src/services/DashboardLayoutService.cpp`
- `src/ui/DashboardView.hpp`
- `src/ui/DashboardView.cpp`

### Behavior Added

- Removed the duplicate fixed allocation chart behavior by replacing legacy chart widgets with three controlled chart panels.
- Added an Allocation panel with data selector for Asset Type, Account, and Ticker / Holding.
- Added a Performance panel with data selector for Portfolio Value, Holdings Value, Cash Balance, and Unrealized Gain/Loss.
- Added an Income / Gains panel with data selector for Dividends and Realized Gains.
- Added in-panel time range selectors.
- Added in-panel chart type selectors using practical local chart types: Allocation Bars, Line Chart, and Bar Chart.
- Added clear no-data messaging for unsupported or insufficient ranges.
- Added local `dashboard_chart_settings` persistence for chart data mode, time range, and chart type.
- Seeded safe defaults for `allocation_panel`, `performance_panel`, and `income_gains_panel`.
- Ignored legacy fixed chart widget keys so old local layouts do not show duplicate chart panels.

### Validation

- Built the Debug CMake preset.
- Built the Release CMake preset.
- Ran a temporary SQLite smoke test for chart settings migration, default seeding, and setting persistence.
- Smoke-launched the app from an ignored temporary working directory to verify startup migrations without touching the real local database.
- Ran `git diff --check`.
- Confirmed no files are staged.
- Confirmed local SQLite databases, `.vs`, `out`, `build`, and object files are ignored and not staged.

### Known Issues

- Allocation history by asset/account/ticker is not stored yet, so Allocation ranges other than `Latest` show a no-data message.
- Pie/donut rendering is not implemented; allocation uses horizontal allocation bars for reliability.
- Interactive chart control click-through should still be performed in the running desktop app.

## 2026-05-29 Holdings And Dividends Row Actions

### Changed Files

- `docs/Codex-ChangeLog.md`
- `src/ui/HoldingsView.hpp`
- `src/ui/HoldingsView.cpp`
- `src/ui/DividendsView.hpp`
- `src/ui/DividendsView.cpp`

### Behavior Added

- Fixed Holdings row actions by using stable per-record Edit/Delete button IDs.
- Fixed Holdings edit and delete modal opening by deferring popup opens until after table row rendering.
- Added record-specific Holdings modal IDs such as `holding_edit_popup_<id>` and `holding_delete_popup_<id>`.
- Kept Holdings delete as soft delete through the existing inactive status behavior.
- Fixed Dividends row actions with stable per-record Edit/Delete button IDs.
- Fixed Dividends edit and delete modal opening by deferring popup opens until after table row rendering.
- Added record-specific Dividends modal IDs such as `dividend_edit_popup_<id>` and `dividend_delete_popup_<id>`.

### Validation

- Built the Debug CMake preset.
- Built the Release CMake preset.
- Ran a temporary SQLite smoke test for Holding update, Holding soft delete to `Inactive`, Dividend update, and Dividend delete.
- Planned manual click-through for Holdings edit/save, Holdings inactive delete, Dividends edit/save, and Dividends delete in the running desktop UI.
- Ran `git diff --check`.
- Confirmed no files are staged.
- Confirmed local SQLite databases, `.vs`, `out`, `build`, and object files are ignored and not staged.

### Known Issues

- Dividends delete still hard-deletes because dividends do not currently have an inactive/status field.
- Interactive desktop click-through should still be performed with local test records.

## 2026-05-29 Top Menu Navigation

### Changed Files

- `README.md`
- `docs/Roadmap.md`
- `docs/Codex-ChangeLog.md`
- `src/app/App.hpp`
- `src/app/App.cpp`

### Behavior Added

- Made the top menu bar the primary navigation system.
- Added a `View` menu with every main page: Dashboard, Accounts, Holdings, Transactions, Dividends, Goals, Watchlist, Reports, and Settings.
- Added a `Records` menu for record-focused sections.
- Expanded the `Dashboard` menu with Dashboard, Customize Dashboard, Reset Dashboard Layout, Refresh Dashboard, and Create Manual Snapshot.
- Expanded the `Tools` menu with Import CSV, Capital Gains Allocation Settings, Data Privacy / Local Data, and a disabled Backup placeholder.
- Kept File and Help menu access for import, settings, exit, about, and privacy notices.
- Removed the old side page navigation list from the left rail.
- Kept the left rail as an account information and watchlist context column.
- Added active page display in the workspace header and left information rail.

### Validation

- Built the Debug CMake preset.
- Built the Release CMake preset.
- Smoke-launched the app from an ignored temporary working directory to verify startup without touching the real local database.
- Confirmed the old side navigation helper calls are no longer present in `App.cpp`.
- Confirmed top-menu entries cover Dashboard, Accounts, Holdings, Transactions, Dividends, Goals, Watchlist, Reports, Import CSV, and Settings.
- Ran `git diff --check`.
- Confirmed no files are staged.
- Confirmed local SQLite databases, `.vs`, `out`, `build`, and object files are ignored and not staged.

### Known Issues

- The left rail is still present for portfolio context; only the page navigation controls were removed.
- Backup and bug-report menu entries are placeholders only.
- Interactive visual menu click-through should still be performed in the running desktop app.

## 2026-05-29 Capital Gains Allocation Helper

### Changed Files

- `CMakeLists.txt`
- `README.md`
- `docs/Data-Model.md`
- `docs/Roadmap.md`
- `docs/Codex-ChangeLog.md`
- `src/app/App.hpp`
- `src/app/App.cpp`
- `src/app/AppState.hpp`
- `src/db/Migrations.cpp`
- `src/models/CapitalGainAllocationRule.hpp`
- `src/repositories/CapitalGainAllocationRepository.hpp`
- `src/repositories/CapitalGainAllocationRepository.cpp`
- `src/services/CapitalGainAllocationService.hpp`
- `src/services/CapitalGainAllocationService.cpp`
- `src/ui/SettingsView.hpp`
- `src/ui/SettingsView.cpp`
- `src/ui/TransactionsView.hpp`
- `src/ui/TransactionsView.cpp`

### Behavior Added

- Added local `capital_gain_allocation_rules` storage for user-defined realized-gain allocation categories.
- Seeded optional default categories with `0%` values when no allocation rules exist: Checking, Savings, and Reinvest.
- Added a Settings section for adding, editing, deactivating, deleting, and reordering capital gains allocation rules.
- Added active percentage total display with warnings when active rules do not total `100%`.
- Added a Transactions row menu with `View Gain Allocation` for Sell transactions.
- Added a Capital Gains Allocation popup showing transaction details, realized gain/loss, rule percentages, allocation amounts, unallocated amount, and overallocated warning.
- Added a copy-to-clipboard summary for allocation plans.
- Kept allocation behavior as a display helper only; it does not move money, create transfers, provide tax advice, or provide financial advice.

### Validation

- Built the Debug CMake preset.
- Built the Release CMake preset.
- Ran a temporary SQLite smoke test for migration creation, default allocation rule seeding, rule persistence, 25/25/50 allocation math, average-cost fallback, and zero/negative realized gain handling.
- Ran `git diff --check`.
- Confirmed no files are staged.
- Confirmed `data/.gitkeep` remains tracked.
- Confirmed local SQLite databases, `.vs`, `out`, and `build` are ignored and not staged.

### Known Issues

- Allocation rules are simple percentages only; no per-account, tax-lot, or destination-account logic is implemented.
- The helper is shown from Transactions only and is not a dashboard feature yet.

## 2026-05-29 Trading-Terminal Interface Redesign

### Changed Files

- `CMakeLists.txt`
- `README.md`
- `docs/Data-Model.md`
- `docs/Roadmap.md`
- `docs/Codex-ChangeLog.md`
- `src/app/App.hpp`
- `src/app/App.cpp`
- `src/app/AppState.hpp`
- `src/main.cpp`
- `src/db/Migrations.cpp`
- `src/repositories/AppSettingsRepository.hpp`
- `src/repositories/AppSettingsRepository.cpp`
- `src/services/DashboardLayoutService.cpp`
- `src/ui/DashboardView.hpp`
- `src/ui/DashboardView.cpp`
- `src/ui/SettingsView.hpp`
- `src/ui/SettingsView.cpp`
- `src/ui/UiTheme.hpp`
- `src/ui/UiTheme.cpp`
- `src/ui/widgets/TerminalPanel.hpp`
- `src/ui/widgets/TerminalPanel.cpp`

### Behavior Added

- Added a compact desktop top menu with File, Dashboard, records, Import, Reports, Tools, and Help actions.
- Reworked the left side into a professional account column with Account Info, compact account balances, and watchlist-style quick symbols.
- Redesigned the Dashboard as a modular terminal-style workspace using thin-bordered panels in a compact grid.
- Added a Holdings dashboard panel for quick portfolio review.
- Kept dashboard show/hide, Move Up / Move Down, and Reset Dashboard Layout behavior.
- Added dynamic theme support with Dark Command Center and Light Trading Terminal palettes.
- Added local `app_settings` storage for theme preferences.
- Added Settings page theme selector with local persistence.
- Added About and Privacy / Local Data help dialogs.

### UI Corrections

- Reduced spacing and table padding for a denser desktop-workstation feel.
- Added light gray panel, border, blue accent, and green/red movement colors for the Light Trading Terminal theme.
- Kept dashboard language focused on portfolio review, snapshot history, holdings, recent activity, and import status.
- Avoided brokerage branding, brokerage names, trading execution language, and buy/sell recommendation language.

### Validation

- Built the Debug CMake preset after the shell/theme/dashboard refactor.
- Built the Release CMake preset after the shell/theme/dashboard refactor.
- Smoke-launched the app from a temporary working directory to verify startup without touching the real local database.
- Ran a local settings smoke test confirming theme preference persistence after reopening the SQLite database.
- Confirmed no files are staged.
- Confirmed no untracked, unignored CSV/database/import/export/backup/log/build/IDE private files are visible to Git.

### Known Issues

- Freeform draggable docking is not implemented; dashboard layout customization still uses persisted show/hide and Move Up / Move Down controls.
- CSV export remains a placeholder.

## 2026-05-29 Account-Linked Goal Current Amounts

### Changed Files

- `README.md`
- `docs/Data-Model.md`
- `docs/Roadmap.md`
- `docs/Codex-ChangeLog.md`
- `src/db/Migrations.cpp`
- `src/models/Goal.hpp`
- `src/repositories/GoalRepository.cpp`
- `src/services/PortfolioCalculator.hpp`
- `src/services/PortfolioCalculator.cpp`
- `src/ui/GoalsView.cpp`

### Behavior Added

- Added `use_account_value` and `linked_account_id` fields to goals.
- Kept `current_amount` as the stored manual amount for manual goals.
- Added runtime goal progress calculation from linked account value when `Use account value` is enabled.
- Linked account value uses the existing local account calculation: active holdings market value plus account cash balance.
- Updated the Goals editor with a `Use account value` checkbox, account selector, read-only calculated current amount, and helper text.
- Manual current amount remains editable when `Use account value` is unchecked.
- Missing or invalid linked accounts show a warning and treat effective current amount as `0.00`.

### Validation

- Built the Debug CMake preset.
- Built the Release CMake preset.
- Ran a local goal account-link smoke test against a temporary SQLite database.
- Confirmed manual goal metrics use stored `current_amount`.
- Confirmed linked goal metrics use active holdings market value plus account cash balance.
- Confirmed inactive holdings are excluded from linked account goal value.
- Confirmed missing linked accounts use `0.00` and set the warning flag.
- Confirmed no files are staged.
- Confirmed no untracked, unignored CSV/database/import/export/backup/log/build/IDE private files are visible to Git.

### Known Issues

- Linked goals calculate from the current selected account only; multi-account goal linking is not implemented.
- This is a local tracking convenience only and does not provide financial advice.

## 2026-05-29 Calendar Date Picker Controls

### Changed Files

- `CMakeLists.txt`
- `README.md`
- `docs/Data-Model.md`
- `docs/Roadmap.md`
- `docs/Codex-ChangeLog.md`
- `src/ui/TransactionsView.cpp`
- `src/ui/DividendsView.cpp`
- `src/ui/GoalsView.cpp`
- `src/ui/widgets/DatePicker.hpp`
- `src/ui/widgets/DatePicker.cpp`
- `src/util/Date.hpp`
- `src/util/Date.cpp`

### Behavior Added

- Added a reusable Dear ImGui calendar date picker control.
- Added calendar picker support to transaction dates, dividend received dates, and goal target dates.
- Kept manual date entry available for fast keyboard entry.
- Added inline date validation messaging near date fields.
- Added optional-date handling with a Clear action for goal target dates.
- Added table date rendering that flags invalid stored dates instead of silently presenting them as normal.
- Improved date utilities with strict `YYYY-MM-DD` parsing, date formatting, leap year handling, days-in-month lookup, and day clamping.
- Tightened repository validation through the shared date utility so invalid dates such as `2026-99-99`, `05/28/2026`, and `abc` are rejected.
- Kept SQLite date storage as `TEXT` in `YYYY-MM-DD` format.

### Validation

- Built the Debug CMake preset.
- Built the Release CMake preset.
- Ran a local date utility smoke test for strict parsing, leap years, invalid manual formats, days-in-month behavior, and date clamping.
- Confirmed editable date fields now use the shared picker in Transactions, Dividends, and Goals.
- Confirmed no files are staged.
- Confirmed no untracked, unignored CSV/database/import/export/backup/log/build/IDE private files are visible to Git.

### Known Issues

- The calendar picker is intentionally simple and does not provide locale-specific display formats.
- Portfolio snapshot and import batch dates are created by app workflows today; there is no direct manual editor for those dates yet.

## 2026-05-28 Customizable Dashboard Layout

### Changed Files

- `CMakeLists.txt`
- `README.md`
- `docs/Data-Model.md`
- `docs/Roadmap.md`
- `docs/Codex-ChangeLog.md`
- `src/app/App.hpp`
- `src/app/App.cpp`
- `src/app/AppState.hpp`
- `src/db/Migrations.cpp`
- `src/models/DashboardWidget.hpp`
- `src/repositories/DashboardLayoutRepository.hpp`
- `src/repositories/DashboardLayoutRepository.cpp`
- `src/services/DashboardLayoutService.hpp`
- `src/services/DashboardLayoutService.cpp`
- `src/ui/DashboardView.hpp`
- `src/ui/DashboardView.cpp`

### Behavior Added

- Added a `dashboard_widgets` SQLite table for local dashboard layout preferences.
- Added default dashboard widget seeding when no saved layout exists.
- Added a dashboard layout repository and service to keep ordering and visibility logic out of the view layer.
- Added `Customize Dashboard` mode with visible checkboxes and Move Up / Move Down buttons.
- Added `Reset Dashboard Layout` to restore the default dashboard order and visibility.
- Dashboard sections now render from stable widget keys and saved sort order.
- Hidden dashboard sections stay available in customize mode so they can be re-enabled.
- Unknown widget keys are ignored safely.

### UI Corrections

- Kept the dark dashboard style while adding a compact customization panel.
- Added helper copy explaining that the dashboard can be rearranged to match the user's review workflow.
- Added a local-storage note for dashboard layout preferences.
- Kept dashboard customization separate from financial calculations.

### Validation

- Built the Debug CMake preset.
- Built the Release CMake preset.
- Ran a temp SQLite dashboard layout reopen smoke test outside the repository.
- Confirmed default layout seeding.
- Confirmed Move Up persistence after reopening the database.
- Confirmed hide/show persistence after reopening the database.
- Confirmed Reset Dashboard Layout restores default order and visibility after reopening the database.
- Confirmed no files are staged.
- Confirmed no untracked, unignored CSV/database/export/backup/log/build/IDE private files are visible to Git.

### Known Issues

- Drag-and-drop dashboard reordering is not implemented in this first pass; Move Up / Move Down buttons are used instead.

## 2026-05-28 CSV Import Workflow, Automatic Snapshots, And Holdings Actions

### Changed Files

- `CMakeLists.txt`
- `README.md`
- `docs/CSV-Import.md`
- `docs/Data-Model.md`
- `docs/Roadmap.md`
- `docs/Codex-ChangeLog.md`
- `src/app/App.hpp`
- `src/app/App.cpp`
- `src/app/AppState.hpp`
- `src/db/Migrations.cpp`
- `src/models/Holding.hpp`
- `src/models/ImportBatch.hpp`
- `src/models/PortfolioSnapshot.hpp`
- `src/repositories/HoldingRepository.hpp`
- `src/repositories/HoldingRepository.cpp`
- `src/repositories/ImportBatchRepository.hpp`
- `src/repositories/ImportBatchRepository.cpp`
- `src/repositories/PortfolioSnapshotRepository.hpp`
- `src/repositories/PortfolioSnapshotRepository.cpp`
- `src/services/CsvImportService.hpp`
- `src/services/CsvImportService.cpp`
- `src/services/DashboardService.hpp`
- `src/services/DashboardService.cpp`
- `src/services/HoldingsCsvImport.hpp`
- `src/services/HoldingsCsvImport.cpp`
- `src/services/PortfolioCalculator.cpp`
- `src/services/TransactionService.cpp`
- `src/ui/DashboardView.cpp`
- `src/ui/HoldingsView.cpp`
- `src/ui/ImportCsvView.hpp`
- `src/ui/ImportCsvView.cpp`

### Behavior Added

- Made repeated CSV imports the normal holdings update workflow.
- Added `import_batches` metadata tracking for each CSV import.
- Added holding metadata for `status`, `last_import_batch_id`, and `last_seen_at`.
- Changed CSV import from insert-only to upsert by `account_id + ticker`.
- Existing holdings update on repeated imports instead of duplicating.
- New CSV tickers are inserted as holdings.
- Active holdings missing from the latest account CSV are marked `Inactive`.
- Added CSV import summaries with imported rows, updated holdings, added holdings, skipped rows, missing holdings, warnings, and errors.
- Added automatic CSV-created portfolio snapshots after successful imports.
- CSV import snapshots are replaced for the same account/date/source instead of silently duplicated.
- Dashboard copy now treats CSV-created snapshots as the primary workflow and manual snapshots as optional.
- Dashboard now shows last CSV import date, last snapshot date, and snapshot source.
- Fixed Holdings Edit/Delete modal behavior by opening popups outside the row ID scope.
- Changed holding delete to a soft inactive action so holdings history is retained locally.
- Inactive holdings are excluded from account balance and dashboard portfolio totals.

### Validation

- Built the Debug CMake preset.
- Built the Release CMake preset.
- Ran a temp SQLite smoke test outside the repository with fake Schwab-style CSV files.
- Confirmed first import adds holdings and creates an import batch plus CSV snapshot.
- Confirmed second import updates existing holdings, adds new holdings, marks missing holdings inactive, and creates a new snapshot.
- Confirmed a repeated same-account/same-date CSV import updates the existing CSV snapshot instead of duplicating it.
- Confirmed soft-inactive holdings are excluded from dashboard portfolio totals.
- Smoke-launched the app from a temporary working directory to verify startup migrations without touching the real local database.

### Known Issues

- Missing holdings are marked inactive automatically for this first pass; a richer review/confirm workflow is planned.
- Original CSV files are not stored, only import metadata.
- CSV import remains local-only and does not connect to brokerage accounts.

## 2026-05-28 Dashboard Performance and Transaction Foundation

### Changed Files

- `CMakeLists.txt`
- `README.md`
- `docs/Data-Model.md`
- `docs/Roadmap.md`
- `docs/Codex-ChangeLog.md`
- `src/app/App.hpp`
- `src/app/App.cpp`
- `src/app/AppState.hpp`
- `src/db/Migrations.cpp`
- `src/models/PortfolioSnapshot.hpp`
- `src/models/Transaction.hpp`
- `src/repositories/PortfolioSnapshotRepository.hpp`
- `src/repositories/PortfolioSnapshotRepository.cpp`
- `src/repositories/TransactionRepository.cpp`
- `src/services/DashboardService.hpp`
- `src/services/DashboardService.cpp`
- `src/services/PerformanceCalculator.hpp`
- `src/services/PerformanceCalculator.cpp`
- `src/services/TransactionService.hpp`
- `src/services/TransactionService.cpp`
- `src/ui/DashboardView.hpp`
- `src/ui/DashboardView.cpp`
- `src/ui/TransactionsView.hpp`
- `src/ui/TransactionsView.cpp`

### Behavior Added

- Added transaction fields for fees, sell quantities, sale price, sale proceeds, cost basis used, realized gain/loss dollars, realized gain/loss percent, and adjustment entries.
- Added SQLite migrations for transaction realized gain fields and `portfolio_snapshots`.
- Added a portfolio snapshot repository.
- Added transaction service logic for simple new buy/sell handling using average cost basis.
- Added realized gain/loss calculations for sell transactions.
- Added centralized performance and dashboard calculation services.
- Upgraded Dashboard cards for portfolio value, holdings value, cash, cost basis, unrealized gain/loss, realized gains, snapshot-based daily/monthly/yearly movement, and dividends.
- Added Snapshot Status panel with last snapshot date, snapshot count, and `Create Today's Snapshot`.
- Added Recent Transactions and Realized Gains dashboard panels.
- Added manual snapshot replacement confirmation when today's snapshot already exists.

### Validation

- Built Debug and Release CMake presets.
- Ran a temp SQLite smoke test outside the repository.
- Confirmed migrations create transaction realized-gain fields and `portfolio_snapshots`.
- Confirmed a Buy transaction saves and creates/updates holdings using average cost basis.
- Confirmed a Sell transaction saves, reduces shares, and calculates sale proceeds, cost basis used, realized gain dollars, and realized gain percent.
- Confirmed DashboardService reports realized gains for today, this month, and this year.
- Confirmed a portfolio snapshot can be saved from current dashboard totals.
- Smoke-launched the app from a temporary working directory so the real local database was not touched.

### Known Limitations

- Snapshot-based daily/monthly/yearly movement is simple dollar movement and may include deposits or withdrawals.
- Contribution-adjusted returns are planned for a future update.
- Realized gain/loss uses average cost basis only.
- New buy/sell transactions can update holdings, but full historical reconciliation for edited/deleted transactions is still future work.
- FIFO/LIFO and tax-lot accounting are not implemented.

## 2026-05-28 Release Notes Documentation

### Changed Files

- `README.md`
- `docs/Release-Notes-Guide.md`
- `docs/Codex-ChangeLog.md`

### Behavior Added

- Added a release notes guide for public GitHub releases.
- Added a standard release notes template with project intent, privacy reminder, validation, and known limitations sections.
- Added personal-use messaging that explains the project is public source code shaped by the maintainer's personal investing workflow first.
- Added bug report guidance that asks users not to include personal CSV files, databases, screenshots, brokerage exports, or financial records in GitHub issues.
- Added example release notes for the current milestone.
- Added a README Project Intent section.

### Validation

- Documentation-only change.
- Confirmed no app behavior changes were made.
- Confirmed no real personal investing data was added.

### Known Issues

- Release notes are guide/template documentation only; no GitHub release was created.

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
