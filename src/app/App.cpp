// SPDX-License-Identifier: MIT
#include "app/App.hpp"

#include "db/Migrations.hpp"
#include "services/DashboardService.hpp"
#include "services/DatabaseBackupService.hpp"
#include "services/PortfolioCalculator.hpp"
#include "services/WatchlistSignalService.hpp"
#include "ui/UiTheme.hpp"
#include "util/Date.hpp"
#include "util/Money.hpp"

#include <imgui.h>

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <set>
#include <string>
#include <system_error>
#include <vector>

namespace {

constexpr const char* DatabasePath = "data/investor_command_center.db";
constexpr const char* AppVersion = "0.3.0";
constexpr float AccountColumnWidth = 340.0f;
constexpr const char* ThemeSettingKey = "theme";
constexpr const char* BackupFolderSettingKey = "database.backup_folder";
constexpr const char* BackupReminderEnabledSettingKey = "database.backup_reminder_enabled";
constexpr const char* BackupReminderFrequencySettingKey = "database.backup_reminder_frequency";
constexpr const char* LastBackupAtSettingKey = "database.last_backup_at";
constexpr const char* DatabasePathSettingKey = "database.path";

void compactMetric(const char* label, const std::string& value, ImVec4 color)
{
    ImGui::TextColored(UiTheme::MutedText, "%s", label);
    ImGui::SameLine(170.0f);
    ImGui::TextColored(color, "%s", value.c_str());
}

std::string normalizeSymbol(std::string symbol)
{
    symbol.erase(symbol.begin(), std::find_if(symbol.begin(), symbol.end(), [](unsigned char character) {
        return std::isspace(character) == 0;
    }));
    symbol.erase(std::find_if(symbol.rbegin(), symbol.rend(), [](unsigned char character) {
        return std::isspace(character) == 0;
    }).base(), symbol.end());
    std::transform(symbol.begin(), symbol.end(), symbol.begin(), [](unsigned char character) {
        return static_cast<char>(std::toupper(character));
    });
    return symbol;
}

bool isActiveAccount(const Account& account)
{
    return account.status.empty() || account.status == "Active";
}

bool isActiveHolding(const Holding& holding)
{
    return holding.status.empty() || holding.status == "Active";
}

const Account* accountForHolding(const std::vector<Account>& accounts, int accountId)
{
    for (const Account& account : accounts) {
        if (account.id == accountId) {
            return &account;
        }
    }
    return nullptr;
}

std::filesystem::path absolutePath(const std::string& path)
{
    std::error_code error;
    const std::filesystem::path absolute = std::filesystem::absolute(std::filesystem::path(path), error);
    return error ? std::filesystem::path(path) : absolute.lexically_normal();
}

std::string absolutePathString(const std::string& path)
{
    return absolutePath(path).string();
}

bool pathsEquivalent(const std::filesystem::path& left, const std::filesystem::path& right)
{
    std::error_code error;
    if (std::filesystem::equivalent(left, right, error)) {
        return true;
    }
    return absolutePath(left.string()).lexically_normal() == absolutePath(right.string()).lexically_normal();
}

bool isPathInside(const std::filesystem::path& child, const std::filesystem::path& parent)
{
    const std::filesystem::path normalizedChild = absolutePath(child.string()).lexically_normal();
    const std::filesystem::path normalizedParent = absolutePath(parent.string()).lexically_normal();

    auto childIterator = normalizedChild.begin();
    auto parentIterator = normalizedParent.begin();
    for (; parentIterator != normalizedParent.end(); ++parentIterator, ++childIterator) {
        if (childIterator == normalizedChild.end() || *childIterator != *parentIterator) {
            return false;
        }
    }

    return true;
}

bool isPathInsideRepository(const std::string& path)
{
    std::error_code error;
    const std::filesystem::path repositoryRoot = std::filesystem::current_path(error);
    if (error) {
        return false;
    }
    return isPathInside(absolutePath(path), repositoryRoot);
}

bool isRepositoryRoot(const std::string& path)
{
    std::error_code error;
    const std::filesystem::path repositoryRoot = std::filesystem::current_path(error);
    if (error) {
        return false;
    }
    return pathsEquivalent(absolutePath(path), repositoryRoot);
}

ImVec4 sidebarSignalColor(const WatchlistItem& item)
{
    if (WatchlistSignalService::hasSignalWarning(item)) {
        return UiTheme::Amber;
    }

    const std::string status = WatchlistSignalService::calculateSignalStatus(item.currentPrice, item.buySignalPrice, item.sellSignalPrice);
    if (status == "Buy") {
        return UiTheme::Gain;
    }
    if (status == "Sell") {
        return UiTheme::Loss;
    }
    return UiTheme::TextSecondary;
}

int watchlistPrioritySortRank(const std::string& priority)
{
    if (priority == "High") {
        return 0;
    }
    if (priority == "Medium") {
        return 1;
    }
    if (priority == "Low") {
        return 2;
    }
    return 3;
}

void sortWatchlistItemsBySignal(std::vector<WatchlistItem>& items)
{
    std::stable_sort(items.begin(), items.end(), [](const WatchlistItem& left, const WatchlistItem& right) {
        const int leftSignalRank = WatchlistSignalService::signalSortRank(left);
        const int rightSignalRank = WatchlistSignalService::signalSortRank(right);
        if (leftSignalRank != rightSignalRank) {
            return leftSignalRank < rightSignalRank;
        }

        const int leftPriorityRank = watchlistPrioritySortRank(left.priority);
        const int rightPriorityRank = watchlistPrioritySortRank(right.priority);
        if (leftPriorityRank != rightPriorityRank) {
            return leftPriorityRank < rightPriorityRank;
        }

        return left.ticker < right.ticker;
    });
}

const Watchlist* sidebarWatchlistForSlot(const AppState& state, int sidebarSlot)
{
    for (const Watchlist& watchlist : state.watchlists) {
        if (watchlist.isActive && watchlist.showInSidebar && watchlist.sidebarSlot == sidebarSlot) {
            return &watchlist;
        }
    }
    return nullptr;
}

std::vector<WatchlistItem> sidebarWatchlistItems(const AppState& state, int watchlistId)
{
    std::vector<WatchlistItem> items;
    for (const WatchlistItem& item : state.watchlist) {
        if (item.watchlistId == watchlistId) {
            items.push_back(item);
        }
    }
    sortWatchlistItemsBySignal(items);
    return items;
}

}

