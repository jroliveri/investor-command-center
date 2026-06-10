# Sidebar Refactor Notes

Inspection date: 2026-06-10

## Current Status

The sidebar/left rail is implemented directly in `src/app/App.cpp`, not as a separate component file. This is an ImGui native UI, so styling is controlled by hard-coded ImGui sizes/flags and `src/ui/UiTheme.*`, not CSS, SCSS, Tailwind, or styled components.

Important: the current working tree contains unresolved conflict/stash markers in sidebar-adjacent files. Treat the notes below as an audit of the visible implementation, but resolve the merge state before making the actual refactor.

## Relevant Files

- `src/app/App.cpp`
  - Sidebar container and current sidebar cards/widgets.
  - `AccountColumnWidth = 340.0f`.
  - `renderAccountColumn()`, `renderPortfolioSummaryCard()`, `renderWatchlistPanel()`.
  - Data refresh state mutation in `refreshDashboardPrices()` and `refreshWatchlistPrices()`.
- `src/app/SidebarModel.*`
  - Pure presentation model for sidebar card order, card labels, row caps, formatted metric text, watchlist tabs, and data-status summaries.
  - Covered by `tests/SidebarModelTests.cpp`.
- `src/app/App.hpp`
  - Declares the sidebar render methods.
  - Owns repositories/services/views used by sidebar rendering through `App`.
- `src/app/AppState.hpp`
  - Holds sidebar data vectors and refresh status structs.
  - Currently has conflict markers around `SignalRules` / backup settings.
- `src/services/DashboardService.*`
  - Builds `DashboardData` used by the portfolio snapshot card.
  - Applies session-only dashboard price overrides to holdings.
- `src/services/PortfolioCalculator.*`
  - Calculates portfolio totals and per-account balances.
- `src/services/PerformanceCalculator.*`
  - Calculates realized gains and daily/monthly/yearly snapshot movement.
- `src/repositories/AccountRepository.*`, `src/repositories/HoldingRepository.*`, `src/repositories/TransactionRepository.*`, `src/repositories/PortfolioSnapshotRepository.*`, `src/repositories/ImportBatchRepository.*`
  - Load the source records into `AppState` through `App::reloadData()`.
- `src/repositories/WatchlistRepository.*`
  - Current checked-in API is single-watchlist item CRUD only.
  - Some conflicted UI/test code expects multi-watchlist APIs that are not present in the visible repository header.
- `src/ui/WatchlistView.*`
  - Full watchlist page.
  - Contains partial/conflicted multi-watchlist manager and sidebar-slot assignment work.
- `src/ui/DashboardView.cpp`
  - Existing full-page `Current Price Refresh` and `Import Status / Snapshot History` panels.
- `src/ui/UiTheme.*`
  - Global ImGui geometry, palette, child/table colors, rounding, spacing, and scrollbar size.
- `README.md`
  - Already states that navigation is handled by the top menu and the left rail is for portfolio context, account summaries, and quick watchlist symbols.

## Current Component Hierarchy

`App::render()` in `src/app/App.cpp`

- Root window: `ImGui::Begin("Investor Command Center", ..., windowFlags)`
- Left rail: `renderAccountColumn()`
  - `ImGui::BeginChild("AccountColumn", ImVec2(AccountColumnWidth, 0.0f), true)`
  - `renderPortfolioSummaryCard()`
    - Child id: `PortfolioSummaryCard`
    - Visible heading: `Portfolio Summary`
    - Current role: combined portfolio metrics plus compact account preview.
    - Uses shared sidebar card flags: no visible scrollbar and no card-level mouse-wheel scrolling.
  - `renderWatchlistPanel()`
    - Child id: `SidebarWatchlistsCard`
    - Row table id: `SidebarWatchlistsRows`
    - Visible heading: `Watchlists`
    - Uses a forward-compatible `SidebarModel::WatchlistGroup` array with a default `Main` group sourced from `state_.watchlist`.
    - Shows up to 5 symbols, then a compact `+N more` line.
  - `renderDataStatusCard()`
    - Child id: `SidebarDataStatusCard`
    - Visible heading: `Data Status`
    - Normal state folds SQLite into `Connected`, shows market provider, compact last refresh, backup reminder, and app version.
    - Uses tighter padding and remains visually secondary.
