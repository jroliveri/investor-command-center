// SPDX-License-Identifier: MIT
#include "services/HoldingsCsvImport.hpp"

#include <algorithm>
#include <cctype>
#include <charconv>
#include <fstream>
#include <optional>
#include <set>
#include <sstream>

namespace {

std::string trim(std::string value)
{
    const auto first = std::find_if_not(value.begin(), value.end(), [](unsigned char c) {
        return std::isspace(c) != 0;
    });
    const auto last = std::find_if_not(value.rbegin(), value.rend(), [](unsigned char c) {
        return std::isspace(c) != 0;
    }).base();

    if (first >= last) {
        return {};
    }

    return std::string(first, last);
}

std::string lowerCopy(std::string value)
{
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return value;
}

std::string normalizeHeader(std::string value)
{
    value = lowerCopy(trim(std::move(value)));
    std::string expanded;
    expanded.reserve(value.size());
    for (const char c : value) {
        if (c == '$') {
            expanded += "dollar";
        } else if (c == '%') {
            expanded += "percent";
        } else {
            expanded.push_back(c);
        }
    }
    value = std::move(expanded);
    value.erase(std::remove_if(value.begin(), value.end(), [](unsigned char c) {
        return std::isspace(c) != 0 || c == '_' || c == '-' || c == '/' || c == '(' || c == ')';
    }), value.end());
    return value;
}

std::string normalizeTicker(std::string value)
{
    value = trim(std::move(value));
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) {
        return static_cast<char>(std::toupper(c));
    });
    return value;
}

std::string cell(const std::vector<std::string>& row, int index)
{
    if (index < 0 || static_cast<std::size_t>(index) >= row.size()) {
        return {};
    }

    return trim(row[static_cast<std::size_t>(index)]);
}

bool parseDouble(std::string value, double& result)
{
    value = trim(std::move(value));
    if (value.empty() || value == "--") {
        result = 0.0;
        return true;
    }

    bool negativeFromParentheses = false;
    if (value.size() >= 2 && value.front() == '(' && value.back() == ')') {
        negativeFromParentheses = true;
        value = value.substr(1, value.size() - 2);
    }

    value.erase(std::remove(value.begin(), value.end(), '$'), value.end());
    value.erase(std::remove(value.begin(), value.end(), ','), value.end());
    value.erase(std::remove(value.begin(), value.end(), '%'), value.end());
    value = trim(std::move(value));

    if (value.empty()) {
        result = 0.0;
        return true;
    }

    const char* begin = value.data();
    const char* end = begin + value.size();
    const auto [ptr, error] = std::from_chars(begin, end, result);
    if (error != std::errc {} || ptr != end) {
        return false;
    }

    if (negativeFromParentheses) {
        result = -result;
    }

    return true;
}

bool isMissingNumber(const std::string& value)
{
    const std::string trimmed = trim(value);
    return trimmed.empty() || trimmed == "--";
}

bool parseRequiredDouble(const std::vector<std::string>& row, int index, const char* field, double& result, std::vector<std::string>& errors)
{
    if (index < 0) {
        errors.push_back(std::string(field) + " column is not mapped.");
        result = 0.0;
        return false;
    }

    const std::string value = cell(row, index);
    if (isMissingNumber(value)) {
        errors.push_back(std::string(field) + " is required.");
        result = 0.0;
        return false;
    }

    if (!parseDouble(value, result)) {
        errors.push_back(std::string(field) + " is not a valid number.");
        result = 0.0;
        return false;
    }

    return true;
}

std::optional<double> parseOptionalDouble(const std::vector<std::string>& row, int index, const char* field, std::vector<std::string>& errors)
{
    if (index < 0) {
        return std::nullopt;
    }

    const std::string value = cell(row, index);
    if (isMissingNumber(value)) {
        return std::nullopt;
    }

    double parsed = 0.0;
    if (!parseDouble(value, parsed)) {
        errors.push_back(std::string(field) + " is not a valid number.");
        return std::nullopt;
    }

    return parsed;
}

std::vector<std::string> parseCsvLine(const std::string& line)
{
    std::vector<std::string> cells;
    std::string current;
    bool inQuotes = false;

    for (std::size_t index = 0; index < line.size(); ++index) {
        const char c = line[index];

        if (c == '"') {
            if (inQuotes && index + 1 < line.size() && line[index + 1] == '"') {
                current.push_back('"');
                ++index;
            } else {
                inQuotes = !inQuotes;
            }
        } else if (c == ',' && !inQuotes) {
            cells.push_back(trim(current));
            current.clear();
        } else {
            current.push_back(c);
        }
    }

    cells.push_back(trim(current));
    return cells;
}