bool App::initialize()
{
    if (!openStartupDatabase()) {
        return false;
    }

    accountRepository_ = std::make_unique<AccountRepository>(database_);
    holdingRepository_ = std::make_unique<HoldingRepository>(database_);
    importBatchRepository_ = std::make_unique<ImportBatchRepository>(database_);
    transactionRepository_ = std::make_unique<TransactionRepository>(database_);
    transactionService_ = std::make_unique<TransactionService>(database_, *transactionRepository_, *holdingRepository_);
    dividendRepository_ = std::make_unique<DividendRepository>(database_);
    goalRepository_ = std::make_unique<GoalRepository>(database_);
    watchlistRepository_ = std::make_unique<WatchlistRepository>(database_);
    marketQuoteCacheRepository_ = std::make_unique<MarketQuoteCacheRepository>(database_);
    portfolioSnapshotRepository_ = std::make_unique<PortfolioSnapshotRepository>(database_);
    dashboardLayoutRepository_ = std::make_unique<DashboardLayoutRepository>(database_);
    dashboardChartSettingsRepository_ = std::make_unique<DashboardChartSettingsRepository>(database_);
    appSettingsRepository_ = std::make_unique<AppSettingsRepository>(database_);
    capitalGainAllocationRepository_ = std::make_unique<CapitalGainAllocationRepository>(database_);
    csvImportService_ = std::make_unique<CsvImportService>(database_, *holdingRepository_, *importBatchRepository_, *portfolioSnapshotRepository_);
    yahooFinanceProvider_ = std::make_unique<YahooFinanceProvider>();
    marketDataService_ = std::make_unique<MarketDataService>(*yahooFinanceProvider_, *marketQuoteCacheRepository_);

    std::string layoutError;
    if (!dashboardLayoutRepository_->ensureDefaults(layoutError)) {
        startupError_ = layoutError;
        return false;
    }

    std::string chartSettingsError;
    if (!dashboardChartSettingsRepository_->ensureDefaults(chartSettingsError)) {
        startupError_ = chartSettingsError;
        return false;
    }

    std::string allocationError;
    if (!capitalGainAllocationRepository_->ensureDefaults(allocationError)) {
        startupError_ = allocationError;
        return false;
    }

    reloadData();
    if (!startupDatabaseWarning_.empty()) {
        state_.setStatus(startupDatabaseWarning_, true);
    }

    return true;
}

bool App::openStartupDatabase()
{
    std::string error;
    if (!openDatabaseAtPath(DatabasePath, error)) {
        startupError_ = error;
        return false;
    }

    AppSettingsRepository bootstrapSettings(database_);
    std::string settingsError;
    const std::string configuredPath = bootstrapSettings.getString(DatabasePathSettingKey, "", settingsError);
    if (!settingsError.empty()) {
        startupDatabaseWarning_ = "Could not read configured database path. Using the default local database.";
        return true;
    }

    if (configuredPath.empty()) {
        return true;
    }

    const std::filesystem::path configuredAbsolutePath = absolutePath(configuredPath);
    const std::filesystem::path activeAbsolutePath = absolutePath(activeDatabasePath_);
    if (pathsEquivalent(configuredAbsolutePath, activeAbsolutePath)) {
        activeDatabasePath_ = activeAbsolutePath.string();
        return true;
    }

    std::error_code filesystemError;
    if (!std::filesystem::exists(configuredAbsolutePath, filesystemError) || filesystemError) {
        startupDatabaseWarning_ = "Configured database path was not found. Using the default local database.";
        return true;
    }

    database_.close();
    if (openDatabaseAtPath(configuredAbsolutePath.string(), error)) {
        return true;
    }

    startupDatabaseWarning_ = "Configured database could not be opened. Using the default local database. " + error;
    database_.close();
    if (!openDatabaseAtPath(DatabasePath, error)) {
        startupError_ = error;
        return false;
    }
    return true;
}

bool App::openDatabaseAtPath(const std::string& databasePath, std::string& error)
{
    error.clear();
    if (!database_.open(databasePath)) {
        error = database_.lastError();
        return false;
    }

    std::string migrationError;
    if (!Migrations::run(database_, migrationError)) {
        error = migrationError;
        return false;
    }

    activeDatabasePath_ = absolutePathString(databasePath);
    return true;
}

bool App::writeDatabasePathSettingToFile(const std::string& databasePath, const std::string& configuredPath, std::string& error)
{
    error.clear();

    Database targetDatabase;
    if (!targetDatabase.open(databasePath)) {
        error = targetDatabase.lastError();
        return false;
    }

    std::string migrationError;
    if (!Migrations::run(targetDatabase, migrationError)) {
        error = migrationError;
        return false;
    }

    AppSettingsRepository targetSettings(targetDatabase);
    return targetSettings.setString(DatabasePathSettingKey, configuredPath, error);
}

