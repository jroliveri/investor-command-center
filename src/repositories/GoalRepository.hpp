// SPDX-License-Identifier: MIT
#pragma once

#include "models/Goal.hpp"

#include <string>
#include <vector>

class Database;

class GoalRepository {
public:
    explicit GoalRepository(Database& database);

    std::vector<Goal> listAll(std::string& error) const;
    bool create(Goal& goal, std::string& error) const;
    bool update(const Goal& goal, std::string& error) const;
    bool remove(int id, std::string& error) const;

    static bool validate(const Goal& goal, std::string& error);

private:
    Database& database_;
};
