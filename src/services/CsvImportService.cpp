// SPDX-License-Identifier: MIT
#include "services/CsvImportService.hpp"

#include "db/Database.hpp"
#include "models/ImportBatch.hpp"
#include "repositories/HoldingRepository.hpp"
#include "repositories/ImportBatchRepository.hpp"
#include "repositories/PortfolioSnapshotRepository.hpp"
#include "services/PerformanceCalculator.hpp"
#include "services/PortfolioCalculator.hpp"

#include <algorithm>
#include <cctype>
#include <map>
#include <set>

namespace {

std::string upperCopy(std::string value)
{
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char character) {
        return static_cast<char>(std::toupper(character));
    });
    return value;
}

bool isActive(const Holding& holding)
{
    return holding.status.empty() || holding.status == "Active";
}

std::string buildSummaryMessage(const CsvImportSummary& summary)
{
    return "CSV import complete: " +
        std::to_string(summary.rowsImported) + " rows imported, " +
        std::to_string(summary.holdingsUpdated) + " updated, " +
        std::to_string(summary.holdingsAdded) + " added, " +
        std::to_string(summary.rowsSkipped) + " skipped, " +
        std::to_string(summary.holdingsMissing) + " missing marked inactive. " +
        (summary.snapshotUpdated
            ? "Snapshot for this account/date was updated from latest CSV import."
            : "Snapshot created from CSV import.");
}

}

CsvImportService::CsvImportService(
    Database& database,
    HoldingRepository& holdingRepository,
    ImportBatchRepository& importBatchRepository,
    PortfolioSnapshotRepository& snapshotRepository)
    : database_(database)
    , holdingRepository_(holdingRepository)
    , importBatchRepository_(importBatchRepository)
    , snapshotRepository_(snapshotRepository)
{
}