void App::render()
{
    UiTheme::apply(UiTheme::themeFromKey(state_.themeKey));
    renderTopMenuBar();

    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->WorkPos);
    ImGui::SetNextWindowSize(viewport->WorkSize);

    const ImGuiWindowFlags windowFlags =
        ImGuiWindowFlags_NoTitleBar |
        ImGuiWindowFlags_NoCollapse |
        ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoBringToFrontOnFocus |
        ImGuiWindowFlags_NoNavFocus;

    ImGui::Begin("Investor Command Center", nullptr, windowFlags);

    renderAccountColumn();
    ImGui::SameLine();

    ImGui::BeginChild("Content", ImVec2(0.0f, 0.0f), false);
    ImGui::Text("%s", sectionTitle(state_.currentSection));
    ImGui::SameLine();
    ImGui::TextColored(UiTheme::MutedText, "Investor Command Center - top menu navigation - local personal investing tracker");
    ImGui::Separator();
    ImGui::Spacing();

    const float statusHeight = state_.statusMessage.empty() ? 0.0f : 38.0f;
    ImGui::BeginChild("PageContent", ImVec2(0.0f, -statusHeight), false);
    renderCurrentSection();
    ImGui::EndChild();

    renderStatusBar();

    ImGui::EndChild();
    renderAppPopups();
    ImGui::End();
}

void App::reloadData()
{
    std::string error;
    state_.accounts = accountRepository_->listAll(error);
    if (!error.empty()) {
        state_.setStatus("Could not load accounts: " + error, true);
        return;
    }

    state_.holdings = holdingRepository_->listAll(error);
    if (!error.empty()) {
        state_.setStatus("Could not load holdings: " + error, true);
        return;
    }

    state_.transactions = transactionRepository_->listAll(error);
    if (!error.empty()) {
        state_.setStatus("Could not load transactions: " + error, true);
        return;
    }

    state_.dividends = dividendRepository_->listAll(error);
    if (!error.empty()) {
        state_.setStatus("Could not load dividends: " + error, true);
        return;
    }

    state_.goals = goalRepository_->listAll(error);
    if (!error.empty()) {
        state_.setStatus("Could not load goals: " + error, true);
        return;
    }

    state_.watchlists = watchlistRepository_->listWatchlists(true, error);
    if (!error.empty()) {
        state_.setStatus("Could not load watchlists: " + error, true);
        return;
    }

    state_.watchlist = watchlistRepository_->listAll(error);
    if (!error.empty()) {
        state_.setStatus("Could not load watchlist: " + error, true);
        return;
    }

    state_.portfolioSnapshots = portfolioSnapshotRepository_->listAll(error);
    if (!error.empty()) {
        state_.setStatus("Could not load portfolio snapshots: " + error, true);
        return;
    }

    state_.importBatches = importBatchRepository_->listAll(error);
    if (!error.empty()) {
        state_.setStatus("Could not load import batches: " + error, true);
        return;
    }

    state_.dashboardWidgets = dashboardLayoutRepository_->listAll(error);
    if (!error.empty()) {
        state_.setStatus("Could not load dashboard layout: " + error, true);
        return;
    }

    state_.dashboardChartSettings = dashboardChartSettingsRepository_->listAll(error);
    if (!error.empty()) {
        state_.setStatus("Could not load dashboard chart settings: " + error, true);
        return;
    }

    state_.capitalGainAllocationRules = capitalGainAllocationRepository_->listAll(error);
    if (!error.empty()) {
        state_.setStatus("Could not load capital gains allocation rules: " + error, true);
        return;
    }

    state_.themeKey = appSettingsRepository_->getString(ThemeSettingKey, "dark_command_center", error);
    if (!error.empty()) {
        state_.setStatus("Could not load app settings: " + error, true);
    }

    state_.databaseBackupSettings.backupFolder = appSettingsRepository_->getString(BackupFolderSettingKey, "", error);
    if (!error.empty()) {
        state_.setStatus("Could not load backup folder setting: " + error, true);
        error.clear();
    }

    state_.databaseBackupSettings.reminderEnabled = appSettingsRepository_->getString(BackupReminderEnabledSettingKey, "0", error) == "1";
    if (!error.empty()) {
        state_.setStatus("Could not load backup reminder setting: " + error, true);
        error.clear();
    }

    state_.databaseBackupSettings.reminderFrequency = DatabaseBackupService::normalizeFrequency(
        appSettingsRepository_->getString(BackupReminderFrequencySettingKey, "Monthly", error));
    if (!error.empty()) {
        state_.setStatus("Could not load backup reminder frequency: " + error, true);
        error.clear();
    }

    state_.databaseBackupSettings.lastBackupAt = appSettingsRepository_->getString(LastBackupAtSettingKey, "", error);
    if (!error.empty()) {
        state_.setStatus("Could not load last backup timestamp: " + error, true);
        error.clear();
    }
}

void App::navigateTo(AppSection section)
{
    state_.currentSection = section;
    state_.clearStatus();
}

bool App::menuSectionItem(AppSection section, const char* label)
{
    const bool selected = state_.currentSection == section;
    if (ImGui::MenuItem(label, nullptr, selected)) {
        navigateTo(section);
        return true;
    }

    return false;
}

void App::requestManualSnapshot()
{
    navigateTo(AppSection::Dashboard);
    if (DashboardService::hasSnapshotForDate(state_, Date::todayIso8601())) {
        showManualSnapshotReplacePopup_ = true;
        return;
    }

    createManualSnapshot(false);
}

