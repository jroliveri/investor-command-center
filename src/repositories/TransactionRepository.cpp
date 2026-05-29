// SPDX-License-Identifier: MIT
#include "repositories/TransactionRepository.hpp"

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

int nullableIntColumn(sqlite3_stmt* statement, int column)
{
    return sqlite3_column_type(statement, column) == SQLITE_NULL ? 0 : sqlite3_column_int(statement, column);
}

bool isBlank(const std::string& value)
{
    return std::all_of(value.begin(), value.end(), [](unsigned char character) {
        return std::isspace(character) != 0;
    });
}

void normalizeTicker(std::string& ticker)
{
    std::transform(ticker.begin(), ticker.end(), ticker.begin(), [](unsigned char value) {
        return static_cast<char>(std::toupper(value));
    });
}

void bindNullableAccount(sqlite3_stmt* statement, int index, int accountId)
{
    if (accountId > 0) {
        sqlite3_bind_int(statement, index, accountId);
    } else {
        sqlite3_bind_null(statement, index);
    }
}

}

TransactionRepository::TransactionRepository(Database& database)
    : database_(database)
{
}

std::vector<Transaction> TransactionRepository::listAll(std::string& error) const
{
    error.clear();
    std::vector<Transaction> transactions;

    sqlite3_stmt* statement = nullptr;
    if (!database_.prepare(
            "SELECT id, account_id, ticker, asset_name, transaction_type, transaction_date, quantity, price, "
            "fees, total_amount, sold_quantity, sale_price, sale_proceeds, cost_basis_used, "
            "realized_gain_dollar, realized_gain_percent, is_adjustment, notes, created_at, updated_at "
            "FROM transactions ORDER BY transaction_date DESC, id DESC;",
            &statement)) {
        error = database_.lastError();
        return transactions;
    }

    while (sqlite3_step(statement) == SQLITE_ROW) {
        Transaction transaction;
        transaction.id = sqlite3_column_int(statement, 0);
        transaction.accountId = nullableIntColumn(statement, 1);
        transaction.ticker = textColumn(statement, 2);
        transaction.assetName = textColumn(statement, 3);
        transaction.transactionType = textColumn(statement, 4);
        transaction.transactionDate = textColumn(statement, 5);
        transaction.quantity = sqlite3_column_double(statement, 6);
        transaction.price = sqlite3_column_double(statement, 7);
        transaction.fees = sqlite3_column_double(statement, 8);
        transaction.totalAmount = sqlite3_column_double(statement, 9);
        transaction.soldQuantity = sqlite3_column_double(statement, 10);
        transaction.salePrice = sqlite3_column_double(statement, 11);
        transaction.saleProceeds = sqlite3_column_double(statement, 12);
        transaction.costBasisUsed = sqlite3_column_double(statement, 13);
        transaction.realizedGainDollar = sqlite3_column_double(statement, 14);
        transaction.realizedGainPercent = sqlite3_column_double(statement, 15);
        transaction.isAdjustment = sqlite3_column_int(statement, 16) != 0;
        transaction.notes = textColumn(statement, 17);
        transaction.createdAt = textColumn(statement, 18);
        transaction.updatedAt = textColumn(statement, 19);
        transactions.push_back(transaction);
    }

    sqlite3_finalize(statement);
    return transactions;
}

