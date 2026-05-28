// SPDX-License-Identifier: MIT
#pragma once

#include "models/Holding.hpp"

#include <string>
#include <vector>

enum class HoldingsCsvImportField {
    Ignore,
    Ticker,
    AssetName,
    Shares,
    CurrentPrice,
    AverageCost,
    TotalCostBasis,
    GainLossDollar,
    GainLossPercent,
    AssetType,
    Notes,
};

struct CsvDataRow {
    int sourceRowNumber = 0;
    std::vector<std::string> cells;
};

struct CsvTable {
    std::vector<std::string> headers;
    std::vector<CsvDataRow> rows;
    int headerRowNumber = 0;
    int skippedMetadataRowCount = 0;
    int skippedBlankRowCount = 0;
    int skippedSummaryRowCount = 0;
};

struct HoldingsCsvColumnMapping {
    std::vector<HoldingsCsvImportField> fields;
};

struct HoldingsCsvPreviewRow {
    int sourceRowNumber = 0;
    Holding holding;
    bool hasTotalCostBasis = false;
    double totalCostBasis = 0.0;
    double marketValue = 0.0;
    double gainLossDollar = 0.0;
    double gainLossPercent = 0.0;
    bool hasSourceGainLossDollar = false;
    double sourceGainLossDollar = 0.0;
    bool hasSourceGainLossPercent = false;
    double sourceGainLossPercent = 0.0;
    std::vector<std::string> errors;
    std::vector<std::string> warnings;

    bool valid() const { return errors.empty(); }
};

namespace HoldingsCsvImport {
bool loadCsvFile(const std::string& path, CsvTable& table, std::string& error, int requestedHeaderRowNumber = 0);
HoldingsCsvColumnMapping autoMapColumns(const std::vector<std::string>& headers);
const char* importFieldLabel(HoldingsCsvImportField field);
int findMappedColumn(const HoldingsCsvColumnMapping& mapping, HoldingsCsvImportField field);
std::vector<HoldingsCsvPreviewRow> buildPreview(
    const CsvTable& table,
    const HoldingsCsvColumnMapping& mapping,
    int accountId,
    const std::vector<Holding>& existingHoldings);
}