void App::createManualSnapshot(bool replaceExisting)
{
    const std::string today = Date::todayIso8601();
    if (!replaceExisting && DashboardService::hasSnapshotForDate(state_, today)) {
        state_.setStatus("A snapshot already exists for today.", true);
        return;
    }

    const DashboardData data = DashboardService::build(state_, today, Date::currentMonthPrefix(), Date::currentYearPrefix());
    PortfolioSnapshot snapshot = DashboardService::buildSnapshot(data, today);

    std::string error;
    if (!portfolioSnapshotRepository_->upsertForDate(snapshot, error)) {
        state_.setStatus("Could not save manual snapshot: " + error, true);
        return;
    }

    reloadData();
    state_.setStatus("Manual portfolio snapshot saved.");
}

void App::refreshSelectedResearchSymbol()
{
    navigateTo(AppSection::StockResearch);
    stockResearchView_.refreshCurrent(*marketDataService_, state_);
}

void App::refreshDashboardPrices()
{
    std::set<std::string> uniqueTickers;
    for (const Holding& holding : state_.holdings) {
        if (!isActiveHolding(holding)) {
            continue;
        }

        const Account* account = accountForHolding(state_.accounts, holding.accountId);
        if (account == nullptr || !isActiveAccount(*account)) {
            continue;
        }

        const std::string symbol = normalizeSymbol(holding.ticker);
        if (!symbol.empty()) {
            uniqueTickers.insert(symbol);
        }
    }

    state_.dashboardPriceOverrides.clear();
    state_.dashboardPriceRefreshStatus = DashboardPriceRefreshStatus {};
    state_.dashboardPriceRefreshStatus.hasRun = true;
    state_.dashboardPriceRefreshStatus.provider = marketDataService_->providerName();
    state_.dashboardPriceRefreshStatus.lastRefreshedAt = Date::nowIso8601();

    if (uniqueTickers.empty()) {
        state_.dashboardPriceRefreshStatus.warning = "No active holding tickers were available to refresh.";
        state_.setStatus("No active holding tickers were available to refresh.", true);
        return;
    }

    std::vector<std::string> failedSymbols;
    for (const std::string& symbol : uniqueTickers) {
        MarketQuoteResult result = marketDataService_->fetchQuote(symbol);
        if (result.success && result.quote.currentPrice.has_value()) {
            DashboardPriceOverride priceOverride;
            priceOverride.symbol = symbol;
            priceOverride.currentPrice = *result.quote.currentPrice;
            priceOverride.provider = result.quote.provider.empty() ? marketDataService_->providerName() : result.quote.provider;
            priceOverride.fetchedAt = result.quote.fetchedAt;
            priceOverride.fromCache = result.fromCache;
            priceOverride.source = result.fromCache ? "Cached Quote" : "Live Quote";
            state_.dashboardPriceOverrides.push_back(priceOverride);
            ++state_.dashboardPriceRefreshStatus.refreshedSymbols;
            if (result.fromCache) {
                ++state_.dashboardPriceRefreshStatus.cachedSymbols;
            }
        } else {
            ++state_.dashboardPriceRefreshStatus.failedSymbols;
            failedSymbols.push_back(symbol);
        }
    }

    if (state_.dashboardPriceRefreshStatus.cachedSymbols > 0) {
        state_.dashboardPriceRefreshStatus.warning = "Some symbols are using cached quotes because live data was unavailable.";
    }
    if (!failedSymbols.empty()) {
        std::string failed = "Failed symbols:";
        const int limit = std::min<int>(6, static_cast<int>(failedSymbols.size()));
        for (int index = 0; index < limit; ++index) {
            failed += index == 0 ? " " : ", ";
            failed += failedSymbols[static_cast<std::size_t>(index)];
        }
        if (static_cast<int>(failedSymbols.size()) > limit) {
            failed += ", ...";
        }
        state_.dashboardPriceRefreshStatus.warning = state_.dashboardPriceRefreshStatus.warning.empty()
            ? failed
            : state_.dashboardPriceRefreshStatus.warning + " " + failed;
    }
    if (state_.dashboardPriceRefreshStatus.warning.empty()) {
        state_.dashboardPriceRefreshStatus.warning = "Current prices refreshed for dashboard display only. Holdings records and snapshots were not modified.";
    }

    navigateTo(AppSection::Dashboard);
    state_.setStatus("Dashboard prices refreshed for display: " + std::to_string(state_.dashboardPriceRefreshStatus.refreshedSymbols) + " updated, " +
        std::to_string(state_.dashboardPriceRefreshStatus.failedSymbols) + " failed.");
}

void App::refreshWatchlistPrices()
{
    std::string error;
    WatchlistPriceRefreshStatus refreshStatus = WatchlistSignalService::refreshPrices(state_.watchlist, *marketDataService_, *watchlistRepository_, error);
    reloadData();
    state_.watchlistPriceRefreshStatus = refreshStatus;
    navigateTo(AppSection::Watchlist);

    if (refreshStatus.refreshedSymbols > 0) {
        state_.setStatus("All watchlists refreshed: " + std::to_string(refreshStatus.refreshedSymbols) + " updated, " +
            std::to_string(refreshStatus.failedSymbols) + " failed. Last refresh: " + refreshStatus.lastRefreshedAt + ". Source: " +
            (refreshStatus.cachedSymbols > 0 ? "Cached " : "") + refreshStatus.provider + ".");
    } else {
        state_.setStatus(error.empty() ? "Watchlist price refresh did not update any symbols." : error, true);
    }
}

