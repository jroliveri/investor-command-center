// SPDX-License-Identifier: MIT
#pragma once

#include "models/Account.hpp"
#include "models/CapitalGainAllocationRule.hpp"
#include "models/Dividend.hpp"
#include "models/Goal.hpp"
#include "models/Holding.hpp"
#include "models/ImportBatch.hpp"
#include "models/DashboardWidget.hpp"
#include "models/DashboardChartSetting.hpp"
#include "models/MarketQuote.hpp"
#include "models/PortfolioSnapshot.hpp"
#include "models/SignalRules.hpp"
#include "models/Transaction.hpp"
#include "models/WatchlistItem.hpp"
#include "models/WatchlistPriceRefresh.hpp"

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
    StockResearch,
    Reports,
    Settings
};

struct SectionInfo {
    AppSection section;
    const char* label;
};

struct DashboardPriceOverride {
    std::string symbol;
    double currentPrice = 0.0;
    std::string provider = "Yahoo Finance";
    std::string fetchedAt;
    std::string source = "Live Quote";
    bool fromCache = false;
};

struct DashboardPriceRefreshStatus {
    bool hasRun = false;
    std::string provider = "Yahoo Finance";
    std::string lastRefreshedAt;
    int refreshedSymbols = 0;
    int failedSymbols = 0;
    int cachedSymbols = 0;
    std::string warning;
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
    std::vector<PortfolioSnapshot> portfolioSnapshots;
    std::vector<ImportBatch> importBatches;
    std::vector<DashboardWidget> dashboardWidgets;
    std::vector<DashboardChartSetting> dashboardChartSettings;
    std::vector<MarketQuote> marketQuoteCache;
    std::vector<DashboardPriceOverride> dashboardPriceOverrides;
    DashboardPriceRefreshStatus dashboardPriceRefreshStatus;
    WatchlistPriceRefreshStatus watchlistPriceRefreshStatus;
<<<<<<< Updated upstream
=======
    SignalRules signalRules;
    DatabaseBackupSettings databaseBackupSettings;
>>>>>>> Stashed changes
    std::vector<CapitalGainAllocationRule> capitalGainAllocationRules;
    std::string themeKey = "dark_command_center";
    std::string statusMessage;
    bool statusIsError = false;

    void setStatus(std::string message, bool isError = false);
    void clearStatus();
};

const char* sectionTitle(AppSection section);
const std::array<SectionInfo, 11>& allSections();
