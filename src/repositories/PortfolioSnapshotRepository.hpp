// SPDX-License-Identifier: MIT
#pragma once

#include "models/PortfolioSnapshot.hpp"

#include <string>
#include <vector>

class Database;

class PortfolioSnapshotRepository {
public:
    explicit PortfolioSnapshotRepository(Database& database);

    std::vector<PortfolioSnapshot> listAll(std::string& error) const;
    bool create(PortfolioSnapshot& snapshot, std::string& error) const;
    bool update(const PortfolioSnapshot& snapshot, std::string& error) const;
    bool upsertForDate(PortfolioSnapshot& snapshot, std::string& error) const;
    bool upsertForAccountDateSource(PortfolioSnapshot& snapshot, std::string& error) const;
    bool existsForAccountDateSource(int accountId, const std::string& snapshotDate, const std::string& source, std::string& error) const;

    static bool validate(const PortfolioSnapshot& snapshot, std::string& error);

private:
    bool findByDate(const std::string& snapshotDate, PortfolioSnapshot& snapshot, std::string& error) const;
    bool findByAccountDateSource(int accountId, const std::string& snapshotDate, const std::string& source, PortfolioSnapshot& snapshot, std::string& error) const;

    Database& database_;
};
