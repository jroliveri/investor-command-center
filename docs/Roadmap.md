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
- Watchlist CRUD
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
- Research menu with first-pass Stock Research using a Yahoo Finance provider abstraction
- Local quote cache for user-requested research lookups
- Portfolio snapshots
- Buy/sell transaction foundation using average cost basis
- Reusable calendar picker controls for editable date fields
- Goals can use manual current amounts or linked calculated account values
- User-defined capital gains allocation helper for Sell transactions

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
- Optional explicit workflow to apply researched prices to local holdings

## Later

- Charts with ImPlot
- Backup and restore workflow
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