void App::backupDatabaseNow()
{
    if (state_.databaseBackupSettings.backupFolder.empty()) {
        state_.setStatus("Choose a backup folder in Settings before creating a database backup.", true);
        return;
    }

    const DatabaseBackupResult result = DatabaseBackupService::createBackup(database_, state_.databaseBackupSettings.backupFolder);
    if (!result.success) {
        state_.setStatus("Database backup failed: " + result.error, true);
        return;
    }

    std::string error;
    if (!appSettingsRepository_->setString(LastBackupAtSettingKey, result.backedUpAt, error)) {
        state_.setStatus("Database backup created, but last backup time could not be saved: " + error, true);
        return;
    }

    reloadData();
    state_.setStatus("Database backup created: " + result.backupPath);
}

bool App::moveDatabaseToFolder(const std::string& folder, std::string& message)
{
    message.clear();

    if (folder.empty()) {
        message = "Choose a database folder before moving the database.";
        return false;
    }

    const std::filesystem::path targetFolder = absolutePath(folder);
    if (isRepositoryRoot(targetFolder.string())) {
        message = "The repository root cannot be used as the database folder.";
        return false;
    }

    const std::filesystem::path activePath = absolutePath(activeDatabasePath_);
    const std::string fileName = activePath.filename().empty() ? "investor_command_center.db" : activePath.filename().string();
    const std::filesystem::path destinationPath = (targetFolder / fileName).lexically_normal();

    if (pathsEquivalent(destinationPath, activePath)) {
        message = "The selected folder is already the active database folder.";
        return false;
    }

    std::error_code filesystemError;
    if (std::filesystem::exists(destinationPath, filesystemError)) {
        message = "A database file already exists in the selected folder. Choose another folder to avoid overwriting local data.";
        return false;
    }

    std::string error;
    if (!DatabaseBackupService::copyDatabaseToFile(database_, destinationPath.string(), false, error)) {
        message = "Could not copy database to the selected folder: " + error;
        return false;
    }

    if (!writeDatabasePathSettingToFile(destinationPath.string(), destinationPath.string(), error)) {
        message = "Database was copied, but the new database path setting could not be written to the copied database: " + error;
        return false;
    }

    const std::filesystem::path defaultPath = absolutePath(DatabasePath);
    if (!pathsEquivalent(activePath, defaultPath)) {
        if (!writeDatabasePathSettingToFile(defaultPath.string(), destinationPath.string(), error)) {
            message = "Database was copied, but the default startup pointer could not be updated: " + error;
            return false;
        }
    }

    if (!appSettingsRepository_->setString(DatabasePathSettingKey, destinationPath.string(), error)) {
        message = "Database was copied, but the current database path setting could not be saved: " + error;
        return false;
    }

    message = "Database copied and verified. Restart Investor Command Center to use: " + destinationPath.string();
    state_.setStatus(message);
    return true;
}

void App::renderTopMenuBar()
{
    if (!ImGui::BeginMainMenuBar()) {
        return;
    }

    if (ImGui::BeginMenu("File")) {
        menuSectionItem(AppSection::ImportCsv, "Import CSV");
        ImGui::BeginDisabled();
        ImGui::MenuItem("Export Data");
        ImGui::EndDisabled();
        menuSectionItem(AppSection::Settings, "Settings");
        ImGui::Separator();
        if (ImGui::MenuItem("Exit")) {
            shouldExit_ = true;
        }
        ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("View")) {
        menuSectionItem(AppSection::Dashboard, "Dashboard");
        menuSectionItem(AppSection::Accounts, "Accounts");
        menuSectionItem(AppSection::Holdings, "Holdings");
        menuSectionItem(AppSection::Transactions, "Transactions");
        menuSectionItem(AppSection::Dividends, "Dividends");
        menuSectionItem(AppSection::Goals, "Goals");
        menuSectionItem(AppSection::Watchlist, "Watchlist");
        menuSectionItem(AppSection::StockResearch, "Stock Research");
        menuSectionItem(AppSection::Reports, "Reports");
        menuSectionItem(AppSection::Settings, "Settings");
        ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("Dashboard")) {
        menuSectionItem(AppSection::Dashboard, "Dashboard");
        ImGui::Separator();
        if (ImGui::MenuItem("Customize Dashboard")) {
            navigateTo(AppSection::Dashboard);
            dashboardView_.setCustomizeMode(true);
        }
        if (ImGui::MenuItem("Reset Dashboard Layout")) {
            std::string error;
            if (dashboardLayoutRepository_->resetToDefaults(error)) {
                reloadData();
                state_.setStatus("Dashboard layout reset.");
            } else {
                state_.setStatus("Could not reset dashboard layout: " + error, true);
            }
        }
        if (ImGui::MenuItem("Refresh Dashboard")) {
            navigateTo(AppSection::Dashboard);
            reloadData();
            state_.setStatus("Dashboard refreshed.");
        }
        if (ImGui::MenuItem("Refresh Current Prices")) {
            refreshDashboardPrices();
        }
        if (ImGui::MenuItem("Create Manual Snapshot")) {
            requestManualSnapshot();
        }
        ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("Records")) {
        menuSectionItem(AppSection::Accounts, "Accounts");
        menuSectionItem(AppSection::Holdings, "Holdings");
        menuSectionItem(AppSection::Transactions, "Transactions");
        menuSectionItem(AppSection::Dividends, "Dividends");
        menuSectionItem(AppSection::Goals, "Goals");
        menuSectionItem(AppSection::Watchlist, "Watchlist");
        ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("Tools")) {
        menuSectionItem(AppSection::ImportCsv, "Import CSV");
        if (ImGui::MenuItem("Refresh Current Prices")) {
            refreshDashboardPrices();
        }
        if (ImGui::MenuItem("Back Up Now")) {
            backupDatabaseNow();
        }
        if (ImGui::MenuItem("Capital Gains Allocation Settings")) {
            navigateTo(AppSection::Settings);
            state_.setStatus("Capital gains allocation rules are managed in Settings.");
        }
        if (ImGui::MenuItem("Data Privacy / Local Data")) {
            showPrivacyPopup_ = true;
        }
        ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("Research")) {
        menuSectionItem(AppSection::StockResearch, "Stock Research");
        if (ImGui::MenuItem("Refresh Selected Symbol")) {
            refreshSelectedResearchSymbol();
        }
        if (ImGui::MenuItem("Refresh Dashboard Prices")) {
            refreshDashboardPrices();
        }
        if (ImGui::MenuItem("Refresh Watchlist Prices")) {
            refreshWatchlistPrices();
        }
        if (ImGui::MenuItem("Research Settings")) {
            navigateTo(AppSection::Settings);
            state_.setStatus("Research settings are shown in Settings. Price refreshes remain explicit/manual.");
        }
        if (ImGui::MenuItem("Data Source / Disclaimer")) {
            showResearchDisclaimerPopup_ = true;
        }
        ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("Help")) {
        if (ImGui::MenuItem("About")) {
            showAboutPopup_ = true;
        }
        if (ImGui::MenuItem("Privacy / Local Data Notice")) {
            showPrivacyPopup_ = true;
        }
        ImGui::BeginDisabled();
        ImGui::MenuItem("Report Bug / GitHub");
        ImGui::EndDisabled();
        ImGui::EndMenu();
    }

    ImGui::EndMainMenuBar();
}

