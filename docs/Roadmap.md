# Roadmap

## MVP Complete

- Native C++ desktop app shell
- Dear ImGui UI
- SQLite local database
- Accounts CRUD
- Holdings CRUD
- Transactions CRUD
- Dividends CRUD
- Goals CRUD
- Multiple named Watchlist CRUD with selected-list item management
- Search/filter tables
- Holdings CSV import with preview, mapping, validation, and duplicate blocking
- Repeated holdings CSV import with account+ticker upsert behavior
- Import batch metadata tracking
- Automatic CSV-created portfolio snapshots
- Dashboard summaries with dividends, realized gains, and snapshot-based movement
- Customizable dashboard layout with local reorder, hide/show, and reset controls
- Dashboard chart panels with local data selector, time range, chart type controls, and persisted chart preferences
- Trading-terminal inspired shell with top menu navigation, account information column, compact panels, and light terminal theme
- All main app sections reachable from the top menu bar without side navigation
- Morning snapshot sidebar with account information, account rows, two watchlist slices, and bottom database/version info
- Research menu with first-pass Stock Research using a Yahoo Finance provider abstraction
- Local quote cache for user-requested research lookups
- Polished Stock Research quote panels with live/fallback/cached/error status labels
- Manual dashboard current-price refresh using market data as a display-only overlay
- Portfolio snapshots
- Buy/sell transaction foundation using average cost basis
- Reusable calendar picker controls for editable date fields
- Goals can use manual current amounts or linked calculated account values
- User-defined capital gains allocation helper for Sell transactions
- User-defined Watchlist buy/sell price signals with explicit Yahoo Finance refresh and local signal badges
- Existing watchlist items migrate into a default `Main Watchlist`
- Local SQLite database backups with configurable folder, Back Up Now button, and local reminder settings
- Configurable local SQLite database location with copy/verify/switch-on-restart safety behavior

## Next

- Basic reports using local records
- Dashboard drag-and-drop reordering
- Freeform dockable dashboard panels
- Richer chart history for allocation changes over time
- Settings page display preferences
- Transaction CSV import
- CSV export for manual records
- Allocation summary export/copy refinements
- More complete validation and duplicate detection
- Better missing-holding review before marking inactive
- Import history view
- Full transaction reconciliation for edited/deleted buy and sell activity
- Contribution-adjusted performance calculations
- Historical price charting in Stock Research
- Optional explicit workflow to persist refreshed prices to local holdings after confirmation
- Optional explicit workflow to create snapshots from refreshed dashboard prices
- Optional watchlist signal history and notification-style review panel
- Optional sidebar watchlist assignment controls

## Later

- Charts with ImPlot
- Restore workflow for local database backups
- Optional exportable reports
- FIFO/LIFO and tax-lot cost basis options
- Deeper realized/unrealized gain reports
- Additional market data providers behind the provider abstraction

## Not Planned For This Private MVP

- Brokerage integrations
- Cloud sync
- Login
- Automatic brokerage price sync
- Trading recommendations
- Buy/sell recommendations
- Financial advice