- Main content:
  - `ImGui::BeginChild("Content", ImVec2(0.0f, 0.0f), false)`
  - `ImGui::BeginChild("PageContent", ImVec2(0.0f, -statusHeight), false)`
  - `renderCurrentSection()`
  - `renderStatusBar()`

## Located Widgets/Cards

- Sidebar container
  - `src/app/App.cpp`, `renderAccountColumn()`.
  - Child id: `AccountColumn`.
  - Width: `AccountColumnWidth = 340.0f`.

- Navigation widget/card
  - No separate sidebar navigation card exists in the visible current implementation.
  - Top navigation is `renderTopMenuBar()`.
  - Sidebar no longer has the old app-title/current-page/top-menu instruction block.
  - Historical note: `docs/Codex-ChangeLog.md` says old side page navigation was removed on 2026-05-29.

- Portfolio Summary widget/card
  - Implemented as `renderPortfolioSummaryCard()`.
  - Child id: `PortfolioSummaryCard`.
  - Combines the former portfolio metrics and accounts sidebar cards.
  - Uses `SidebarModel::buildPortfolioSummaryCard()` for labels, formatted values, row caps, and active-status hiding.

- Watchlist widget/card
  - `renderWatchlistPanel()`.
  - Current child id: `SidebarWatchlistsCard`.
  - Current row table id: `SidebarWatchlistsRows`.
  - Renders compact tabs from `SidebarModel::WatchlistGroup`; currently the visible repository supplies a default `Main` group from the single-list `state_.watchlist`.
  - Partial/conflicted multi-watchlist manager code exists in `WatchlistView.cpp`, but it does not line up with the current model/repository files.

- Data Status widget/card
  - `renderDataStatusCard()`.
  - Current child id: `SidebarDataStatusCard`.
  - Normal/healthy state is compact and visually secondary.
  - Refresh/database warnings expand the card with an issue summary.
  - Richer refresh/data-provider status is currently on dashboard/watchlist pages:
    - Dashboard: `DashboardView.cpp::drawPriceRefreshStatus()` reads `state.dashboardPriceRefreshStatus`.
    - Watchlist page: `WatchlistView.cpp` reads `state.watchlistPriceRefreshStatus`.

## Styling And Layout Controls

- Sidebar width
  - `src/app/App.cpp`: `constexpr float AccountColumnWidth = 340.0f`.
  - Applied by `ImGui::BeginChild("AccountColumn", ImVec2(AccountColumnWidth, 0.0f), true)`.

- Card spacing
  - `renderAccountColumn()` renders each sidebar card sequentially.
  - Global spacing comes from `UiTheme::applyCompactGeometry()`:
    - `WindowPadding = ImVec2(10.0f, 9.0f)`
    - `FramePadding = ImVec2(8.0f, 5.0f)`
    - `CellPadding = ImVec2(7.0f, 4.0f)`
    - `ItemSpacing = ImVec2(8.0f, 6.0f)`
    - `ItemInnerSpacing = ImVec2(6.0f, 4.0f)`

- Card heights
  - `PortfolioSummaryCard`: dynamic height based on capped visible account rows and current ImGui row height.
  - `SidebarWatchlistsCard`: dynamic height based on capped visible symbol rows and current ImGui row height.
  - `SidebarDataStatusCard`: fixed compact height in normal state; expands only for database/refresh warnings.
  - Sidebar itself uses height `0.0f`, filling available height.

- Scroll behavior
  - `AccountColumn` is an ImGui child with border and no explicit `NoScrollbar`, so it can scroll when contents exceed viewport height.
  - Sidebar cards use `SidebarCardFlags` (`ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse`) and cap/truncate dynamic content.
  - Current sidebar tables do not use `ImGuiTableFlags_ScrollY`; overflow is primarily from child windows.
  - `UiTheme::applyCompactGeometry()` sets `ScrollbarSize = 13.0f`.
  - `UiTheme::metricCard()` uses `ImGuiWindowFlags_NoScrollbar`, but the sidebar cards do not use this helper.

- Inner widget overflow
  - Portfolio summary, watchlist, and data status sidebar cards use fixed-height or computed-height `BeginChild` containers without their own scrolling.
  - Account names, ticker symbols, price text, and refresh warning text are truncated for the 340px sidebar.
  - `compactMetric()` aligns values with `ImGui::SameLine(170.0f)`, which may clip long currency text inside a 340px sidebar.

