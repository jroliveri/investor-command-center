// SPDX-License-Identifier: MIT
#pragma once

#include "app/AppState.hpp"
#include "db/Database.hpp"
#include "repositories/AccountRepository.hpp"
#include "repositories/DividendRepository.hpp"
#include "repositories/GoalRepository.hpp"
#include "repositories/HoldingRepository.hpp"
#include "repositories/TransactionRepository.hpp"
#include "repositories/WatchlistRepository.hpp"
#include "ui/AccountsView.hpp"
#include "ui/DashboardView.hpp"
#include "ui/DividendsView.hpp"
#include "ui/GoalsView.hpp"
#include "ui/HoldingsView.hpp"
#include "ui/ImportCsvView.hpp"
#include "ui/SettingsView.hpp"
#include "ui/TransactionsView.hpp"
#include "ui/WatchlistView.hpp"

#include <memory>
#include <string>

class App {
public:
    bool initialize();
    void render();

    const std::string& startupError() const { return startupError_; }

private:
    void reloadData();
    void renderSidebar();
    void renderCurrentSection();
    void renderPlaceholder(const char* title, const char* note);
    void renderStatusBar();

    Database database_;
    std::unique_ptr<AccountRepository> accountRepository_;
    std::unique_ptr<HoldingRepository> holdingRepository_;
    std::unique_ptr<TransactionRepository> transactionRepository_;
    std::unique_ptr<DividendRepository> dividendRepository_;
    std::unique_ptr<GoalRepository> goalRepository_;
    std::unique_ptr<WatchlistRepository> watchlistRepository_;
    AppState state_;
    DashboardView dashboardView_;
    AccountsView accountsView_;
    HoldingsView holdingsView_;
    TransactionsView transactionsView_;
    DividendsView dividendsView_;
    GoalsView goalsView_;
    WatchlistView watchlistView_;
    ImportCsvView importCsvView_;
    SettingsView settingsView_;
    std::string startupError_;
};
