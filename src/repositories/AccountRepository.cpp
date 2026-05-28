// SPDX-License-Identifier: MIT
#include "repositories/AccountRepository.hpp"

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

}

AccountRepository::AccountRepository(Database& database)
    : database_(database)
{
}

std::vector<Account> AccountRepository::listAll(std::string& error) const
{
    error.clear();
    std::vector<Account> accounts;

    sqlite3_stmt* statement = nullptr;
    if (!database_.prepare(
            "SELECT id, account_name, account_type, institution_name, current_balance, cash_balance, "
            "notes, status, created_at, updated_at "
            "FROM accounts ORDER BY account_name COLLATE NOCASE;",
            &statement)) {
        error = database_.lastError();
        return accounts;
    }

    while (sqlite3_step(statement) == SQLITE_ROW) {
        Account account;
        account.id = sqlite3_column_int(statement, 0);
        account.accountName = textColumn(statement, 1);
        account.accountType = textColumn(statement, 2);
        account.institutionName = textColumn(statement, 3);
        account.currentBalance = sqlite3_column_double(statement, 4);
        account.cashBalance = sqlite3_column_double(statement, 5);
        account.notes = textColumn(statement, 6);
        account.status = textColumn(statement, 7);
        account.createdAt = textColumn(statement, 8);
        account.updatedAt = textColumn(statement, 9);
        accounts.push_back(account);
    }

    sqlite3_finalize(statement);
    return accounts;
}

bool AccountRepository::create(Account& account, std::string& error) const
{
    error.clear();
    if (!validate(account, error)) {
        return false;
    }

    const std::string timestamp = Date::nowIso8601();
    account.createdAt = timestamp;
    account.updatedAt = timestamp;

    sqlite3_stmt* statement = nullptr;
    if (!database_.prepare(
            "INSERT INTO accounts(account_name, account_type, institution_name, current_balance, cash_balance, "
            "notes, status, created_at, updated_at) "
            "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?);",
            &statement)) {
        error = database_.lastError();
        return false;
    }

    sqlite3_bind_text(statement, 1, account.accountName.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(statement, 2, account.accountType.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(statement, 3, account.institutionName.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_double(statement, 4, account.currentBalance);
    sqlite3_bind_double(statement, 5, account.cashBalance);
    sqlite3_bind_text(statement, 6, account.notes.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(statement, 7, account.status.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(statement, 8, account.createdAt.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(statement, 9, account.updatedAt.c_str(), -1, SQLITE_TRANSIENT);

    if (sqlite3_step(statement) != SQLITE_DONE) {
        error = sqlite3_errmsg(database_.handle());
        sqlite3_finalize(statement);
        return false;
    }

    sqlite3_finalize(statement);
    account.id = static_cast<int>(database_.lastInsertRowId());
    return true;
}

bool AccountRepository::update(const Account& account, std::string& error) const
{
    error.clear();
    if (account.id <= 0) {
        error = "Cannot update an account without a database id.";
        return false;
    }

    if (!validate(account, error)) {
        return false;
    }

    const std::string timestamp = Date::nowIso8601();

    sqlite3_stmt* statement = nullptr;
    if (!database_.prepare(
            "UPDATE accounts SET account_name = ?, account_type = ?, institution_name = ?, "
            "current_balance = ?, cash_balance = ?, notes = ?, status = ?, updated_at = ? "
            "WHERE id = ?;",
            &statement)) {
        error = database_.lastError();
        return false;
    }

    sqlite3_bind_text(statement, 1, account.accountName.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(statement, 2, account.accountType.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(statement, 3, account.institutionName.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_double(statement, 4, account.currentBalance);
    sqlite3_bind_double(statement, 5, account.cashBalance);
    sqlite3_bind_text(statement, 6, account.notes.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(statement, 7, account.status.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(statement, 8, timestamp.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(statement, 9, account.id);

    if (sqlite3_step(statement) != SQLITE_DONE) {
        error = sqlite3_errmsg(database_.handle());
        sqlite3_finalize(statement);
        return false;
    }

    sqlite3_finalize(statement);
    return true;
}

bool AccountRepository::remove(int id, std::string& error) const
{
    error.clear();

    sqlite3_stmt* statement = nullptr;
    if (!database_.prepare("DELETE FROM accounts WHERE id = ?;", &statement)) {
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

bool AccountRepository::validate(const Account& account, std::string& error)
{
    error.clear();

    if (account.accountName.empty() || isBlank(account.accountName)) {
        error = "Account name is required.";
        return false;
    }

    if (account.accountType.empty() || isBlank(account.accountType)) {
        error = "Account type is required.";
        return false;
    }

    if (account.status.empty() || isBlank(account.status)) {
        error = "Status is required.";
        return false;
    }

    return true;
}