## Data Sources By Current Card

### Portfolio Summary

Rendering path: `App::renderPortfolioSummaryCard()`.

- Presentation path: `SidebarModel::buildPortfolioSummaryCard()`.
- Builds `DashboardData` with `DashboardService::build(state_, today, Date::currentMonthPrefix(), Date::currentYearPrefix())`.
- Portfolio total value:
  - `data.portfolio.accountBalance`
  - Calculated by `PortfolioCalculator::calculateSummary(state.accounts, DashboardService::holdingsWithDashboardPrices(state))`.
- Holdings value:
  - `data.portfolio.holdingsMarketValue`.
- Cash balance:
  - `data.portfolio.cashBalance`.
  - Sums active account `cashBalance` values in `PortfolioCalculator::calculateSummary()`.
- Unrealized gain/loss:
  - `data.portfolio.gainLossDollar`.
  - Derived from active holdings market value minus cost basis.
- Realized gain/loss:
  - Current sidebar shows only YTD: `data.realizedGains.thisYear`.
  - Computed by `PerformanceCalculator::calculateRealizedGains(state.transactions, today, monthPrefix, yearPrefix)`.
- Daily movement:
  - `data.performance.daily` with `data.performance.hasDaily`; renders `N/A` when unavailable.
  - Computed by `PerformanceCalculator::calculateSnapshotPerformance(state.portfolioSnapshots, data.portfolio.accountBalance, ...)`.
- Account names:
  - `account.accountName` from `state_.accounts`.
  - Loaded by `AccountRepository::listAll()`.
- Account balances:
  - `PortfolioCalculator::calculateAccount(account, holdings).calculatedBalance`.
  - `holdings` comes from `DashboardService::holdingsWithDashboardPrices(state_)`.
  - Calculated balance = active holdings market value for that account plus `account.cashBalance`.
- Account cash balances:
  - Account-level cash is used in `PortfolioCalculator::calculateAccount()` and total cash is shown in the metric rows.
- Account statuses:
  - `account.status`.
  - Current sidebar only displays status inline when it is non-empty and not `Active`.
- Holdings source:
  - `state_.holdings`, loaded by `HoldingRepository::listAll()`, then optionally overlaid with session-only dashboard prices.

### Watchlist

Rendering path: `App::renderWatchlistPanel()`.

- Current single-list implementation:
  - Reads all rows from `state_.watchlist` through a default `SidebarModel::WatchlistGroup` named `Main`.
  - Loaded by `WatchlistRepository::listAll()`.
  - Displays up to 6 items.
  - Columns: ticker, current/cached price, cached daily change/percent.
  - Current price: `item.currentPrice`.
  - Cached quote data: `MarketDataService::cachedQuote(item.ticker, error)` for current price override and price change/percent when available.
  - Alert/target indicator: ticker text is colored by `WatchlistSignalService::calculateSignalStatus(...)`.
  - `selectedSidebarWatchlistTab_` in `App` preserves the selected sidebar tab across render frames.
  - Presentation behavior is covered by `SidebarModel::buildWatchlistsCard()`.

- Conflicted slot-based implementation:
  - Intended to use `state.watchlists`, `Watchlist::showInSidebar`, `Watchlist::sidebarSlot`, and `WatchlistItem::watchlistId`.
  - Intended to sort by calculated signal and priority using `TechnicalIndicatorService` and `state.signalRules`.
  - Not currently coherent with the visible `WatchlistItem` and `WatchlistRepository` definitions.

- Watchlist refresh status:
  - `state.watchlistPriceRefreshStatus`.
  - Struct fields: `hasRun`, `provider`, `lastRefreshedAt`, `refreshedSymbols`, `failedSymbols`, `cachedSymbols`, `warning`.
  - Updated by `App::refreshWatchlistPrices()` and `WatchlistView::refreshPrices()`.

### Data Status

Current sidebar path: `App::renderDataStatusCard()`.

- Presentation path: `SidebarModel::buildDataStatusCard()`.
- Database provider/path:
  - Hard-coded `DatabasePath = "data/investor_command_center.db"`.
  - Normal state is folded into `Connected`; the path is available as a hover tooltip.