int findHeader(const std::vector<std::string>& headers, std::initializer_list<const char*> names)
{
    std::set<std::string> targets;
    for (const char* name : names) {
        targets.insert(normalizeHeader(name));
    }

    for (std::size_t index = 0; index < headers.size(); ++index) {
        if (targets.contains(normalizeHeader(headers[index]))) {
            return static_cast<int>(index);
        }
    }

    return -1;
}

bool hasHeader(const std::vector<std::string>& headers, std::initializer_list<const char*> names)
{
    return findHeader(headers, names) >= 0;
}

bool isHeaderRow(const std::vector<std::string>& row)
{
    return hasHeader(row, { "Symbol", "Ticker" }) &&
        hasHeader(row, { "Qty (Quantity)", "Quantity", "Shares" }) &&
        hasHeader(row, { "Price", "Current Price", "Last Price" });
}

bool rowIsBlank(const std::vector<std::string>& row)
{
    return std::all_of(row.begin(), row.end(), [](const std::string& value) {
        return trim(value).empty();
    });
}

bool isSummaryRow(const std::vector<std::string>& row)
{
    static const std::set<std::string> summaryLabels {
        "futures cash",
        "futures positions market value",
        "cash & cash investments",
        "cash and cash investments",
        "positions total",
    };

    for (const std::string& value : row) {
        if (summaryLabels.contains(lowerCopy(trim(value)))) {
            return true;
        }
    }

    return false;
}

std::string duplicateKey(int accountId, const std::string& ticker, const std::string& assetName)
{
    return std::to_string(accountId) + "|" + lowerCopy(ticker) + "|" + lowerCopy(trim(assetName));
}

std::string normalizeAssetType(std::string value)
{
    value = trim(std::move(value));
    const std::string normalized = lowerCopy(value);

    if (normalized == "equity") {
        return "Stock";
    }

    if (normalized == "etfs & closed end funds" || normalized == "etfs and closed end funds" || normalized == "etf") {
        return "ETF";
    }

    if (normalized == "cash and money market") {
        return "Cash";
    }

    if (normalized == "schwab futures") {
        return "Other";
    }

    return value;
}

void validateMoney(double value, const char* field, std::vector<std::string>& errors)
{
    if (value < 0.0) {
        errors.push_back(std::string(field) + " cannot be negative.");
    }
}

}

