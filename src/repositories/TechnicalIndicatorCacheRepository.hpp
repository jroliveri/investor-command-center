// SPDX-License-Identifier: MIT
#pragma once

#include "models/TechnicalIndicatorSnapshot.hpp"

#include <optional>
#include <string>

class Database;

class TechnicalIndicatorCacheRepository {
public:
    explicit TechnicalIndicatorCacheRepository(Database& database);

    std::optional<TechnicalIndicatorSnapshot> findLatestBySymbol(const std::string& symbol, const std::string& provider, std::string& error) const;
    bool upsert(const TechnicalIndicatorSnapshot& snapshot, std::string& error) const;

private:
    Database& database_;
};
