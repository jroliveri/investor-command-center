# Investor Command Center

Investor Command Center is a personal Windows desktop investing tracker written in C++20. It is designed as a private, local-first cockpit for manually tracking accounts, holdings, and long-term portfolio progress.

Subtitle: Long-term progress beats short-term noise.

This app does not connect to brokerage accounts, does not sync to the cloud, does not call stock price APIs, and does not provide financial advice or buy/sell recommendations.

## Current App

- Native Win32 desktop shell using Dear ImGui with a DirectX 11 renderer
- Sidebar navigation for Dashboard, Accounts, Holdings, Transactions, Dividends, Goals, Watchlist, Reports, and Settings
- Dashboard summary cards based on local account, holding, transaction, dividend, goal, and watchlist records
- Account create, edit, and delete workflow
- Holding create, edit, and delete workflow
- Transaction create, edit, delete, and search workflow
- Dividend create, edit, delete, and search workflow with month, year, and lifetime totals
- Goal create, edit, delete, and search workflow with progress bars
- Watchlist create, edit, delete, and search workflow with priority badges
- Holdings CSV import with local file selection, column mapping, preview, validation, and duplicate blocking
- SQLite database initialization at `data/investor_command_center.db`
- SQLite migrations for `accounts`, `holdings`, `transactions`, `dividends`, `goals`, and `watchlist`
- Basic validation and delete confirmation dialogs
- C++ portfolio calculations for cost basis, market value, and gain/loss

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

The database file is intentionally ignored by Git. Do not commit real account balances, holdings, transactions, dividend records, brokerage exports, backup files, logs, spreadsheets, or any other personal financial data.

This public repository is for source code and documentation only. Review `git status` before every commit and confirm that local databases, exports, backups, and logs are not staged.

See `docs/Privacy-And-Local-Data.md` for the full data-safety guidance.

CSV imports are local-only. Do not store real brokerage CSV files in the repository. See `docs/CSV-Import.md`.

## License

Investor Command Center is released under the MIT License. See `LICENSE`.

## Current Limitations

- Prices are manual fields; there are no market data APIs.
- Reports is a placeholder section.
- CSV export buttons are placeholders.
- CSV import currently targets holdings only; transaction CSV import is planned.
- Account deletion is blocked by SQLite foreign keys when holdings still reference that account.
- Transactions and dividends can optionally reference an account, but do not enforce account foreign keys.
- No authentication, no cloud sync, and no brokerage integration.
- The first configure step downloads third-party source dependencies.
