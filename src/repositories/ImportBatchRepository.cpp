// SPDX-License-Identifier: MIT
#include "repositories/ImportBatchRepository.hpp"

#include "db/Database.hpp"
#include "util/Date.hpp"

#include <algorithm>
#include <cctype>
#include <sqlite3.h>

namespace {

std::string textColumn(sqlite3_stmt* statement, int column)
{
    const unsigned char* text = sqlite3_column_text(statement, column);
    return text == nullptr ? std::string() : reinterpret_cast<const char*>(text);
}

bool isBlank(const std::string& value)
{
    return std::all_of(value.begin(), value.end(), [](unsigned char character) {
        return std::isspace(character) != 0;
    });
}

ImportBatch readBatch(sqlite3_stmt* statement)
{
    ImportBatch batch;
    batch.id = sqlite3_column_int(statement, 0);
    batch.accountId = sqlite3_column_int(statement, 1);
    batch.importDate = textColumn(statement, 2);
    batch.sourceType = textColumn(statement, 3);
    batch.sourceName = textColumn(statement, 4);
    batch.totalRows = sqlite3_column_int(statement, 5);
    batch.importedRows = sqlite3_column_int(statement, 6);
    batch.updatedHoldings = sqlite3_column_int(statement, 7);
    batch.addedHoldings = sqlite3_column_int(statement, 8);
    batch.skippedRows = sqlite3_column_int(statement, 9);
    batch.warningCount = sqlite3_column_int(statement, 10);
    batch.errorCount = sqlite3_column_int(statement, 11);
    batch.missingHoldings = sqlite3_column_int(statement, 12);
    batch.notes = textColumn(statement, 13);
    batch.createdAt = textColumn(statement, 14);
    batch.updatedAt = textColumn(statement, 15);
    return batch;
}

}

ImportBatchRepository::ImportBatchRepository(Database& database)
    : database_(database)
{
}

std::vector<ImportBatch> ImportBatchRepository::listAll(std::string& error) const
{
    error.clear();
    std::vector<ImportBatch> batches;

    sqlite3_stmt* statement = nullptr;
    if (!database_.prepare(
            "SELECT id, account_id, import_date, source_type, source_name, total_rows, imported_rows, "
            "updated_holdings, added_holdings, skipped_rows, warning_count, error_count, missing_holdings, "
            "notes, created_at, updated_at "
            "FROM import_batches ORDER BY import_date DESC, id DESC;",
            &statement)) {
        error = database_.lastError();
        return batches;
    }

    while (sqlite3_step(statement) == SQLITE_ROW) {
        batches.push_back(readBatch(statement));
    }

    sqlite3_finalize(statement);
    return batches;
}

bool ImportBatchRepository::create(ImportBatch& batch, std::string& error) const
{
    error.clear();
    if (!validate(batch, error)) {
        return false;
    }

    const std::string timestamp = Date::nowIso8601();
    batch.createdAt = timestamp;
    batch.updatedAt = timestamp;

    sqlite3_stmt* statement = nullptr;
    if (!database_.prepare(
            "INSERT INTO import_batches(account_id, import_date, source_type, source_name, total_rows, "
            "imported_rows, updated_holdings, added_holdings, skipped_rows, warning_count, error_count, "
            "missing_holdings, notes, created_at, updated_at) "
            "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?);",
            &statement)) {
        error = database_.lastError();
        return false;
    }

    sqlite3_bind_int(statement, 1, batch.accountId);
    sqlite3_bind_text(statement, 2, batch.importDate.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(statement, 3, batch.sourceType.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(statement, 4, batch.sourceName.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(statement, 5, batch.totalRows);
    sqlite3_bind_int(statement, 6, batch.importedRows);
    sqlite3_bind_int(statement, 7, batch.updatedHoldings);
    sqlite3_bind_int(statement, 8, batch.addedHoldings);
    sqlite3_bind_int(statement, 9, batch.skippedRows);
    sqlite3_bind_int(statement, 10, batch.warningCount);
    sqlite3_bind_int(statement, 11, batch.errorCount);
    sqlite3_bind_int(statement, 12, batch.missingHoldings);
    sqlite3_bind_text(statement, 13, batch.notes.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(statement, 14, batch.createdAt.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(statement, 15, batch.updatedAt.c_str(), -1, SQLITE_TRANSIENT);

    if (sqlite3_step(statement) != SQLITE_DONE) {
        error = sqlite3_errmsg(database_.handle());
        sqlite3_finalize(statement);
        return false;
    }

    sqlite3_finalize(statement);
    batch.id = static_cast<int>(database_.lastInsertRowId());
    return true;
}

bool ImportBatchRepository::validate(const ImportBatch& batch, std::string& error)
{
    error.clear();

    if (batch.accountId <= 0) {
        error = "Import batch account is required.";
        return false;
    }

    if (batch.importDate.empty() || isBlank(batch.importDate)) {
        error = "Import date is required.";
        return false;
    }

    if (!Date::isIsoDate(batch.importDate)) {
        error = "Import date must use YYYY-MM-DD.";
        return false;
    }

    return true;
}
