// SPDX-License-Identifier: MIT
#pragma once

#include "models/MarketQuote.hpp"

#include <optional>
#include <string>

class Database;

class MarketQuoteCacheRepository {
public:
    explicit MarketQuoteCacheRepository(Database& database);

    std::optional<MarketQuote> findBySymbol(const std::string& symbol, std::string& error) const;
    bool upsert(const MarketQuote& quote, std::string& error) const;

private:
    Database& database_;
};
