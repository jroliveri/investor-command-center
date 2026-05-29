// SPDX-License-Identifier: MIT
#include "repositories/CapitalGainAllocationRepository.hpp"

#include "db/Database.hpp"
#include "util/Date.hpp"

#include <sqlite3.h>

#include <algorithm>
#include <cctype>

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

CapitalGainAllocationRule readRule(sqlite3_stmt* statement)
{
    CapitalGainAllocationRule rule;
    rule.id = sqlite3_column_int(statement, 0);
    rule.name = textColumn(statement, 1);
    rule.percentage = sqlite3_column_double(statement, 2);
    rule.sortOrder = sqlite3_column_int(statement, 3);
    rule.isActive = sqlite3_column_int(statement, 4) != 0;
    rule.createdAt = textColumn(statement, 5);
    rule.updatedAt = textColumn(statement, 6);
    return rule;
}

}

CapitalGainAllocationRepository::CapitalGainAllocationRepository(Database& database)
    : database_(database)
{
}

std::vector<CapitalGainAllocationRule> CapitalGainAllocationRepository::listAll(std::string& error) const
{
    error.clear();
    std::vector<CapitalGainAllocationRule> rules;

    sqlite3_stmt* statement = nullptr;
    if (!database_.prepare(
            "SELECT id, name, percentage, sort_order, is_active, created_at, updated_at "
            "FROM capital_gain_allocation_rules ORDER BY sort_order ASC, id ASC;",
            &statement)) {
        error = database_.lastError();
        return rules;
    }

    while (sqlite3_step(statement) == SQLITE_ROW) {
        rules.push_back(readRule(statement));
    }

    sqlite3_finalize(statement);
    return rules;
}

bool CapitalGainAllocationRepository::ensureDefaults(std::string& error) const
{
    error.clear();

    std::vector<CapitalGainAllocationRule> rules = listAll(error);
    if (!error.empty()) {
        return false;
    }

    if (!rules.empty()) {
        return true;
    }

    rules = {
        CapitalGainAllocationRule { 0, "Checking", 0.0, 1, true },
        CapitalGainAllocationRule { 0, "Savings", 0.0, 2, true },
        CapitalGainAllocationRule { 0, "Reinvest", 0.0, 3, true },
    };

    return saveAll(rules, error);
}

bool CapitalGainAllocationRepository::create(CapitalGainAllocationRule& rule, std::string& error) const
{
    error.clear();
    if (!validate(rule, error)) {
        return false;
    }

    return insertRule(rule, error);
}

bool CapitalGainAllocationRepository::update(const CapitalGainAllocationRule& rule, std::string& error) const
{
    error.clear();
    if (rule.id <= 0) {
        error = "Cannot update an allocation category without a database id.";
        return false;
    }

    if (!validate(rule, error)) {
        return false;
    }

    const std::string timestamp = Date::nowIso8601();

    sqlite3_stmt* statement = nullptr;
    if (!database_.prepare(
            "UPDATE capital_gain_allocation_rules "
            "SET name = ?, percentage = ?, sort_order = ?, is_active = ?, updated_at = ? "
            "WHERE id = ?;",
            &statement)) {
        error = database_.lastError();
        return false;
    }

    sqlite3_bind_text(statement, 1, rule.name.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_double(statement, 2, rule.percentage);
    sqlite3_bind_int(statement, 3, rule.sortOrder);
    sqlite3_bind_int(statement, 4, rule.isActive ? 1 : 0);
    sqlite3_bind_text(statement, 5, timestamp.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(statement, 6, rule.id);

    if (sqlite3_step(statement) != SQLITE_DONE) {
        error = sqlite3_errmsg(database_.handle());
        sqlite3_finalize(statement);
        return false;
    }

    sqlite3_finalize(statement);
    return true;
}

bool CapitalGainAllocationRepository::remove(int id, std::string& error) const
{
    error.clear();

    sqlite3_stmt* statement = nullptr;
    if (!database_.prepare("DELETE FROM capital_gain_allocation_rules WHERE id = ?;", &statement)) {
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

bool CapitalGainAllocationRepository::saveAll(std::vector<CapitalGainAllocationRule> rules, std::string& error) const
{
    error.clear();

    for (std::size_t index = 0; index < rules.size(); ++index) {
        rules[index].sortOrder = static_cast<int>(index) + 1;
        if (!validate(rules[index], error)) {
            return false;
        }
    }

    if (!database_.execute("BEGIN TRANSACTION;")) {
        error = database_.lastError();
        return false;
    }

    for (CapitalGainAllocationRule& rule : rules) {
        const bool saved = rule.id > 0 ? update(rule, error) : insertRule(rule, error);
        if (!saved) {
            database_.execute("ROLLBACK;");
            return false;
        }
    }

    if (!database_.execute("COMMIT;")) {
        error = database_.lastError();
        return false;
    }

    return true;
}

bool CapitalGainAllocationRepository::validate(const CapitalGainAllocationRule& rule, std::string& error)
{
    error.clear();

    if (rule.name.empty() || isBlank(rule.name)) {
        error = "Allocation category name is required.";
        return false;
    }

    if (rule.percentage < 0.0) {
        error = "Allocation percentage cannot be negative.";
        return false;
    }

    return true;
}

bool CapitalGainAllocationRepository::insertRule(CapitalGainAllocationRule& rule, std::string& error) const
{
    const std::string timestamp = Date::nowIso8601();
    rule.createdAt = timestamp;
    rule.updatedAt = timestamp;

    sqlite3_stmt* statement = nullptr;
    if (!database_.prepare(
            "INSERT INTO capital_gain_allocation_rules(name, percentage, sort_order, is_active, created_at, updated_at) "
            "VALUES (?, ?, ?, ?, ?, ?);",
            &statement)) {
        error = database_.lastError();
        return false;
    }

    sqlite3_bind_text(statement, 1, rule.name.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_double(statement, 2, rule.percentage);
    sqlite3_bind_int(statement, 3, rule.sortOrder);
    sqlite3_bind_int(statement, 4, rule.isActive ? 1 : 0);
    sqlite3_bind_text(statement, 5, rule.createdAt.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(statement, 6, rule.updatedAt.c_str(), -1, SQLITE_TRANSIENT);

    if (sqlite3_step(statement) != SQLITE_DONE) {
        error = sqlite3_errmsg(database_.handle());
        sqlite3_finalize(statement);
        return false;
    }

    sqlite3_finalize(statement);
    rule.id = static_cast<int>(database_.lastInsertRowId());
    return true;
}
