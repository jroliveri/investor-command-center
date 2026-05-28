# Product Brief

## App

Investor Command Center

## Subtitle

Long-term progress beats short-term noise.

## Purpose

Investor Command Center is a private Windows desktop application for manually tracking personal investment accounts, holdings, transactions, dividends, goals, and watchlist ideas. The app establishes a stable local data model, a calm desktop UI, and basic workflows for records that can support future reporting.

## Principles

- Local-first: all user data is stored in a local SQLite database.
- Manual control: no brokerage connections and no automated trading actions.
- Calm by default: the UI should emphasize long-term progress over short-term market noise.
- No advice: the app records and summarizes user-entered data only.

## Current Scope

- App shell and native desktop window
- Sidebar navigation
- Dashboard summary cards
- Account records with create, edit, and delete
- Holding records with create, edit, and delete
- Transaction records with create, edit, delete, and search
- Dividend records with create, edit, delete, search, and totals
- Goal records with create, edit, delete, search, and progress bars
- Watchlist records with create, edit, delete, search, and priority badges
- SQLite setup and migrations
- Basic validation
- Confirmation before deleting records

## Out of Scope

- Stock price APIs
- Brokerage account connections
- Cloud sync
- Login or account management
- Buy/sell recommendations
- Financial advice
