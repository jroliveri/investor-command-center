// SPDX-License-Identifier: MIT
#include "repositories/MarketQuoteCacheRepository.hpp"

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

MarketQuote readQuote(sqlite3_stmt* statement)
{
    MarketQuote quote;
    quote.id = sqlite3_column_int(statement, 0);
    quote.symbol = textColumn(statement, 1);
    quote.provider = textColumn(statement, 2);
    quote.companyName = textColumn(statement, 3);
    quote.currentPrice = optionalDoubleColumn(statement, 4);
    quote.previousClose = optionalDoubleColumn(statement, 5);
    quote.openPrice = optionalDoubleColumn(statement, 6);
    quote.dayHigh = optionalDoubleColumn(statement, 7);
    quote.dayLow = optionalDoubleColumn(statement, 8);
    quote.fiftyTwoWeekHigh = optionalDoubleColumn(statement, 9);
    quote.fiftyTwoWeekLow = optionalDoubleColumn(statement, 10);
    quote.marketCap = optionalDoubleColumn(statement, 11);
    quote.volume = optionalDoubleColumn(statement, 12);
    quote.averageVolume = optionalDoubleColumn(statement, 13);
    quote.peRatio = optionalDoubleColumn(statement, 14);
    quote.eps = optionalDoubleColumn(statement, 15);
    quote.dividendYield = optionalDoubleColumn(statement, 16);
    quote.beta = optionalDoubleColumn(statement, 17);
    quote.currency = textColumn(statement, 18);
    quote.exchangeName = textColumn(statement, 19);
    quote.quoteTime = textColumn(statement, 20);
    quote.fetchedAt = textColumn(statement, 21);
    quote.rawStatus = textColumn(statement, 22);
    quote.createdAt = textColumn(statement, 23);
    quote.updatedAt = textColumn(statement, 24);
    return quote;
}

} // namespace

MarketQuoteCacheRepository::MarketQuoteCacheRepository(Database& database)
    : database_(database)
{
}

std::optional<MarketQuote> MarketQuoteCacheRepository::findBySymbol(const std::string& symbol, std::string& error) const
{
    error.clear();
    const std::string normalizedSymbol = normalizeSymbol(symbol);
    if (normalizedSymbol.empty()) {
        return std::nullopt;
    }

    sqlite3_stmt* statement = nullptr;
    if (!database_.prepare(
            "SELECT id, symbol, provider, company_name, current_price, previous_close, open_price, day_high, day_low, "
            "fifty_two_week_high, fifty_two_week_low, market_cap, volume, average_volume, pe_ratio, eps, "
            "dividend_yield, beta, currency, exchange_name, quote_time, fetched_at, raw_status, created_at, updated_at "
            "FROM market_quote_cache WHERE symbol = ? LIMIT 1;",
            &statement)) {
        error = database_.lastError();
        return std::nullopt;
    }

    sqlite3_bind_text(statement, 1, normalizedSymbol.c_str(), -1, SQLITE_TRANSIENT);
    const int result = sqlite3_step(statement);
    if (result == SQLITE_ROW) {
        MarketQuote quote = readQuote(statement);
        sqlite3_finalize(statement);
        return quote;
    }

    if (result != SQLITE_DONE) {
        error = sqlite3_errmsg(database_.handle());
    }

    sqlite3_finalize(statement);
    return std::nullopt;
}

bool MarketQuoteCacheRepository::upsert(const MarketQuote& quote, std::string& error) const
{
    error.clear();
    const std::string timestamp = Date::nowIso8601();
    const std::string symbol = normalizeSymbol(quote.symbol);
    if (symbol.empty()) {
        error = "Cannot cache a market quote without a symbol.";
        return false;
    }

    sqlite3_stmt* statement = nullptr;
    if (!database_.prepare(
            "INSERT INTO market_quote_cache(symbol, provider, company_name, current_price, previous_close, open_price, "
            "day_high, day_low, fifty_two_week_high, fifty_two_week_low, market_cap, volume, average_volume, pe_ratio, "
            "eps, dividend_yield, beta, currency, exchange_name, quote_time, fetched_at, raw_status, created_at, updated_at) "
            "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?) "
            "ON CONFLICT(symbol) DO UPDATE SET "
            "provider = excluded.provider, company_name = excluded.company_name, current_price = excluded.current_price, "
            "previous_close = excluded.previous_close, open_price = excluded.open_price, day_high = excluded.day_high, "
            "day_low = excluded.day_low, fifty_two_week_high = excluded.fifty_two_week_high, "
            "fifty_two_week_low = excluded.fifty_two_week_low, market_cap = excluded.market_cap, volume = excluded.volume, "
            "average_volume = excluded.average_volume, pe_ratio = excluded.pe_ratio, eps = excluded.eps, "
            "dividend_yield = excluded.dividend_yield, beta = excluded.beta, currency = excluded.currency, "
            "exchange_name = excluded.exchange_name, quote_time = excluded.quote_time, fetched_at = excluded.fetched_at, "
            "raw_status = excluded.raw_status, updated_at = excluded.updated_at;",
            &statement)) {
        error = database_.lastError();
        return false;
    }

    sqlite3_bind_text(statement, 1, symbol.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(statement, 2, quote.provider.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(statement, 3, quote.companyName.c_str(), -1, SQLITE_TRANSIENT);
    bindOptionalDouble(statement, 4, quote.currentPrice);
    bindOptionalDouble(statement, 5, quote.previousClose);
    bindOptionalDouble(statement, 6, quote.openPrice);
    bindOptionalDouble(statement, 7, quote.dayHigh);
    bindOptionalDouble(statement, 8, quote.dayLow);
    bindOptionalDouble(statement, 9, quote.fiftyTwoWeekHigh);
    bindOptionalDouble(statement, 10, quote.fiftyTwoWeekLow);
    bindOptionalDouble(statement, 11, quote.marketCap);
    bindOptionalDouble(statement, 12, quote.volume);
    bindOptionalDouble(statement, 13, quote.averageVolume);
    bindOptionalDouble(statement, 14, quote.peRatio);
    bindOptionalDouble(statement, 15, quote.eps);
    bindOptionalDouble(statement, 16, quote.dividendYield);
    bindOptionalDouble(statement, 17, quote.beta);
    sqlite3_bind_text(statement, 18, quote.currency.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(statement, 19, quote.exchangeName.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(statement, 20, quote.quoteTime.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(statement, 21, quote.fetchedAt.empty() ? timestamp.c_str() : quote.fetchedAt.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(statement, 22, quote.rawStatus.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(statement, 23, timestamp.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(statement, 24, timestamp.c_str(), -1, SQLITE_TRANSIENT);

    if (sqlite3_step(statement) != SQLITE_DONE) {
        error = sqlite3_errmsg(database_.handle());
        sqlite3_finalize(statement);
        return false;
    }

    sqlite3_finalize(statement);
    return true;
}
