// SPDX-License-Identifier: MIT
#pragma once

#include "models/Holding.hpp"

#include <string>
#include <vector>

class Database;

class HoldingRepository {
public:
    explicit HoldingRepository(Database& database);

    std::vector<Holding> listAll(std::string& error) const;
    bool create(Holding& holding, std::string& error) const;
    bool update(const Holding& holding, std::string& error) const;
    bool remove(int id, std::string& error) const;
    bool softDelete(int id, std::string& error) const;

    static bool validate(const Holding& holding, std::string& error);

private:
    Database& database_;
};