void App::renderAccountColumn()
{
    constexpr float FooterHeight = 210.0f;

    ImGui::BeginChild("MorningSnapshotSidebar", ImVec2(AccountColumnWidth, 0.0f), true);
    ImGui::BeginChild("MorningSnapshotSections", ImVec2(0.0f, -FooterHeight), false);
    renderAccountInfoPanel();
    ImGui::Spacing();
    renderAccountsPanel();
    ImGui::Spacing();
    renderWatchlistPanel(1);
    ImGui::Spacing();
    renderWatchlistPanel(2);
    ImGui::EndChild();

    renderSidebarFooter();
    ImGui::EndChild();
}

void App::renderAccountInfoPanel()
{
    const std::string today = Date::todayIso8601();
    const DashboardData data = DashboardService::build(state_, today, Date::currentMonthPrefix(), Date::currentYearPrefix());
    const ImportBatch* latestImport = DashboardService::latestImportBatch(state_);

    std::string latestPriceRefreshAt;
    std::string latestPriceRefresh = "None";
    if (state_.dashboardPriceRefreshStatus.hasRun && !state_.dashboardPriceRefreshStatus.lastRefreshedAt.empty()) {
        latestPriceRefreshAt = state_.dashboardPriceRefreshStatus.lastRefreshedAt;
        latestPriceRefresh = state_.dashboardPriceRefreshStatus.lastRefreshedAt + " (Dashboard)";
    }
    if (state_.watchlistPriceRefreshStatus.hasRun && !state_.watchlistPriceRefreshStatus.lastRefreshedAt.empty() &&
        (latestPriceRefreshAt.empty() || state_.watchlistPriceRefreshStatus.lastRefreshedAt > latestPriceRefreshAt)) {
        latestPriceRefreshAt = state_.watchlistPriceRefreshStatus.lastRefreshedAt;
        latestPriceRefresh = state_.watchlistPriceRefreshStatus.lastRefreshedAt + " (Watchlist)";
    }
    for (const WatchlistItem& item : state_.watchlist) {
        if (!item.lastPriceRefreshAt.empty() && (latestPriceRefreshAt.empty() || item.lastPriceRefreshAt > latestPriceRefreshAt)) {
            latestPriceRefreshAt = item.lastPriceRefreshAt;
            latestPriceRefresh = item.lastPriceRefreshAt + " (Watchlist)";
        }
    }

    ImGui::BeginChild("AccountInfoPanel", ImVec2(0.0f, 254.0f), true);
    ImGui::TextColored(UiTheme::Accent, "Account Information");
    ImGui::Separator();
    compactMetric("Total Portfolio Value", Money::format(data.portfolio.accountBalance), UiTheme::Gain);
    compactMetric("Holdings Value", Money::format(data.portfolio.holdingsMarketValue), UiTheme::Gain);
    compactMetric("Cash Balance", Money::format(data.portfolio.cashBalance), UiTheme::Amber);
    compactMetric("Unrealized G/L", Money::format(data.portfolio.gainLossDollar), UiTheme::moneyColor(data.portfolio.gainLossDollar));
    compactMetric("Realized G/L", Money::format(data.realizedGains.thisYear), UiTheme::moneyColor(data.realizedGains.thisYear));
    compactMetric("Daily Movement", data.performance.hasDaily ? Money::format(data.performance.daily) : "N/A", data.performance.hasDaily ? UiTheme::moneyColor(data.performance.daily) : UiTheme::TextMuted);
    compactMetric("Last CSV Import", latestImport == nullptr ? "None" : latestImport->importDate, UiTheme::MutedText);
    compactMetric("Last Price Refresh", latestPriceRefresh, UiTheme::MutedText);
    ImGui::EndChild();
}

