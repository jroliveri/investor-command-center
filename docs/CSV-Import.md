# CSV Import

CSV importing is the normal update workflow for Investor Command Center. Import a brokerage positions CSV, preview the rows, confirm the import, and the app updates local holdings for the selected account.

## Privacy Warning

CSV files may contain personal financial data, including account names, holdings, quantities, prices, transaction history, and notes. Do not commit real CSV files to GitHub.

The app reads CSV files from the location the user chooses. It does not copy source CSV files into the repository by default.

Git ignores CSV files and import folders:

- `*.csv`
- `data/imports/`
- `imports/`

## First Supported Import

The first import target is holdings.

Target fields:

- `account_id`
- `ticker`
- `asset_name`
- `asset_type`
- `shares`
- `average_cost`
- `current_price`
- `notes`

The user selects the account before importing. Imported rows are written to the `holdings` table only after preview and validation.

Repeated imports are supported and expected:

- Existing holdings are updated by `account_id + ticker`.
- New tickers are inserted as new holdings.
- Holdings that were active for the account but missing from the latest CSV are marked `Inactive`.
- The app does not blindly duplicate holdings on repeated imports.
- Source CSV files stay local and are not copied into the project folder by default.

## Flexible Column Mapping

The importer auto-maps common brokerage-style column names and lets the user adjust mappings before import. The Column Mapping section is a table with one row per CSV column:

```text
CSV Column Header | CSV Sample Value | Import Field | Parsed/Calculated Preview | Status
```

Each dropdown is tied to the CSV column shown on that row. Columns set to `Ignore Column` are not imported.

Supported variations include:

- Ticker: `Symbol`, `Ticker`
- Asset name: `Name`, `Description`, `Asset Name`
- Shares: `Qty (Quantity)`, `Quantity`, `Shares`
- Average cost: `Average Cost`, `Avg Cost`, `Average Price`, `Cost Basis Per Share`
- Current price: `Last Price`, `Current Price`, `Price`
- Total cost basis: `Cost Basis`, `Total Cost Basis`
- Gain/loss dollar: `Gain $ (Gain/Loss $)`, `Gain/Loss $`, `Gain $`
- Gain/loss percent: `Gain % (Gain/Loss %)`, `Gain/Loss %`, `Gain %`
- Asset type: `Asset Type`, `Type`
- Notes: `Notes`, `Memo`

Average cost and total cost basis are different values. If an average cost column is present, the importer uses it directly. If only total cost basis is present, the importer calculates:

```text
averageCost = totalCostBasis / shares
```

When shares are zero, division is avoided and average cost is set to `0.00` with a warning.

Gain/loss columns are shown for review in the mapping table and import preview, but they are not stored in the `holdings` table. Investor Command Center can recalculate gain/loss from shares, current price, and cost basis.

## Supported Schwab Positions CSV Format

The importer supports Schwab-style positions CSV files that include metadata before the real header row.

Example structure:

- Row 1: account/title metadata such as `Positions for account ...`
- Row 2: blank
- Row 3: holdings header
- Rows 4 and later: holdings and bottom summary rows

Expected Schwab columns include:

- `Symbol`
- `Description`
- `Qty (Quantity)`
- `Price`
- `Cost Basis`
- `Gain $ (Gain/Loss $)`
- `Gain % (Gain/Loss %)`
- `Asset Type`

Importer behavior for this format:

- Detects the holdings header row instead of assuming row 1 is the header.
- Ignores metadata/title rows before the header.
- Ignores blank rows.
- Uses `Symbol` as ticker.
- Uses `Description` as asset name.
- Uses `Qty (Quantity)` as shares.
- Uses `Price` as current price.
- Uses `Cost Basis` as total cost basis and calculates average cost. Schwab positions CSV files usually provide total cost basis, not average cost per share.
- Maps gain/loss columns for preview/reference only.
- Maps `Equity` to `Stock`.
- Maps `ETFs & Closed End Funds` to `ETF`.
- Skips Schwab summary/cash rows such as `Futures Cash`, `Futures Positions Market Value`, `Cash & Cash Investments`, and `Positions Total`.

Cash balance should be managed separately on the account. The holdings importer imports normal holding rows only.

## Import Batches And Snapshots

Every successful CSV import creates an `import_batches` record with import metadata:

- Account
- Import date
- Source type and source name
- Total rows
- Imported rows
- Updated holdings
- Added holdings
- Skipped rows
- Warning count
- Error count
- Missing holdings

Only metadata is stored. The original CSV file is not copied into the repository or stored in SQLite.

After holdings are updated, the app automatically creates or updates a portfolio snapshot for the import date. If a CSV import snapshot already exists for the same account and date, the latest import replaces that snapshot instead of creating a duplicate.

Manual snapshots are optional. CSV import is the primary snapshot workflow.

## Validation Behavior

Rows are previewed before saving. Invalid rows are blocked and show row-level errors.

Validation checks include:

- Account must be selected.
- Ticker is required.
- Shares is required and must be a valid number.
- Current price is required and must be a valid number.
- Average cost must be a valid number when an average cost column is mapped.
- Total cost basis must be a valid number when used to calculate average cost.
- Shares, average cost, and current price cannot be negative.
- Duplicate holdings are blocked.

If asset name is missing, the preview uses the ticker as the asset name and shows a warning. If average cost is missing and cannot be calculated from total cost basis divided by shares, the row can still import with `average_cost = 0.00` and a warning.

Blank numeric values and `--` are treated as missing. Dollar signs, commas, percent signs, and negative values in parentheses are parsed safely. Percent columns are ignored for holdings import.

The Import Preview table shows the final app fields after mapping:

- Row and validation status
- Ticker, asset name, and asset type
- Shares and current price
- Total cost basis and calculated average cost
- Market value and calculated gain/loss
- Notes
- Errors and warnings

## Duplicate Handling

Repeated imports update rows that match an existing holding by:

```text
account_id + ticker
```

The preview still blocks duplicate tickers inside the same CSV import batch. This avoids importing conflicting rows blindly.

Repeated imports do not block rows just because a holding already exists. Existing holdings are updated by account and ticker.

## Future Plans

- Transaction CSV import
- Import templates for common brokerage export formats
- Saved column mapping profiles
- Better duplicate review and merge workflows
- Optional fake sample CSVs under `docs/examples/` only, using obvious fake tickers and fake numbers
