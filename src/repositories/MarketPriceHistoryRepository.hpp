// SPDX-License-Identifier: MIT
#pragma once

#include "models/MarketPriceHistory.hpp"

#include <string>
#include <vector>

class Database;

class MarketPriceHistoryRepository {
public:
    explicit MarketPriceHistoryRepository(Database& database);

    std::vector<MarketPriceHistoryRow> listBySymbol(const std::string& symbol, const std::string& provider, std::string& error) const;
    bool upsertMany(const std::vector<MarketPriceHistoryRow>& rows, int& rowsStored, std::string& error) const;

private:
    Database& database_;
};
