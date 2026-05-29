// SPDX-License-Identifier: MIT
#pragma once

#include "app/AppState.hpp"
#include "services/HoldingsCsvImport.hpp"

#include <string>
#include <vector>

class Database;
class HoldingRepository;
class ImportBatchRepository;
class PortfolioSnapshotRepository;

struct CsvImportSummary {
    int rowsImported = 0;
    int holdingsUpdated = 0;
    int holdingsAdded = 0;
    int rowsSkipped = 0;
    int holdingsMissing = 0;
    int warningCount = 0;
    int errorCount = 0;
    bool snapshotUpdated = false;
    std::string importDate;
    std::string message;
};

class CsvImportService {
public:
    CsvImportService(
        Database& database,
        HoldingRepository& holdingRepository,
        ImportBatchRepository& importBatchRepository,
        PortfolioSnapshotRepository& snapshotRepository);

    bool importHoldings(
        const AppState& state,
        int accountId,
        const std::vector<HoldingsCsvPreviewRow>& previewRows,
        const std::string& importDate,
        const std::string& sourceName,
        CsvImportSummary& summary,
        std::string& error) const;

private:
    Database& database_;
    HoldingRepository& holdingRepository_;
    ImportBatchRepository& importBatchRepository_;
    PortfolioSnapshotRepository& snapshotRepository_;
};
