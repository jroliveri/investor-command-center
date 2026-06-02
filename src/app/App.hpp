// SPDX-License-Identifier: MIT
#pragma once

#include "app/AppState.hpp"
#include "db/Database.hpp"
#include "repositories/AccountRepository.hpp"
#include "repositories/AppSettingsRepository.hpp"
#include "repositories/CapitalGainAllocationRepository.hpp"
#include "repositories/DashboardChartSettingsRepository.hpp"
#include "repositories/DashboardLayoutRepository.hpp"
#include "repositories/DividendRepository.hpp"
#include "repositories/GoalRepository.hpp"
#include "repositories/HoldingRepository.hpp"
#include "repositories/ImportBatchRepository.hpp"
#include "repositories/MarketQuoteCacheRepository.hpp"
#include "repositories/PortfolioSnapshotRepository.hpp"
#include "repositories/TransactionRepository.hpp"
#include "repositories/WatchlistRepository.hpp"
#include "services/CsvImportService.hpp"
#include "services/MarketDataService.hpp"
#include "services/TransactionService.hpp"
#include "services/YahooFinanceProvider.hpp"
#include "ui/AccountsView.hpp"
#include "ui/DashboardView.hpp"
#include "ui/DividendsView.hpp"
#include "ui/GoalsView.hpp"
#include "ui/HoldingsView.hpp"
#include "ui/ImportCsvView.hpp"
#include "ui/SettingsView.hpp"
#include "ui/StockResearchView.hpp"
#include "ui/TransactionsView.hpp"
#include "ui/WatchlistView.hpp"

#include <memory>
#include <string>

class App {
public:
    bool initialize();
    void render();

    const std::string& startupError() const { return startupError_; }
    bool shouldExit() const { return shouldExit_; }

private:
    void reloadData();
    void navigateTo(AppSection section);
    bool menuSectionItem(AppSection section, const char* label);
    void requestManualSnapshot();
    void createManualSnapshot(bool replaceExisting);
    void refreshSelectedResearchSymbol();
    void refreshDashboardPrices();
    void refreshWatchlistPrices();
    void renderTopMenuBar();
    void renderAccountColumn();
    void renderAccountInfoPanel();
    void renderAccountsPanel();
    void renderWatchlistPanel(const char* title, int startIndex);
    void renderSidebarFooter();
    void renderAppPopups();
    void renderCurrentSection();
    void renderPlaceholder(const char* title, const char* note);
    void renderStatusBar();

    Database database_;
    std::unique_ptr<AccountRepository> accountRepository_;
    std::unique_ptr<HoldingRepository> holdingRepository_;
    std::unique_ptr<ImportBatchRepository> importBatchRepository_;
    std::unique_ptr<TransactionRepository> transactionRepository_;
    std::unique_ptr<TransactionService> transactionService_;
    std::unique_ptr<CsvImportService> csvImportService_;
    std::unique_ptr<DividendRepository> dividendRepository_;
    std::unique_ptr<GoalRepository> goalRepository_;
    std::unique_ptr<WatchlistRepository> watchlistRepository_;
    std::unique_ptr<MarketQuoteCacheRepository> marketQuoteCacheRepository_;
    std::unique_ptr<PortfolioSnapshotRepository> portfolioSnapshotRepository_;
    std::unique_ptr<DashboardLayoutRepository> dashboardLayoutRepository_;
    std::unique_ptr<DashboardChartSettingsRepository> dashboardChartSettingsRepository_;
    std::unique_ptr<AppSettingsRepository> appSettingsRepository_;
    std::unique_ptr<CapitalGainAllocationRepository> capitalGainAllocationRepository_;
    std::unique_ptr<YahooFinanceProvider> yahooFinanceProvider_;
    std::unique_ptr<MarketDataService> marketDataService_;
    AppState state_;
    DashboardView dashboardView_;
    AccountsView accountsView_;
    HoldingsView holdingsView_;
    TransactionsView transactionsView_;
    DividendsView dividendsView_;
    GoalsView goalsView_;
    WatchlistView watchlistView_;
    ImportCsvView importCsvView_;
    StockResearchView stockResearchView_;
    SettingsView settingsView_;
    std::string startupError_;
    bool shouldExit_ = false;
    bool showAboutPopup_ = false;
    bool showPrivacyPopup_ = false;
    bool showResearchDisclaimerPopup_ = false;
    bool showManualSnapshotReplacePopup_ = false;
};