void App::renderAccountsPanel()
{
    ImGui::BeginChild("AccountsColumnPanel", ImVec2(0.0f, 196.0f), true);
    ImGui::TextColored(UiTheme::Accent, "Accounts");
    ImGui::Separator();

    if (state_.accounts.empty()) {
        ImGui::TextColored(UiTheme::MutedText, "No accounts yet.");
    } else if (ImGui::BeginTable("AccountColumnTable", 4, ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerH | ImGuiTableFlags_SizingStretchProp)) {
        ImGui::TableSetupColumn("Account");
        ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthFixed, 84.0f);
        ImGui::TableSetupColumn("Cash", ImGuiTableColumnFlags_WidthFixed, 72.0f);
        ImGui::TableSetupColumn("Status", ImGuiTableColumnFlags_WidthFixed, 58.0f);
        ImGui::TableHeadersRow();

        const std::vector<Holding> holdings = DashboardService::holdingsWithDashboardPrices(state_);
        for (const Account& account : state_.accounts) {
            const AccountMetrics metrics = PortfolioCalculator::calculateAccount(account, holdings);
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::Text("%s", account.accountName.c_str());
            ImGui::TableNextColumn();
            ImGui::Text("%s", Money::format(metrics.calculatedBalance).c_str());
            ImGui::TableNextColumn();
            ImGui::TextColored(account.status == "Active" ? UiTheme::Amber : UiTheme::MutedText, "%s", Money::format(account.cashBalance).c_str());
            ImGui::TableNextColumn();
            ImGui::TextColored(isActiveAccount(account) ? UiTheme::TextSuccess : UiTheme::TextMuted, "%s", account.status.empty() ? "Active" : account.status.c_str());
        }
        ImGui::EndTable();
    }

    ImGui::EndChild();
}

void App::renderWatchlistPanel(int sidebarSlot)
{
    const Watchlist* assignedWatchlist = sidebarWatchlistForSlot(state_, sidebarSlot);
    const std::string fallbackTitle = sidebarSlot == 1 ? "Watchlist 1" : "Watchlist 2";
    const std::string title = assignedWatchlist == nullptr ? fallbackTitle : assignedWatchlist->name;
    const std::string panelId = "SidebarWatchlistSlot" + std::to_string(sidebarSlot) + "ColumnPanel";

    ImGui::BeginChild(panelId.c_str(), ImVec2(0.0f, 238.0f), true);
    ImGui::TextColored(UiTheme::Accent, "%s", title.c_str());
    ImGui::Separator();

    if (assignedWatchlist == nullptr) {
        ImGui::TextColored(UiTheme::MutedText, "No watchlist selected.");
        ImGui::TextWrapped("Assign one from the Watchlist Manager.");
        ImGui::EndChild();
        return;
    }

    const std::vector<WatchlistItem> items = sidebarWatchlistItems(state_, assignedWatchlist->id);
    if (items.empty()) {
        ImGui::TextColored(UiTheme::MutedText, "No items.");
    } else if (ImGui::BeginTable((panelId + "Table").c_str(), 3, ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerH | ImGuiTableFlags_SizingStretchProp)) {
        ImGui::TableSetupColumn("Ticker", ImGuiTableColumnFlags_WidthFixed, 64.0f);
        ImGui::TableSetupColumn("Price", ImGuiTableColumnFlags_WidthFixed, 86.0f);
        ImGui::TableSetupColumn("Signal");
        ImGui::TableHeadersRow();

        const int limit = std::min<int>(10, static_cast<int>(items.size()));
        for (int index = 0; index < limit; ++index) {
            const WatchlistItem& item = items[static_cast<std::size_t>(index)];
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::Text("%s", item.ticker.c_str());
            ImGui::TableNextColumn();
            const std::string currentPrice = item.currentPrice > 0.0 ? Money::format(item.currentPrice) : "N/A";
            ImGui::Text("%s", currentPrice.c_str());
            ImGui::TableNextColumn();
            const std::string signalStatus = WatchlistSignalService::calculateSignalStatus(item.currentPrice, item.buySignalPrice, item.sellSignalPrice);
            ImGui::TextColored(sidebarSignalColor(item), "%s", signalStatus.c_str());
        }
        ImGui::EndTable();
    }

    ImGui::EndChild();
}

void App::renderSidebarFooter()
{
    ImGui::BeginChild("SidebarVersionDatabaseInfo", ImVec2(0.0f, 0.0f), true);
    ImGui::TextColored(UiTheme::Accent, "Version Information / Database Info");
    ImGui::Separator();
    compactMetric("App Version", AppVersion, UiTheme::TextSecondary);
    compactMetric("SQLite Status", database_.handle() == nullptr ? "Disconnected" : "Connected", database_.handle() == nullptr ? UiTheme::TextDanger : UiTheme::TextSuccess);
    const std::string reminderText = DatabaseBackupService::reminderStatusText(state_.databaseBackupSettings, Date::todayIso8601());
    const bool backupDue = DatabaseBackupService::isReminderDue(state_.databaseBackupSettings, Date::todayIso8601());
    compactMetric("Backup", reminderText, backupDue ? UiTheme::Amber : UiTheme::TextMuted);
    if (ImGui::Button("Back Up Now", ImVec2(120.0f, 0.0f))) {
        backupDatabaseNow();
    }
    if (state_.databaseBackupSettings.backupFolder.empty()) {
        ImGui::SameLine();
        ImGui::TextColored(UiTheme::TextWarning, "Set folder in Settings");
    }
    ImGui::TextColored(UiTheme::TextMuted, "Current Database Path");
    ImGui::TextWrapped("%s", activeDatabasePath_.c_str());
    if (isPathInsideRepository(activeDatabasePath_)) {
        ImGui::TextColored(UiTheme::TextWarning, "Database is inside the repository. Consider moving it outside Git.");
    }
    ImGui::EndChild();
}