namespace HoldingsCsvImport {

bool loadCsvFile(const std::string& path, CsvTable& table, std::string& error, int requestedHeaderRowNumber)
{
    table = {};
    error.clear();

    std::ifstream file(path);
    if (!file.is_open()) {
        error = "Could not open CSV file.";
        return false;
    }

    bool foundHeader = false;
    std::string line;
    int lineNumber = 0;
    while (std::getline(file, line)) {
        ++lineNumber;
        const std::vector<std::string> parsed = parseCsvLine(line);
        const bool blank = trim(line).empty() || rowIsBlank(parsed);

        if (requestedHeaderRowNumber > 0 && !foundHeader) {
            if (lineNumber < requestedHeaderRowNumber) {
                if (blank) {
                    ++table.skippedBlankRowCount;
                } else {
                    ++table.skippedMetadataRowCount;
                }
                continue;
            }

            if (lineNumber == requestedHeaderRowNumber) {
                if (blank) {
                    error = "Selected header row is blank.";
                    return false;
                }

                table.headers = parsed;
                table.headerRowNumber = lineNumber;
                foundHeader = true;
                continue;
            }
        }

        if (blank) {
            ++table.skippedBlankRowCount;
            continue;
        }

        if (!foundHeader) {
            if (isHeaderRow(parsed)) {
                table.headers = parsed;
                table.headerRowNumber = lineNumber;
                foundHeader = true;
            } else {
                ++table.skippedMetadataRowCount;
            }
            continue;
        }

        if (isSummaryRow(parsed)) {
            ++table.skippedSummaryRowCount;
            continue;
        }

        table.rows.push_back(CsvDataRow { lineNumber, parsed });
    }

    if (!foundHeader) {
        error = requestedHeaderRowNumber > 0
            ? "Could not use the selected header row."
            : "Could not detect a holdings header row. Expected columns such as Symbol, Description, Quantity, and Price.";
        return false;
    }

    return true;
}

HoldingsCsvColumnMapping autoMapColumns(const std::vector<std::string>& headers)
{
    HoldingsCsvColumnMapping mapping;
    mapping.fields.assign(headers.size(), HoldingsCsvImportField::Ignore);

    const auto assign = [&mapping, &headers](HoldingsCsvImportField field, std::initializer_list<const char*> names) {
        const int column = findHeader(headers, names);
        if (column >= 0 && static_cast<std::size_t>(column) < mapping.fields.size()) {
            mapping.fields[static_cast<std::size_t>(column)] = field;
        }
    };

    assign(HoldingsCsvImportField::Ticker, { "Symbol", "Ticker" });
    assign(HoldingsCsvImportField::AssetName, { "Name", "Description", "Asset Name" });
    assign(HoldingsCsvImportField::Shares, { "Qty (Quantity)", "Quantity", "Shares" });
    assign(HoldingsCsvImportField::CurrentPrice, { "Last Price", "Current Price", "Price" });
    assign(HoldingsCsvImportField::AverageCost, { "Average Cost", "Avg Cost", "Average Price", "Cost Basis Per Share" });
    assign(HoldingsCsvImportField::TotalCostBasis, { "Cost Basis", "Total Cost Basis" });
    assign(HoldingsCsvImportField::GainLossDollar, { "Gain $ (Gain/Loss $)", "Gain/Loss $", "Gain $", "Gain Dollar", "Gain Loss Dollar" });
    assign(HoldingsCsvImportField::GainLossPercent, { "Gain % (Gain/Loss %)", "Gain/Loss %", "Gain %", "Gain Percent", "Gain Loss Percent" });
    assign(HoldingsCsvImportField::AssetType, { "Asset Type", "Type" });
    assign(HoldingsCsvImportField::Notes, { "Notes", "Memo" });

    return mapping;
}

const char* importFieldLabel(HoldingsCsvImportField field)
{
    switch (field) {
    case HoldingsCsvImportField::Ignore:
        return "Ignore Column";
    case HoldingsCsvImportField::Ticker:
        return "Ticker";
    case HoldingsCsvImportField::AssetName:
        return "Asset Name";
    case HoldingsCsvImportField::Shares:
        return "Shares";
    case HoldingsCsvImportField::CurrentPrice:
        return "Current Price";
    case HoldingsCsvImportField::AverageCost:
        return "Average Cost";
    case HoldingsCsvImportField::TotalCostBasis:
        return "Total Cost Basis";
    case HoldingsCsvImportField::GainLossDollar:
        return "Gain/Loss Dollar";
    case HoldingsCsvImportField::GainLossPercent:
        return "Gain/Loss Percent";
    case HoldingsCsvImportField::AssetType:
        return "Asset Type";
    case HoldingsCsvImportField::Notes:
        return "Notes";
    }

    return "Ignore Column";
}

int findMappedColumn(const HoldingsCsvColumnMapping& mapping, HoldingsCsvImportField field)
{
    for (std::size_t index = 0; index < mapping.fields.size(); ++index) {
        if (mapping.fields[index] == field) {
            return static_cast<int>(index);
        }
    }

    return -1;
}

std::vector<HoldingsCsvPreviewRow> buildPreview(
    const CsvTable& table,
    const HoldingsCsvColumnMapping& mapping,
    int accountId,
    const std::vector<Holding>& existingHoldings)
{
    std::vector<HoldingsCsvPreviewRow> preview;
    std::set<std::string> existingKeys;
    std::set<std::string> seenImportKeys;

    const int tickerColumn = findMappedColumn(mapping, HoldingsCsvImportField::Ticker);
    const int assetNameColumn = findMappedColumn(mapping, HoldingsCsvImportField::AssetName);
    const int assetTypeColumn = findMappedColumn(mapping, HoldingsCsvImportField::AssetType);
    const int sharesColumn = findMappedColumn(mapping, HoldingsCsvImportField::Shares);
    const int currentPriceColumn = findMappedColumn(mapping, HoldingsCsvImportField::CurrentPrice);
    const int averageCostColumn = findMappedColumn(mapping, HoldingsCsvImportField::AverageCost);
    const int totalCostBasisColumn = findMappedColumn(mapping, HoldingsCsvImportField::TotalCostBasis);
    const int gainLossDollarColumn = findMappedColumn(mapping, HoldingsCsvImportField::GainLossDollar);
    const int gainLossPercentColumn = findMappedColumn(mapping, HoldingsCsvImportField::GainLossPercent);
    const int notesColumn = findMappedColumn(mapping, HoldingsCsvImportField::Notes);

    for (const Holding& holding : existingHoldings) {
        existingKeys.insert(duplicateKey(holding.accountId, holding.ticker, holding.assetName));
    }

    for (const CsvDataRow& csvRow : table.rows) {
        const std::vector<std::string>& row = csvRow.cells;
        HoldingsCsvPreviewRow previewRow;
        previewRow.sourceRowNumber = csvRow.sourceRowNumber;
        previewRow.holding.accountId = accountId;
        previewRow.holding.ticker = normalizeTicker(cell(row, tickerColumn));
        previewRow.holding.assetName = cell(row, assetNameColumn);
        previewRow.holding.assetType = normalizeAssetType(cell(row, assetTypeColumn));
        previewRow.holding.notes = cell(row, notesColumn);

        if (previewRow.holding.assetType.empty()) {
            previewRow.holding.assetType = "Stock";
        }

        if (accountId <= 0) {
            previewRow.errors.push_back("Select an account before importing.");
        }

        if (tickerColumn < 0) {
            previewRow.errors.push_back("Ticker column is not mapped.");
        }

        if (previewRow.holding.ticker.empty()) {
            previewRow.errors.push_back("Ticker is required.");
        }

        if (previewRow.holding.assetName.empty()) {
            previewRow.holding.assetName = previewRow.holding.ticker;
            previewRow.warnings.push_back("Asset name missing; using ticker.");
        }

        parseRequiredDouble(row, sharesColumn, "Shares", previewRow.holding.shares, previewRow.errors);
        parseRequiredDouble(row, currentPriceColumn, "Current price", previewRow.holding.currentPrice, previewRow.errors);

        if (const std::optional<double> sourceGainLossDollar = parseOptionalDouble(row, gainLossDollarColumn, "Gain/loss dollar", previewRow.errors)) {
            previewRow.hasSourceGainLossDollar = true;
            previewRow.sourceGainLossDollar = *sourceGainLossDollar;
        }

        if (const std::optional<double> sourceGainLossPercent = parseOptionalDouble(row, gainLossPercentColumn, "Gain/loss percent", previewRow.errors)) {
            previewRow.hasSourceGainLossPercent = true;
            previewRow.sourceGainLossPercent = *sourceGainLossPercent;
        }

        if (const std::optional<double> directAverageCost = parseOptionalDouble(row, averageCostColumn, "Average cost", previewRow.errors)) {
            previewRow.holding.averageCost = *directAverageCost;
            previewRow.totalCostBasis = previewRow.holding.shares * previewRow.holding.averageCost;
            previewRow.hasTotalCostBasis = true;
        } else if (const std::optional<double> totalCostBasis = parseOptionalDouble(row, totalCostBasisColumn, "Total cost basis", previewRow.errors)) {
            previewRow.hasTotalCostBasis = true;
            previewRow.totalCostBasis = *totalCostBasis;
            if (previewRow.holding.shares != 0.0) {
                previewRow.holding.averageCost = *totalCostBasis / previewRow.holding.shares;
            } else {
                previewRow.holding.averageCost = 0.0;
                previewRow.warnings.push_back("Total cost basis present but shares are zero; average cost set to 0.00.");
            }
        } else {
            previewRow.holding.averageCost = 0.0;
            previewRow.warnings.push_back("Average cost missing; importing as 0.00.");
        }

        previewRow.marketValue = previewRow.holding.shares * previewRow.holding.currentPrice;
        const double costBasisForGainLoss = previewRow.hasTotalCostBasis
            ? previewRow.totalCostBasis
            : previewRow.holding.shares * previewRow.holding.averageCost;
        previewRow.gainLossDollar = previewRow.marketValue - costBasisForGainLoss;
        previewRow.gainLossPercent = costBasisForGainLoss != 0.0
            ? previewRow.gainLossDollar / costBasisForGainLoss * 100.0
            : 0.0;

        validateMoney(previewRow.holding.shares, "Shares", previewRow.errors);
        validateMoney(previewRow.holding.averageCost, "Average cost", previewRow.errors);
        validateMoney(previewRow.holding.currentPrice, "Current price", previewRow.errors);

        const std::string key = duplicateKey(accountId, previewRow.holding.ticker, previewRow.holding.assetName);
        if (existingKeys.contains(key)) {
            previewRow.errors.push_back("Duplicate existing holding for this account.");
        }

        if (seenImportKeys.contains(key)) {
            previewRow.errors.push_back("Duplicate row in this CSV preview.");
        } else {
            seenImportKeys.insert(key);
        }

        preview.push_back(previewRow);
    }

    return preview;
}

}
