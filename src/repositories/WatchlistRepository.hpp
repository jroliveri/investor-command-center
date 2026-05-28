// SPDX-License-Identifier: MIT
#pragma once

#include "models/WatchlistItem.hpp"

#include <string>
#include <vector>

class Database;

class WatchlistRepository {
public:
    explicit WatchlistRepository(Database& database);

    std::vector<WatchlistItem> listAll(std::string& error) const;
    bool create(WatchlistItem& item, std::string& error) const;
    bool update(const WatchlistItem& item, std::string& error) const;
    bool remove(int id, std::string& error) const;

    static bool validate(const WatchlistItem& item, std::string& error);

private:
    Database& database_;
};
