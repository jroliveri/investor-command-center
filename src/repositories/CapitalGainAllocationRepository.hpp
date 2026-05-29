// SPDX-License-Identifier: MIT
#pragma once

#include "models/CapitalGainAllocationRule.hpp"

#include <string>
#include <vector>

class Database;

class CapitalGainAllocationRepository {
public:
    explicit CapitalGainAllocationRepository(Database& database);

    std::vector<CapitalGainAllocationRule> listAll(std::string& error) const;
    bool ensureDefaults(std::string& error) const;
    bool create(CapitalGainAllocationRule& rule, std::string& error) const;
    bool update(const CapitalGainAllocationRule& rule, std::string& error) const;
    bool remove(int id, std::string& error) const;
    bool saveAll(std::vector<CapitalGainAllocationRule> rules, std::string& error) const;

    static bool validate(const CapitalGainAllocationRule& rule, std::string& error);

private:
    bool insertRule(CapitalGainAllocationRule& rule, std::string& error) const;

    Database& database_;
};
