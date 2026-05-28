// SPDX-License-Identifier: MIT
#pragma once

#include "models/Dividend.hpp"

#include <string>
#include <vector>

class Database;

class DividendRepository {
public:
    explicit DividendRepository(Database& database);

    std::vector<Dividend> listAll(std::string& error) const;
    bool create(Dividend& dividend, std::string& error) const;
    bool update(const Dividend& dividend, std::string& error) const;
    bool remove(int id, std::string& error) const;

    static bool validate(const Dividend& dividend, std::string& error);

private:
    Database& database_;
};
