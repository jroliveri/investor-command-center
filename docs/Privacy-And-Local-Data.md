# Privacy And Local Data

Investor Command Center is a local-first personal investing tracker. It stores personal investing data on the user's machine and does not require brokerage connections, cloud sync, or external market data APIs.

## Public Source Code

This GitHub repository should contain source code, project documentation, build configuration, and non-private project assets only.

Public repository files include:

- `src/`
- `docs/`
- `README.md`
- `LICENSE`
- `CMakeLists.txt`
- `CMakePresets.json` when present and not user-specific
- `data/.gitkeep`

## Private Local Data

Personal financial records should never be committed to GitHub.

The default local SQLite database path is:

```text
data/investor_command_center.db
```

That database file is intentionally ignored by Git. It may contain account names, balances, holdings, transaction history, dividends, goals, watchlist notes, and other personal investing information.

The repository also ignores imports, exports, backups, logs, spreadsheets, and common brokerage export formats, including CSV, TSV, Excel, OFX, QFX, and QIF files.

## Before Every Commit

Run:

```powershell
git status --short --ignored
```

Confirm that no database files, brokerage exports, backups, logs, spreadsheets, `.env` files, or local user data files are staged.

If a private file was accidentally tracked, remove it from Git tracking without deleting the local file:

```powershell
git rm --cached path\to\private-file
```

## Financial Advice

Investor Command Center records and summarizes user-entered data only. It does not provide financial advice, buy/sell recommendations, portfolio recommendations, or brokerage services.
