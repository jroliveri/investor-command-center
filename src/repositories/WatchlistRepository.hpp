// SPDX-License-Identifier: MIT
#pragma once

#include "models/Watchlist.hpp"
#include "models/WatchlistItem.hpp"

#include <string>
#include <vector>

class Database;

class WatchlistRepository {
public:
    explicit WatchlistRepository(Database& database);

    std::vector<Watchlist> listWatchlists(bool includeInactive, std::string& error) const;
    bool createWatchlist(Watchlist& watchlist, std::string& error) const;
    bool updateWatchlist(const Watchlist& watchlist, std::string& error) const;
    bool deactivateWatchlist(int id, std::string& error) const;
    bool saveWatchlistOrder(const std::vector<Watchlist>& watchlists, std::string& error) const;
    int itemCountForWatchlist(int watchlistId, std::string& error) const;

    std::vector<WatchlistItem> listAll(std::string& error) const;
    std::vector<WatchlistItem> listByWatchlist(int watchlistId, std::string& error) const;
    bool create(WatchlistItem& item, std::string& error) const;
    bool update(const WatchlistItem& item, std::string& error) const;
    bool remove(int id, std::string& error) const;

    bool defaultWatchlistId(int& watchlistId, std::string& error) const;
    static bool validateWatchlist(const Watchlist& watchlist, std::string& error);
    static bool validate(const WatchlistItem& item, std::string& error);

private:
    Database& database_;
};
