// SPDX-License-Identifier: MIT
#pragma once

#include "models/Account.hpp"
#include "models/Dividend.hpp"
#include "models/Goal.hpp"
#include "models/Holding.hpp"
#include "models/Transaction.hpp"
#include "models/WatchlistItem.hpp"

#include <array>
#include <string>
#include <vector>

enum class AppSection {
    Dashboard,
    Accounts,
    Holdings,
    Transactions,
    Dividends,
    Goals,
    Watchlist,
    ImportCsv,
    Reports,
    Settings
};

struct SectionInfo {
    AppSection section;
    const char* label;
};

class AppState {
public:
    AppSection currentSection = AppSection::Dashboard;
    std::vector<Account> accounts;
    std::vector<Holding> holdings;
    std::vector<Transaction> transactions;
    std::vector<Dividend> dividends;
    std::vector<Goal> goals;
    std::vector<WatchlistItem> watchlist;
    std::string statusMessage;
    bool statusIsError = false;

    void setStatus(std::string message, bool isError = false);
    void clearStatus();
};

const char* sectionTitle(AppSection section);
const std::array<SectionInfo, 10>& allSections();
