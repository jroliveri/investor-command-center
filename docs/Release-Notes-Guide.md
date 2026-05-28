# Release Notes Guide

Investor Command Center is a personal-use C++ desktop investing tracker. The source is public and open source, but the app is primarily built around Joseph R. Oliveri's own investing workflow.

Bug reports are welcome. Feature requests may be considered, but project direction follows the maintainer's personal workflow first.

This app does not provide financial advice, does not connect to brokerage accounts, and stores data locally.

## Release Notes Style Guide

Release notes should be clear, practical, and grounded in what changed for the desktop app.

Use this tone:

- Personal and direct
- Calm, plain-language, and non-commercial
- Honest about limitations
- Friendly to contributors and bug reporters
- Careful with privacy and local data

Avoid:

- Financial advice language
- Buy, sell, hold, or recommendation wording
- Claims that the app is production-grade financial software
- Language implying brokerage integration, cloud sync, or automated account access
- Real account names, balances, holdings, CSV rows, database contents, or screenshots containing private records

It is fine for release notes to acknowledge the spirit of the project:

> I am building this app to unwind and relax in the evening - AI Zombie time and flow coding. Everything is made up, and the points don't matter - except they kind of do, because I am making this to track my investments my way.

## Standard Release Notes Template

```markdown
# Investor Command Center vX.Y.Z

Personal desktop investing tracker for local-first manual portfolio tracking.

## Project Intent

Investor Command Center is built for personal use and follows the maintainer's workflow first. Bug reports are welcome. Feature requests may be considered, but this is not a commercial finance product.

This app does not provide financial advice. Users are responsible for their own financial decisions.

## Highlights

- Added ...
- Improved ...
- Fixed ...

## Details

- ...
- ...

## Privacy Reminder

Investor Command Center stores personal investing data locally. Do not include real CSV files, SQLite database files, brokerage exports, account balances, holdings, screenshots with financial records, backups, logs, or spreadsheets in GitHub issues or pull requests.

## Validation

- Built with ...
- Tested ...

## Known Limitations

- No brokerage account connections
- No cloud sync
- No financial advice
- Prices and records are manually managed unless otherwise noted
```

## Personal-Use Disclaimer

Use this standard disclaimer in release notes when appropriate:

Investor Command Center is a personal desktop investing tracker built for local-first manual record keeping. It is public source code, but it is primarily shaped by the maintainer's own workflow. It is not a commercial finance product and does not provide financial advice.

Users are responsible for their own financial decisions.

## Privacy Reminder

Never include private financial data in release notes, issues, pull requests, screenshots, or sample files.

Do not publish:

- Real brokerage CSV files
- SQLite database files
- Account balances
- Holdings
- Transaction history
- Dividend records
- Watchlists
- Backup files
- Export files
- Logs
- Spreadsheets
- Screenshots containing personal financial records

Use fake examples only. Fake examples should be obvious and should not resemble real personal accounts.

## Bug Report Reminder

Bug reports are welcome. Good bug reports should include:

- App version or commit SHA
- Windows version
- Visual Studio or CMake version, if build-related
- Steps to reproduce
- Expected behavior
- Actual behavior
- A screenshot only if it contains no personal financial data
- A fake/minimized CSV sample only if needed to reproduce an import bug

Do not attach personal CSV files, SQLite databases, brokerage exports, logs with private paths, screenshots with account values, or real financial records.

## Example Release Notes For Current Milestone

# Investor Command Center Current Milestone

Personal desktop investing tracker for local-first manual portfolio tracking.

## Project Intent

Investor Command Center is built for personal use and follows the maintainer's workflow first. It is public and open source so the code can be shared, inspected, and improved, but it is not intended to be a commercial finance product.

Bug reports are welcome. Feature requests may be considered, but the project direction follows the maintainer's personal investing workflow first.

This app does not provide financial advice. Users are responsible for their own financial decisions.

## Highlights

- Added broader local tracking sections for transactions, dividends, goals, and watchlist items.
- Added calculated account balances based on holdings market value plus cash balance.
- Added a local holdings CSV import workflow with preview, mapping, validation, and duplicate blocking.
- Improved Schwab-style positions CSV support, including header detection, summary row skipping, total cost basis handling, and clearer mapping UI.
- Added Visual Studio 2026 CMake presets and Windows release workflow support.
- Added privacy and local-data protections for public repository use.

## Privacy Reminder

Investor Command Center stores personal investing data locally. Do not include real CSV files, SQLite database files, brokerage exports, account balances, holdings, screenshots with financial records, backups, logs, or spreadsheets in GitHub issues or pull requests.

## Validation

- Built Debug and Release CMake presets.
- Tested fake Schwab-style positions CSV import outside the repository.
- Confirmed local database and generated files remain ignored by Git.

## Known Limitations

- No brokerage integration.
- No cloud sync.
- No stock price APIs.
- No financial advice.
- Holdings CSV import is supported first; transaction CSV import is future work.
