// SPDX-License-Identifier: MIT
#pragma once

#include <string>

struct WatchlistPriceRefreshStatus {
    bool hasRun = false;
    std::string provider = "Yahoo Finance";
    std::string lastRefreshedAt;
    int refreshedSymbols = 0;
    int failedSymbols = 0;
    int cachedSymbols = 0;
    std::string warning;
};