bool CsvImportService::importHoldings(
    const AppState& state,
    int accountId,
    const std::vector<HoldingsCsvPreviewRow>& previewRows,
    const std::string& importDate,
    const std::string& sourceName,
    CsvImportSummary& summary,
    std::string& error) const
{
    summary = {};
    summary.importDate = importDate;
    error.clear();

    if (accountId <= 0) {
        error = "Select an account before importing.";
        return false;
    }

    std::map<std::string, Holding> existingByTicker;
    for (const Holding& holding : state.holdings) {
        if (holding.accountId == accountId) {
            existingByTicker[upperCopy(holding.ticker)] = holding;
        }
    }

    std::set<std::string> incomingTickers;
    for (const HoldingsCsvPreviewRow& row : previewRows) {
        summary.warningCount += static_cast<int>(row.warnings.size());
        summary.errorCount += static_cast<int>(row.errors.size());
        if (!row.valid()) {
            ++summary.rowsSkipped;
            continue;
        }

        ++summary.rowsImported;
        incomingTickers.insert(upperCopy(row.holding.ticker));
        if (existingByTicker.contains(upperCopy(row.holding.ticker))) {
            ++summary.holdingsUpdated;
        } else {
            ++summary.holdingsAdded;
        }
    }

    for (const auto& [ticker, holding] : existingByTicker) {
        if (isActive(holding) && !incomingTickers.contains(ticker)) {
            ++summary.holdingsMissing;
        }
    }

    ImportBatch batch;
    batch.accountId = accountId;
    batch.importDate = importDate;
    batch.sourceType = "CSV";
    batch.sourceName = sourceName;
    batch.totalRows = static_cast<int>(previewRows.size());
    batch.importedRows = summary.rowsImported;
    batch.updatedHoldings = summary.holdingsUpdated;
    batch.addedHoldings = summary.holdingsAdded;
    batch.skippedRows = summary.rowsSkipped;
    batch.warningCount = summary.warningCount;
    batch.errorCount = summary.errorCount;
    batch.missingHoldings = summary.holdingsMissing;
    batch.notes = "Holdings upserted by account and ticker.";

    if (!database_.execute("BEGIN TRANSACTION;")) {
        error = database_.lastError();
        return false;
    }

    if (!importBatchRepository_.create(batch, error)) {
        database_.execute("ROLLBACK;");
        return false;
    }

    for (const HoldingsCsvPreviewRow& row : previewRows) {
        if (!row.valid()) {
            continue;
        }

        const std::string ticker = upperCopy(row.holding.ticker);
        const auto existing = existingByTicker.find(ticker);
        if (existing == existingByTicker.end()) {
            Holding created = row.holding;
            created.accountId = accountId;
            created.status = "Active";
            created.lastImportBatchId = batch.id;
            created.lastSeenAt = importDate;
            if (!row.hasAverageCost) {
                created.averageCost = 0.0;
            }
            if (!holdingRepository_.create(created, error)) {
                database_.execute("ROLLBACK;");
                return false;
            }
            existingByTicker[ticker] = created;
            continue;
        }

        Holding updated = existing->second;
        updated.status = "Active";
        updated.lastImportBatchId = batch.id;
        updated.lastSeenAt = importDate;
        updated.shares = row.holding.shares;
        updated.currentPrice = row.holding.currentPrice;
        if (row.hasAverageCost) {
            updated.averageCost = row.holding.averageCost;
        }
        if (row.hasAssetName) {
            updated.assetName = row.holding.assetName;
        }
        if (row.hasAssetType) {
            updated.assetType = row.holding.assetType;
        }
        if (row.hasNotes) {
            updated.notes = row.holding.notes;
        }

        if (!holdingRepository_.update(updated, error)) {
            database_.execute("ROLLBACK;");
            return false;
        }
        existing->second = updated;
    }

    for (const auto& [ticker, holding] : existingByTicker) {
        if (!isActive(holding) || incomingTickers.contains(ticker)) {
            continue;
        }

        Holding missing = holding;
        missing.status = "Inactive";
        missing.lastImportBatchId = batch.id;
        if (!holdingRepository_.update(missing, error)) {
            database_.execute("ROLLBACK;");
            return false;
        }
    }

    std::vector<Holding> updatedHoldings = holdingRepository_.listAll(error);
    if (!error.empty()) {
        database_.execute("ROLLBACK;");
        return false;
    }

    const PortfolioSummary portfolio = PortfolioCalculator::calculateSummary(state.accounts, updatedHoldings);
    const RealizedGainSummary realized = PerformanceCalculator::calculateRealizedGains(
        state.transactions,
        importDate,
        importDate.substr(0, 7),
        importDate.substr(0, 4));

    PortfolioSnapshot snapshot;
    snapshot.accountId = accountId;
    snapshot.importBatchId = batch.id;
    snapshot.snapshotDate = importDate;
    snapshot.source = "CSV Import";
    snapshot.portfolioValue = portfolio.accountBalance;
    snapshot.holdingsValue = portfolio.holdingsMarketValue;
    snapshot.cashBalance = portfolio.cashBalance;
    snapshot.costBasis = portfolio.costBasis;
    snapshot.unrealizedGain = portfolio.gainLossDollar;
    snapshot.realizedGainDay = realized.today;
    snapshot.realizedGainMonth = realized.thisMonth;
    snapshot.realizedGainYear = realized.thisYear;
    snapshot.notes = "Created automatically after CSV import.";

    summary.snapshotUpdated = snapshotRepository_.existsForAccountDateSource(accountId, importDate, "CSV Import", error);
    if (!error.empty()) {
        database_.execute("ROLLBACK;");
        return false;
    }

    if (!snapshotRepository_.upsertForAccountDateSource(snapshot, error)) {
        database_.execute("ROLLBACK;");
        return false;
    }

    if (!database_.execute("COMMIT;")) {
        error = database_.lastError();
        return false;
    }

    summary.message = buildSummaryMessage(summary);
    return true;
}