void App::renderCurrentSection()
{
    const auto reload = [this]() {
        reloadData();
    };

    switch (state_.currentSection) {
    case AppSection::Dashboard:
        dashboardView_.render(state_, *portfolioSnapshotRepository_, *dashboardLayoutRepository_, *dashboardChartSettingsRepository_, [this]() {
            refreshDashboardPrices();
        },
            reload);
        break;
    case AppSection::Accounts:
        accountsView_.render(state_, *accountRepository_, reload);
        break;
    case AppSection::Holdings:
        holdingsView_.render(state_, *holdingRepository_, reload);
        break;
    case AppSection::Transactions:
        transactionsView_.render(state_, *transactionService_, reload);
        break;
    case AppSection::Dividends:
        dividendsView_.render(state_, *dividendRepository_, reload);
        break;
    case AppSection::Goals:
        goalsView_.render(state_, *goalRepository_, reload);
        break;
    case AppSection::Watchlist:
        watchlistView_.render(state_, *watchlistRepository_, *marketDataService_, reload);
        break;
    case AppSection::ImportCsv:
        importCsvView_.render(state_, *csvImportService_, reload);
        break;
    case AppSection::StockResearch:
        stockResearchView_.render(state_, *marketDataService_, *watchlistRepository_, reload);
        break;
    case AppSection::Reports:
        renderPlaceholder("Reports", "Reports will summarize local records without advice or recommendations.");
        break;
    case AppSection::Settings:
        settingsView_.render(state_, *appSettingsRepository_, *capitalGainAllocationRepository_, activeDatabasePath_, AppVersion, [this]() {
            backupDatabaseNow();
        },
            [this](const std::string& folder, std::string& message) {
                return moveDatabaseToFolder(folder, message);
            },
            reload);
        break;
    }
}

void App::renderAppPopups()
{
    if (showAboutPopup_) {
        ImGui::OpenPopup("About Investor Command Center");
        showAboutPopup_ = false;
    }
    if (ImGui::BeginPopupModal("About Investor Command Center", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("Investor Command Center");
        ImGui::TextColored(UiTheme::MutedText, "Long-term progress beats short-term noise.");
        ImGui::Separator();
        ImGui::TextWrapped("A local-first personal investing tracker for reviewing accounts, holdings, transactions, dividends, goals, and import snapshots.");
        ImGui::TextColored(UiTheme::MutedText, "Version %s", AppVersion);
        if (ImGui::Button("Close", ImVec2(100.0f, 0.0f))) {
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }

    if (showPrivacyPopup_) {
        ImGui::OpenPopup("Privacy And Local Data");
        showPrivacyPopup_ = false;
    }
    if (ImGui::BeginPopupModal("Privacy And Local Data", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("Privacy / Local Data");
        ImGui::Separator();
        ImGui::TextWrapped("Investor Command Center stores personal investing data in a local SQLite database. It does not connect to brokerage accounts or cloud services. Stock Research can fetch informational Yahoo Finance data only when explicitly requested.");
        ImGui::Spacing();
        ImGui::TextColored(UiTheme::Amber, "Do not commit databases, CSV files, exports, backups, or logs.");
        ImGui::TextColored(UiTheme::MutedText, "This app is for personal tracking and does not provide financial advice.");
        if (ImGui::Button("Close", ImVec2(100.0f, 0.0f))) {
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }

    if (showResearchDisclaimerPopup_) {
        ImGui::OpenPopup("Research Data Source");
        showResearchDisclaimerPopup_ = false;
    }
    if (ImGui::BeginPopupModal("Research Data Source", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("Research Data Source");
        ImGui::Separator();
        ImGui::TextWrapped("Stock Research uses Yahoo Finance as the first informational market data source. Yahoo Finance endpoints may be delayed, unavailable, rate-limited, or changed without notice.");
        ImGui::Spacing();
        ImGui::TextWrapped("Research data is fetched only when explicitly requested. Stock Research lookups do not write local holdings; Dashboard price refresh uses a display-only overlay. Neither replaces CSV imports, connects to brokerage accounts, or provides financial advice.");
        ImGui::Spacing();
        ImGui::TextColored(UiTheme::Amber, "CSV import remains the primary portfolio update workflow.");
        if (ImGui::Button("Close", ImVec2(100.0f, 0.0f))) {
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }

    if (showManualSnapshotReplacePopup_) {
        ImGui::OpenPopup("Replace Manual Snapshot");
        showManualSnapshotReplacePopup_ = false;
    }
    if (ImGui::BeginPopupModal("Replace Manual Snapshot", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("A snapshot already exists for today.");
        ImGui::TextColored(UiTheme::MutedText, "Replace it with the current local portfolio totals?");
        ImGui::Spacing();
        if (ImGui::Button("Replace Snapshot", ImVec2(150.0f, 0.0f))) {
            createManualSnapshot(true);
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(100.0f, 0.0f))) {
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
}

void App::renderPlaceholder(const char* title, const char* note)
{
    UiTheme::sectionHeading(title, note);
    ImGui::BeginChild(title, ImVec2(0.0f, 230.0f), true);
    ImGui::TextColored(UiTheme::Amber, "Planned");
    ImGui::TextColored(UiTheme::MutedText, "This page will stay local-first and read from the SQLite database.");
    ImGui::Spacing();
    ImGui::BeginDisabled();
    ImGui::Button("Export CSV", ImVec2(140.0f, 0.0f));
    ImGui::EndDisabled();
    ImGui::SameLine();
    ImGui::TextColored(UiTheme::MutedText, "CSV export placeholder");
    ImGui::EndChild();
}

void App::renderStatusBar()
{
    if (state_.statusMessage.empty()) {
        return;
    }

    ImGui::Separator();
    ImGui::TextColored(state_.statusIsError ? UiTheme::Loss : UiTheme::Gain, "%s", state_.statusMessage.c_str());
}