- Market data provider:
  - Available through `MarketDataService::providerName()`.
  - Dashboard refresh status stores `state.dashboardPriceRefreshStatus.provider`.
  - Watchlist refresh status stores `state.watchlistPriceRefreshStatus.provider`.
  - Current sidebar chooses the latest dashboard/watchlist refresh provider when available, otherwise falls back to `MarketDataService::providerName()`.
- Last refresh:
  - Uses the latest dashboard or watchlist refresh status timestamp.
  - Compact formatting shows current-day times as `HH:MM` and older refreshes as `MM-DD HH:MM`.
- Backup reminder:
  - Current visible app model has no persisted backup date; sidebar preserves the existing monthly manual backup reminder as `Backup monthly`.
- App version:
  - Uses `AppVersion`.
- Dashboard refresh status:
  - Updated by `App::refreshDashboardPrices()`.
  - Reads active holdings/account status, calls `marketDataService_->fetchQuote(symbol)`, stores session-only `state_.dashboardPriceOverrides`, then updates `state_.dashboardPriceRefreshStatus`.
- Watchlist refresh status:
  - Updated by `WatchlistSignalService::refreshPrices(...)`.
  - Stores status in `state_.watchlistPriceRefreshStatus`.

## Recommended Implementation Order

1. Resolve the merge/conflict markers before touching behavior.
   - Files with visible markers include `CMakeLists.txt`, `src/app/App.cpp`, `src/app/AppState.hpp`, `src/ui/WatchlistView.*`, `src/services/WatchlistSignalService.*`, and `src/ui/SettingsView.*`.
   - Decide whether the multi-watchlist model/repository work is in scope for the sidebar refactor or should land first as a separate data-model change.

2. Extract the sidebar from `App.cpp` into a small dedicated module after the merge is clean.
   - Suggested files: `src/app/Sidebar.hpp` and `src/app/Sidebar.cpp`, or `src/ui/SidebarView.*`.
   - Keep initial extraction behavior-preserving.

3. Remove sidebar navigation copy/card behavior only after extraction.
   - Current implementation has no actual nav controls, only current-page/top-menu instructional text.
   - Keep top menu navigation untouched.

4. Refine the combined Portfolio Summary widget as needed.
   - It now reuses `DashboardService::build()` and `PortfolioCalculator::calculateAccount()`.
   - Keep avoiding duplicate summary math.
   - Daily movement uses `data.performance.hasDaily`; otherwise it shows a compact unavailable state.

5. Graduate the sidebar tabs to real multi-watchlist data when the model is ready.
   - The sidebar now renders through a `SidebarModel::WatchlistGroup` array and keeps a selected tab in `selectedSidebarWatchlistTab_`.
   - The current visible repository still supplies only a default `Main` group from `state_.watchlist`.
   - If multiple watchlists are desired, add/finish `Watchlist` model, `AppState::watchlists`, `WatchlistItem::watchlistId`, migrations, and repository APIs first.

6. Extend Data Status only when the backup model is resolved.
   - Current sidebar combines local database status, market provider, dashboard/watchlist refresh status, monthly backup reminder, and app version.
   - If the conflicted backup settings model lands, replace `Backup monthly` with the real next-due/last-backup value.

7. Reduce nested scrollbars.
   - Current sidebar uses a single scrollable `AccountColumn`; the individual cards are non-scrolling under normal content.
   - Continue preferring row caps, truncation, and `View all` links over nested card scrollbars.

## Risks / Unclear Areas

- The codebase is not currently buildable because of unresolved conflict/stash markers.
- The visible watchlist repository/model are single-list, but conflicted UI/tests expect named watchlists and sidebar slots.
- Sidebar watchlist tabs are forward-compatible UI only until the multi-watchlist repository/model lands.
- `CMakeLists.txt` references files that are not visible in `src/models` / `src/repositories`, such as `Watchlist.hpp`, `MarketPriceHistory.hpp`, and technical indicator repositories.
- Some conflicted code references theme helpers/colors not declared in the visible `UiTheme.hpp` (`pushPanelStyle`, `pushTableStyle`, `pushButtonStyle`, `NeonMagenta`, etc.).
- The current sidebar hard-codes card heights; simply combining cards may make nested scrollbars worse unless the layout model is adjusted.
- Account status is available but not visibly displayed in the sidebar Accounts card today.
- Daily movement is available through `DashboardData.performance` but not currently rendered in the sidebar.
- The visible app model has no persisted backup due date, so the sidebar cannot show a real `Backup Jun 15` style value yet.
