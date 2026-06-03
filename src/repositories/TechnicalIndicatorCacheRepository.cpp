// SPDX-License-Identifier: MIT
#include "repositories/TechnicalIndicatorCacheRepository.hpp"

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

std::optional<double> optionalDoubleColumn(sqlite3_stmt* statement, int column)
{
    if (sqlite3_column_type(statement, column) == SQLITE_NULL) {
        return std::nullopt;
    }
    return sqlite3_column_double(statement, column);
}

void bindOptionalDouble(sqlite3_stmt* statement, int index, const std::optional<double>& value)
{
    if (value.has_value()) {
        sqlite3_bind_double(statement, index, *value);
    } else {
        sqlite3_bind_null(statement, index);
    }
}

std::string normalizeSymbol(std::string symbol)
{
    symbol.erase(symbol.begin(), std::find_if(symbol.begin(), symbol.end(), [](unsigned char character) {
        return std::isspace(character) == 0;
    }));
    symbol.erase(std::find_if(symbol.rbegin(), symbol.rend(), [](unsigned char character) {
        return std::isspace(character) == 0;
    }).base(), symbol.end());
    std::transform(symbol.begin(), symbol.end(), symbol.begin(), [](unsigned char character) {
        return static_cast<char>(std::toupper(character));
    });
    return symbol;
}

TechnicalIndicatorSnapshot readSnapshot(sqlite3_stmt* statement)
{
    TechnicalIndicatorSnapshot snapshot;
    snapshot.id = sqlite3_column_int(statement, 0);
    snapshot.symbol = textColumn(statement, 1);
    snapshot.provider = textColumn(statement, 2);
    snapshot.calculatedForDate = textColumn(statement, 3);
    snapshot.rsi14 = optionalDoubleColumn(statement, 4);
    snapshot.macdLine = optionalDoubleColumn(statement, 5);
    snapshot.macdSignal = optionalDoubleColumn(statement, 6);
    snapshot.macdHistogram = optionalDoubleColumn(statement, 7);
    snapshot.latestVolume = optionalDoubleColumn(statement, 8);
    snapshot.avgVolume20 = optionalDoubleColumn(statement, 9);
    snapshot.avgVolume50 = optionalDoubleColumn(statement, 10);
    snapshot.volumeVsAvg20Percent = optionalDoubleColumn(statement, 11);
    snapshot.sourceHistoryRows = sqlite3_column_int(statement, 12);
    snapshot.calculatedAt = textColumn(statement, 13);
    snapshot.createdAt = textColumn(statement, 14);
    snapshot.updatedAt = textColumn(statement, 15);
    return snapshot;
}

} // namespace

TechnicalIndicatorCacheRepository::TechnicalIndicatorCacheRepository(Database& database)
    : database_(database)
{
}

std::optional<TechnicalIndicatorSnapshot> TechnicalIndicatorCacheRepository::findLatestBySymbol(const std::string& symbol, const std::string& provider, std::string& error) const
{
    error.clear();
    const std::string normalizedSymbol = normalizeSymbol(symbol);
    if (normalizedSymbol.empty()) {
        return std::nullopt;
    }

    sqlite3_stmt* statement = nullptr;
    if (!database_.prepare(
            "SELECT id, symbol, provider, calculated_for_date, rsi_14, macd_line, macd_signal, macd_histogram, "
            "latest_volume, avg_volume_20, avg_volume_50, volume_vs_avg_20_percent, source_history_rows, "
            "calculated_at, created_at, updated_at "
            "FROM technical_indicator_cache WHERE symbol = ? AND provider = ? "
            "ORDER BY calculated_for_date DESC, calculated_at DESC LIMIT 1;",
            &statement)) {
        error = database_.lastError();
        return std::nullopt;
    }

    sqlite3_bind_text(statement, 1, normalizedSymbol.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(statement, 2, provider.empty() ? "Yahoo Finance" : provider.c_str(), -1, SQLITE_TRANSIENT);
    const int result = sqlite3_step(statement);
    if (result == SQLITE_ROW) {
        TechnicalIndicatorSnapshot snapshot = readSnapshot(statement);
        sqlite3_finalize(statement);
        return snapshot;
    }

    if (result != SQLITE_DONE) {
        error = sqlite3_errmsg(database_.handle());
    }

    sqlite3_finalize(statement);
    return std::nullopt;
}

bool TechnicalIndicatorCacheRepository::upsert(const TechnicalIndicatorSnapshot& snapshot, std::string& error) const
{
    error.clear();
    const std::string timestamp = Date::nowIso8601();
    const std::string symbol = normalizeSymbol(snapshot.symbol);
    const std::string provider = snapshot.provider.empty() ? "Yahoo Finance" : snapshot.provider;
    if (symbol.empty()) {
        error = "Cannot cache indicators without a symbol.";
        return false;
    }
    if (snapshot.calculatedForDate.empty()) {
        error = "Cannot cache indicators without a calculated date.";
        return false;
    }

    sqlite3_stmt* statement = nullptr;
    if (!database_.prepare(
            "INSERT INTO technical_indicator_cache(symbol, provider, calculated_for_date, rsi_14, macd_line, macd_signal, "
            "macd_histogram, latest_volume, avg_volume_20, avg_volume_50, volume_vs_avg_20_percent, source_history_rows, "
            "calculated_at, created_at, updated_at) "
            "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?) "
            "ON CONFLICT(symbol, provider, calculated_for_date) DO UPDATE SET "
            "rsi_14 = excluded.rsi_14, macd_line = excluded.macd_line, macd_signal = excluded.macd_signal, "
            "macd_histogram = excluded.macd_histogram, latest_volume = excluded.latest_volume, "
            "avg_volume_20 = excluded.avg_volume_20, avg_volume_50 = excluded.avg_volume_50, "
            "volume_vs_avg_20_percent = excluded.volume_vs_avg_20_percent, "
            "source_history_rows = excluded.source_history_rows, calculated_at = excluded.calculated_at, "
            "updated_at = excluded.updated_at;",
            &statement)) {
        error = database_.lastError();
        return false;
    }

    sqlite3_bind_text(statement, 1, symbol.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(statement, 2, provider.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(statement, 3, snapshot.calculatedForDate.c_str(), -1, SQLITE_TRANSIENT);
    bindOptionalDouble(statement, 4, snapshot.rsi14);
    bindOptionalDouble(statement, 5, snapshot.macdLine);
    bindOptionalDouble(statement, 6, snapshot.macdSignal);
    bindOptionalDouble(statement, 7, snapshot.macdHistogram);
    bindOptionalDouble(statement, 8, snapshot.latestVolume);
    bindOptionalDouble(statement, 9, snapshot.avgVolume20);
    bindOptionalDouble(statement, 10, snapshot.avgVolume50);
    bindOptionalDouble(statement, 11, snapshot.volumeVsAvg20Percent);
    sqlite3_bind_int(statement, 12, snapshot.sourceHistoryRows);
    sqlite3_bind_text(statement, 13, snapshot.calculatedAt.empty() ? timestamp.c_str() : snapshot.calculatedAt.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(statement, 14, timestamp.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(statement, 15, timestamp.c_str(), -1, SQLITE_TRANSIENT);

    if (sqlite3_step(statement) != SQLITE_DONE) {
        error = sqlite3_errmsg(database_.handle());
        sqlite3_finalize(statement);
        return false;
    }

    sqlite3_finalize(statement);
    return true;
}