bool TransactionRepository::create(Transaction& transaction, std::string& error) const
{
    error.clear();
    normalizeTicker(transaction.ticker);
    if (!validate(transaction, error)) {
        return false;
    }

    const std::string timestamp = Date::nowIso8601();
    transaction.createdAt = timestamp;
    transaction.updatedAt = timestamp;

    sqlite3_stmt* statement = nullptr;
    if (!database_.prepare(
            "INSERT INTO transactions(account_id, ticker, asset_name, transaction_type, transaction_date, quantity, "
            "price, fees, total_amount, sold_quantity, sale_price, sale_proceeds, cost_basis_used, "
            "realized_gain_dollar, realized_gain_percent, is_adjustment, notes, created_at, updated_at) "
            "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?);",
            &statement)) {
        error = database_.lastError();
        return false;
    }

    bindNullableAccount(statement, 1, transaction.accountId);
    sqlite3_bind_text(statement, 2, transaction.ticker.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(statement, 3, transaction.assetName.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(statement, 4, transaction.transactionType.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(statement, 5, transaction.transactionDate.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_double(statement, 6, transaction.quantity);
    sqlite3_bind_double(statement, 7, transaction.price);
    sqlite3_bind_double(statement, 8, transaction.fees);
    sqlite3_bind_double(statement, 9, transaction.totalAmount);
    sqlite3_bind_double(statement, 10, transaction.soldQuantity);
    sqlite3_bind_double(statement, 11, transaction.salePrice);
    sqlite3_bind_double(statement, 12, transaction.saleProceeds);
    sqlite3_bind_double(statement, 13, transaction.costBasisUsed);
    sqlite3_bind_double(statement, 14, transaction.realizedGainDollar);
    sqlite3_bind_double(statement, 15, transaction.realizedGainPercent);
    sqlite3_bind_int(statement, 16, transaction.isAdjustment ? 1 : 0);
    sqlite3_bind_text(statement, 17, transaction.notes.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(statement, 18, transaction.createdAt.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(statement, 19, transaction.updatedAt.c_str(), -1, SQLITE_TRANSIENT);

    if (sqlite3_step(statement) != SQLITE_DONE) {
        error = sqlite3_errmsg(database_.handle());
        sqlite3_finalize(statement);
        return false;
    }

    sqlite3_finalize(statement);
    transaction.id = static_cast<int>(database_.lastInsertRowId());
    return true;
}

bool TransactionRepository::update(const Transaction& transaction, std::string& error) const
{
    error.clear();
    if (transaction.id <= 0) {
        error = "Cannot update a transaction without a database id.";
        return false;
    }

    Transaction normalized = transaction;
    normalizeTicker(normalized.ticker);
    if (!validate(normalized, error)) {
        return false;
    }

    const std::string timestamp = Date::nowIso8601();

    sqlite3_stmt* statement = nullptr;
    if (!database_.prepare(
            "UPDATE transactions SET account_id = ?, ticker = ?, asset_name = ?, transaction_type = ?, "
            "transaction_date = ?, quantity = ?, price = ?, fees = ?, total_amount = ?, sold_quantity = ?, "
            "sale_price = ?, sale_proceeds = ?, cost_basis_used = ?, realized_gain_dollar = ?, "
            "realized_gain_percent = ?, is_adjustment = ?, notes = ?, updated_at = ? "
            "WHERE id = ?;",
            &statement)) {
        error = database_.lastError();
        return false;
    }

    bindNullableAccount(statement, 1, normalized.accountId);
    sqlite3_bind_text(statement, 2, normalized.ticker.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(statement, 3, normalized.assetName.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(statement, 4, normalized.transactionType.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(statement, 5, normalized.transactionDate.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_double(statement, 6, normalized.quantity);
    sqlite3_bind_double(statement, 7, normalized.price);
    sqlite3_bind_double(statement, 8, normalized.fees);
    sqlite3_bind_double(statement, 9, normalized.totalAmount);
    sqlite3_bind_double(statement, 10, normalized.soldQuantity);
    sqlite3_bind_double(statement, 11, normalized.salePrice);
    sqlite3_bind_double(statement, 12, normalized.saleProceeds);
    sqlite3_bind_double(statement, 13, normalized.costBasisUsed);
    sqlite3_bind_double(statement, 14, normalized.realizedGainDollar);
    sqlite3_bind_double(statement, 15, normalized.realizedGainPercent);
    sqlite3_bind_int(statement, 16, normalized.isAdjustment ? 1 : 0);
    sqlite3_bind_text(statement, 17, normalized.notes.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(statement, 18, timestamp.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(statement, 19, normalized.id);

    if (sqlite3_step(statement) != SQLITE_DONE) {
        error = sqlite3_errmsg(database_.handle());
        sqlite3_finalize(statement);
        return false;
    }

    sqlite3_finalize(statement);
    return true;
}

bool TransactionRepository::remove(int id, std::string& error) const
{
    error.clear();

    sqlite3_stmt* statement = nullptr;
    if (!database_.prepare("DELETE FROM transactions WHERE id = ?;", &statement)) {
        error = database_.lastError();
        return false;
    }

    sqlite3_bind_int(statement, 1, id);
    if (sqlite3_step(statement) != SQLITE_DONE) {
        error = sqlite3_errmsg(database_.handle());
        sqlite3_finalize(statement);
        return false;
    }

    sqlite3_finalize(statement);
    return true;
}

bool TransactionRepository::validate(const Transaction& transaction, std::string& error)
{
    error.clear();

    if (transaction.transactionType.empty() || isBlank(transaction.transactionType)) {
        error = "Transaction type is required.";
        return false;
    }

    if (transaction.transactionDate.empty() || isBlank(transaction.transactionDate)) {
        error = "Transaction date is required.";
        return false;
    }

    if (!Date::isIsoDate(transaction.transactionDate)) {
        error = "Transaction date must use YYYY-MM-DD.";
        return false;
    }

    if (transaction.quantity < 0.0 || transaction.price < 0.0 || transaction.fees < 0.0 ||
        transaction.soldQuantity < 0.0 || transaction.salePrice < 0.0 || transaction.saleProceeds < 0.0 ||
        transaction.costBasisUsed < 0.0) {
        error = "Quantity, price, fees, and sell calculation fields cannot be negative.";
        return false;
    }

    return true;
}
