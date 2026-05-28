// SPDX-License-Identifier: MIT
#include "repositories/GoalRepository.hpp"

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

GoalRepository::GoalRepository(Database& database)
    : database_(database)
{
}

std::vector<Goal> GoalRepository::listAll(std::string& error) const
{
    error.clear();
    std::vector<Goal> goals;

    sqlite3_stmt* statement = nullptr;
    if (!database_.prepare(
            "SELECT id, goal_name, target_amount, current_amount, target_date, category, notes, created_at, updated_at "
            "FROM goals ORDER BY target_date IS NULL, target_date, goal_name COLLATE NOCASE;",
            &statement)) {
        error = database_.lastError();
        return goals;
    }

    while (sqlite3_step(statement) == SQLITE_ROW) {
        Goal goal;
        goal.id = sqlite3_column_int(statement, 0);
        goal.goalName = textColumn(statement, 1);
        goal.targetAmount = sqlite3_column_double(statement, 2);
        goal.currentAmount = sqlite3_column_double(statement, 3);
        goal.targetDate = textColumn(statement, 4);
        goal.category = textColumn(statement, 5);
        goal.notes = textColumn(statement, 6);
        goal.createdAt = textColumn(statement, 7);
        goal.updatedAt = textColumn(statement, 8);
        goals.push_back(goal);
    }

    sqlite3_finalize(statement);
    return goals;
}

bool GoalRepository::create(Goal& goal, std::string& error) const
{
    error.clear();
    if (!validate(goal, error)) {
        return false;
    }

    const std::string timestamp = Date::nowIso8601();
    goal.createdAt = timestamp;
    goal.updatedAt = timestamp;

    sqlite3_stmt* statement = nullptr;
    if (!database_.prepare(
            "INSERT INTO goals(goal_name, target_amount, current_amount, target_date, category, notes, created_at, updated_at) "
            "VALUES (?, ?, ?, ?, ?, ?, ?, ?);",
            &statement)) {
        error = database_.lastError();
        return false;
    }

    sqlite3_bind_text(statement, 1, goal.goalName.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_double(statement, 2, goal.targetAmount);
    sqlite3_bind_double(statement, 3, goal.currentAmount);
    sqlite3_bind_text(statement, 4, goal.targetDate.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(statement, 5, goal.category.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(statement, 6, goal.notes.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(statement, 7, goal.createdAt.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(statement, 8, goal.updatedAt.c_str(), -1, SQLITE_TRANSIENT);

    if (sqlite3_step(statement) != SQLITE_DONE) {
        error = sqlite3_errmsg(database_.handle());
        sqlite3_finalize(statement);
        return false;
    }

    sqlite3_finalize(statement);
    goal.id = static_cast<int>(database_.lastInsertRowId());
    return true;
}

bool GoalRepository::update(const Goal& goal, std::string& error) const
{
    error.clear();
    if (goal.id <= 0) {
        error = "Cannot update a goal without a database id.";
        return false;
    }

    if (!validate(goal, error)) {
        return false;
    }

    const std::string timestamp = Date::nowIso8601();

    sqlite3_stmt* statement = nullptr;
    if (!database_.prepare(
            "UPDATE goals SET goal_name = ?, target_amount = ?, current_amount = ?, target_date = ?, "
            "category = ?, notes = ?, updated_at = ? "
            "WHERE id = ?;",
            &statement)) {
        error = database_.lastError();
        return false;
    }

    sqlite3_bind_text(statement, 1, goal.goalName.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_double(statement, 2, goal.targetAmount);
    sqlite3_bind_double(statement, 3, goal.currentAmount);
    sqlite3_bind_text(statement, 4, goal.targetDate.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(statement, 5, goal.category.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(statement, 6, goal.notes.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(statement, 7, timestamp.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(statement, 8, goal.id);

    if (sqlite3_step(statement) != SQLITE_DONE) {
        error = sqlite3_errmsg(database_.handle());
        sqlite3_finalize(statement);
        return false;
    }

    sqlite3_finalize(statement);
    return true;
}

bool GoalRepository::remove(int id, std::string& error) const
{
    error.clear();

    sqlite3_stmt* statement = nullptr;
    if (!database_.prepare("DELETE FROM goals WHERE id = ?;", &statement)) {
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

bool GoalRepository::validate(const Goal& goal, std::string& error)
{
    error.clear();

    if (goal.goalName.empty() || isBlank(goal.goalName)) {
        error = "Goal name is required.";
        return false;
    }

    if (goal.targetAmount < 0.0 || goal.currentAmount < 0.0) {
        error = "Goal amounts cannot be negative.";
        return false;
    }

    if (!goal.targetDate.empty() && !Date::isIsoDate(goal.targetDate)) {
        error = "Target date must use YYYY-MM-DD when provided.";
        return false;
    }

    return true;
}
