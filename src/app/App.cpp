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
#include <array>
#include <cctype>
#include <filesystem>
#include <map>
#include <optional>
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

void compactMetric(const char* label, const std::string& value, ImVec4 color, bool numericValue = true)
{
    constexpr float ValueColumnX = 148.0f;

    ImGui::TextColored(UiTheme::TextMuted, "%s", label);
    ImGui::SameLine(ValueColumnX);
    ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + ImGui::GetContentRegionAvail().x);
    if (numericValue) {
        UiTheme::pushNumericFont();
    }
    ImGui::TextColored(color, "%s", value.c_str());
    if (numericValue) {
        UiTheme::popNumericFont();
    }
    ImGui::PopTextWrapPos();
    ImGui::Dummy(ImVec2(0.0f, 2.0f));
}

void drawPanelChrome(const char* title, ImVec4 accent, float accentAlpha = 0.38f)
{
    const ImVec2 min = ImGui::GetWindowPos();
    const ImVec2 size = ImGui::GetWindowSize();
    const ImVec2 max(min.x + size.x, min.y + size.y);
    ImDrawList* drawList = ImGui::GetWindowDrawList();

    drawList->AddRectFilled(min, ImVec2(max.x, min.y + 34.0f), ImGui::GetColorU32(UiTheme::withAlpha(UiTheme::SurfaceElevated, 0.14f)), 16.0f, ImDrawFlags_RoundCornersTop);
    drawList->AddRectFilled(ImVec2(min.x + 14.0f, min.y + 9.0f), ImVec2(min.x + 56.0f, min.y + 11.0f), ImGui::GetColorU32(UiTheme::withAlpha(accent, accentAlpha)), 2.0f);
    drawList->AddRect(min, max, ImGui::GetColorU32(UiTheme::withAlpha(UiTheme::BorderSubtle, 0.07f)), 16.0f);

    ImGui::TextColored(UiTheme::TextSecondary, "%s", title);
    const ImVec2 cursor = ImGui::GetCursorScreenPos();
    const float width = ImGui::GetContentRegionAvail().x;
    drawList->AddRectFilled(cursor, ImVec2(cursor.x + width, cursor.y + 1.0f), ImGui::GetColorU32(UiTheme::withAlpha(UiTheme::BorderSubtle, 0.08f)));
    ImGui::Dummy(ImVec2(width, 7.0f));
}

void beginFinancePanel(const char* id, const char* title, ImVec2 size, ImVec4 accent, ImGuiWindowFlags flags = 0)
{
    UiTheme::pushPanelStyle();
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(13.0f, 11.0f));
    ImGui::BeginChild(id, size, true, flags);
    ImGui::PopStyleVar();
    drawPanelChrome(title, accent);
}

void endFinancePanel()
{
    ImGui::EndChild();
    UiTheme::popPanelStyle();
}

std::string latestPriceRefreshText(const AppState& state)
{
    std::string latestPriceRefreshAt;
    std::string latestPriceRefresh = "None";
    if (state.dashboardPriceRefreshStatus.hasRun && !state.dashboardPriceRefreshStatus.lastRefreshedAt.empty()) {
        latestPriceRefreshAt = state.dashboardPriceRefreshStatus.lastRefreshedAt;
        latestPriceRefresh = state.dashboardPriceRefreshStatus.lastRefreshedAt + " (Dashboard)";
    }
    if (state.watchlistPriceRefreshStatus.hasRun && !state.watchlistPriceRefreshStatus.lastRefreshedAt.empty() &&
        (latestPriceRefreshAt.empty() || state.watchlistPriceRefreshStatus.lastRefreshedAt > latestPriceRefreshAt)) {
        latestPriceRefreshAt = state.watchlistPriceRefreshStatus.lastRefreshedAt;
        latestPriceRefresh = state.watchlistPriceRefreshStatus.lastRefreshedAt + " (Watchlist)";
    }
    for (const WatchlistItem& item : state.watchlist) {
        if (!item.lastPriceRefreshAt.empty() && (latestPriceRefreshAt.empty() || item.lastPriceRefreshAt > latestPriceRefreshAt)) {
            latestPriceRefreshAt = item.lastPriceRefreshAt;
            latestPriceRefresh = item.lastPriceRefreshAt + " (Watchlist)";
        }
    }

    return latestPriceRefresh;
}

bool isPortfolioShellSection(AppSection section)
{
    return section == AppSection::Accounts ||
        section == AppSection::Holdings ||
        section == AppSection::Transactions ||
        section == AppSection::Dividends ||
        section == AppSection::Goals ||
        section == AppSection::ImportCsv;
}

const char* sectionSubtitle(AppSection section)
{
    switch (section) {
    case AppSection::Dashboard:
        return "Portfolio overview, performance, and dashboard intelligence.";
    case AppSection::Accounts:
        return "Account balances, status, and local brokerage records.";
    case AppSection::Holdings:
        return "Open positions, cost basis, and market value.";
    case AppSection::Transactions:
        return "Activity ledger for buys, sells, deposits, withdrawals, and adjustments.";
    case AppSection::Dividends:
        return "Dividend income, payment history, and yield tracking.";
    case AppSection::Goals:
        return "Long-term portfolio targets and progress monitoring.";
    case AppSection::Watchlist:
        return "Tracked symbols, price alerts, and technical signal review.";
    case AppSection::ImportCsv:
        return "CSV import workflow for local portfolio updates.";
    case AppSection::StockResearch:
        return "Symbol research, quote refreshes, and technical indicators.";
    case AppSection::Reports:
        return "Local reporting workspace for portfolio records.";
    case AppSection::Settings:
        return "Application preferences, backups, and local data controls.";
    }

    return "Local-first portfolio intelligence.";
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

std::optional<TechnicalIndicatorSnapshot> cachedIndicatorsFor(TechnicalIndicatorService& technicalIndicatorService, const WatchlistItem& item)
{
    std::string error;
    return technicalIndicatorService.cachedSnapshot(item.ticker, "Yahoo Finance", error);
}

ImVec4 sidebarSignalColor(const WatchlistSignalResult& signal)
{
    if (signal.signal == "Buy") {
        return UiTheme::Gain;
    }
    if (signal.signal == "Sell") {
        return UiTheme::Loss;
    }
    return UiTheme::ElectricCyan;
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

void sortWatchlistItemsBySignal(std::vector<WatchlistItem>& items, TechnicalIndicatorService& technicalIndicatorService)
{
    std::map<int, WatchlistSignalResult> signalResults;
    for (const WatchlistItem& item : items) {
        signalResults[item.id] = WatchlistSignalService::calculateSignal(item, cachedIndicatorsFor(technicalIndicatorService, item));
    }

    std::stable_sort(items.begin(), items.end(), [](const WatchlistItem& left, const WatchlistItem& right) {
        return left.ticker < right.ticker;
    });

    std::stable_sort(items.begin(), items.end(), [&signalResults](const WatchlistItem& left, const WatchlistItem& right) {
        const int leftSignalRank = WatchlistSignalService::signalSortRank(signalResults[left.id].signal);
        const int rightSignalRank = WatchlistSignalService::signalSortRank(signalResults[right.id].signal);
        if (leftSignalRank != rightSignalRank) {
            return leftSignalRank < rightSignalRank;
        }

        const int leftPriorityRank = watchlistPrioritySortRank(left.priority);
        const int rightPriorityRank = watchlistPrioritySortRank(right.priority);
        if (leftPriorityRank != rightPriorityRank) {
            return leftPriorityRank < rightPriorityRank;
        }

        return false;
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

std::vector<WatchlistItem> sidebarWatchlistItems(const AppState& state, int watchlistId, TechnicalIndicatorService& technicalIndicatorService)
{
    std::vector<WatchlistItem> items;
    for (const WatchlistItem& item : state.watchlist) {
        if (item.watchlistId == watchlistId) {
            items.push_back(item);
        }
    }
    sortWatchlistItemsBySignal(items, technicalIndicatorService);
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
    marketPriceHistoryRepository_ = std::make_unique<MarketPriceHistoryRepository>(database_);
    technicalIndicatorCacheRepository_ = std::make_unique<TechnicalIndicatorCacheRepository>(database_);
    portfolioSnapshotRepository_ = std::make_unique<PortfolioSnapshotRepository>(database_);
    dashboardLayoutRepository_ = std::make_unique<DashboardLayoutRepository>(database_);
    dashboardChartSettingsRepository_ = std::make_unique<DashboardChartSettingsRepository>(database_);
    appSettingsRepository_ = std::make_unique<AppSettingsRepository>(database_);
    capitalGainAllocationRepository_ = std::make_unique<CapitalGainAllocationRepository>(database_);
    csvImportService_ = std::make_unique<CsvImportService>(database_, *holdingRepository_, *importBatchRepository_, *portfolioSnapshotRepository_);
    yahooFinanceProvider_ = std::make_unique<YahooFinanceProvider>();
    marketDataService_ = std::make_unique<MarketDataService>(*yahooFinanceProvider_, *marketQuoteCacheRepository_, *marketPriceHistoryRepository_);
    technicalIndicatorService_ = std::make_unique<TechnicalIndicatorService>(*marketPriceHistoryRepository_, *technicalIndicatorCacheRepository_);

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
    UiTheme::drawAppBackdrop(viewport->WorkPos, viewport->WorkSize);
    ImGui::SetNextWindowPos(viewport->WorkPos);
    ImGui::SetNextWindowSize(viewport->WorkSize);

    const ImGuiWindowFlags windowFlags =
        ImGuiWindowFlags_NoTitleBar |
        ImGuiWindowFlags_NoCollapse |
        ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoBringToFrontOnFocus |
        ImGuiWindowFlags_NoNavFocus |
        ImGuiWindowFlags_NoBackground;

    ImGui::Begin("Investor Command Center", nullptr, windowFlags);

    renderShellHeader();
    ImGui::Spacing();

    renderAccountColumn();
    ImGui::SameLine();

    UiTheme::pushPanelStyle();
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(16.0f, 14.0f));
    ImGui::BeginChild("Content", ImVec2(0.0f, 0.0f), true);
    ImGui::PopStyleVar();

    UiTheme::sectionHeading(sectionTitle(state_.currentSection), sectionSubtitle(state_.currentSection));

    const float statusHeight = state_.statusMessage.empty() ? 0.0f : 38.0f;
    ImGui::BeginChild("PageContent", ImVec2(0.0f, -statusHeight), false);
    renderCurrentSection();
    ImGui::EndChild();

    renderStatusBar();

    ImGui::EndChild();
    UiTheme::popPanelStyle();
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

void App::refreshSelectedResearchHistory()
{
    navigateTo(AppSection::StockResearch);
    stockResearchView_.refreshCurrentHistory(*marketDataService_, *technicalIndicatorService_, state_);
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
    WatchlistPriceRefreshStatus refreshStatus = WatchlistSignalService::refreshPrices(state_.watchlist, *marketDataService_, *technicalIndicatorService_, *watchlistRepository_, error);
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
    ImGui::PushStyleColor(ImGuiCol_MenuBarBg, UiTheme::withAlpha(UiTheme::SurfaceMain, 0.96f));
    ImGui::PushStyleColor(ImGuiCol_PopupBg, UiTheme::withAlpha(UiTheme::GlassPanel, 0.98f));
    ImGui::PushStyleColor(ImGuiCol_Header, UiTheme::withAlpha(UiTheme::ElectricCyan, 0.10f));
    ImGui::PushStyleColor(ImGuiCol_HeaderHovered, UiTheme::withAlpha(UiTheme::ElectricCyan, 0.16f));
    ImGui::PushStyleColor(ImGuiCol_HeaderActive, UiTheme::withAlpha(UiTheme::NeonMagenta, 0.14f));
    ImGui::PushStyleColor(ImGuiCol_Border, UiTheme::BorderSubtle);
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(10.0f, 6.0f));
    if (!ImGui::BeginMainMenuBar()) {
        ImGui::PopStyleVar();
        ImGui::PopStyleColor(6);
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
        if (ImGui::MenuItem("Refresh History for Research Symbol")) {
            refreshSelectedResearchHistory();
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
    ImGui::PopStyleVar();
    ImGui::PopStyleColor(6);
}

void App::renderShellHeader()
{
    constexpr float HeaderHeight = 104.0f;

    UiTheme::pushPanelStyle();
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(18.0f, 14.0f));
    ImGui::BeginChild("AppShellHeader", ImVec2(0.0f, HeaderHeight), true, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
    ImGui::PopStyleVar();

    const ImVec2 min = ImGui::GetWindowPos();
    const ImVec2 size = ImGui::GetWindowSize();
    const ImVec2 max(min.x + size.x, min.y + size.y);
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    drawList->AddRectFilled(min, max, ImGui::GetColorU32(UiTheme::withAlpha(UiTheme::GlassPanel, 0.92f)), 18.0f);
    drawList->AddRectFilled(min, ImVec2(max.x, min.y + 44.0f), ImGui::GetColorU32(UiTheme::withAlpha(UiTheme::SurfaceElevated, 0.24f)), 18.0f, ImDrawFlags_RoundCornersTop);
    drawList->AddRectFilled(ImVec2(min.x + 18.0f, min.y + 10.0f), ImVec2(min.x + 128.0f, min.y + 12.0f), ImGui::GetColorU32(UiTheme::withAlpha(UiTheme::ElectricCyan, 0.34f)), 2.0f);
    drawList->AddRect(min, max, ImGui::GetColorU32(UiTheme::withAlpha(UiTheme::BorderSubtle, 0.08f)), 18.0f);

    ImGui::SetWindowFontScale(1.20f);
    ImGui::TextColored(UiTheme::TextPrimary, "Investor Command Center");
    ImGui::SetWindowFontScale(1.0f);
    ImGui::TextColored(UiTheme::TextSecondary, "Advanced Portfolio Intelligence");

    if (size.x > 820.0f) {
        const bool databaseConnected = database_.handle() != nullptr;
        const float statusWidth = 390.0f;
        ImGui::SetCursorPos(ImVec2(size.x - statusWidth - 18.0f, 18.0f));
        UiTheme::badge(databaseConnected ? "SQLite Connected" : "SQLite Disconnected", databaseConnected ? UiTheme::Gain : UiTheme::Loss);
        ImGui::SameLine(0.0f, 10.0f);
        ImGui::TextColored(UiTheme::TextMuted, "Market Data: %s", marketDataService_ == nullptr ? "N/A" : marketDataService_->providerName());
    }

    ImGui::SetCursorPos(ImVec2(18.0f, 60.0f));
    renderTopNavigationTabs();

    ImGui::EndChild();
    UiTheme::popPanelStyle();
}

void App::renderTopNavigationTabs()
{
    struct TopTab {
        const char* label;
        AppSection target;
        bool enabled;
        bool portfolioGroup;
        const char* unavailableMessage;
    };

    static constexpr std::array<TopTab, 8> Tabs {{
        { "Dashboard", AppSection::Dashboard, true, false, nullptr },
        { "Portfolio", AppSection::Holdings, true, true, nullptr },
        { "Watchlist", AppSection::Watchlist, true, false, nullptr },
        { "Screener", AppSection::StockResearch, true, false, nullptr },
        { "News", AppSection::Dashboard, false, false, "News is not available in this build." },
        { "Analytics", AppSection::Dashboard, false, false, "Analytics is currently shown inside Dashboard." },
        { "Reports", AppSection::Reports, true, false, nullptr },
        { "Settings", AppSection::Settings, true, false, nullptr },
    }};

    const float rowStartX = ImGui::GetCursorPosX();
    const float rowEndX = ImGui::GetWindowContentRegionMax().x;
    bool firstTab = true;

    for (const TopTab& tab : Tabs) {
        const ImVec2 textSize = ImGui::CalcTextSize(tab.label);
        const ImVec2 tabSize(std::max(78.0f, textSize.x + 30.0f), 34.0f);

        if (!firstTab) {
            ImGui::SameLine(0.0f, 8.0f);
        }
        if (ImGui::GetCursorPosX() + tabSize.x > rowEndX && ImGui::GetCursorPosX() > rowStartX) {
            ImGui::NewLine();
            ImGui::SetCursorPosX(rowStartX);
        }

        const bool selected = tab.enabled && (tab.portfolioGroup ? isPortfolioShellSection(state_.currentSection) : state_.currentSection == tab.target);
        ImGui::PushID(tab.label);
        if (!tab.enabled) {
            ImGui::BeginDisabled();
        }
        const ImVec2 min = ImGui::GetCursorScreenPos();
        const bool clicked = ImGui::InvisibleButton("TopNavTab", tabSize);
        const bool hovered = ImGui::IsItemHovered();
        const bool focused = ImGui::IsItemFocused();
        if (!tab.enabled) {
            ImGui::EndDisabled();
        }

        const ImVec2 max(min.x + tabSize.x, min.y + tabSize.y);
        const ImVec4 fill = selected
            ? UiTheme::withAlpha(UiTheme::SurfaceElevated, 0.40f)
            : hovered ? UiTheme::withAlpha(UiTheme::ElectricCyan, 0.07f) : UiTheme::withAlpha(UiTheme::SurfaceMain, 0.04f);
        const ImVec4 border = selected
            ? UiTheme::withAlpha(UiTheme::NeonMagenta, 0.20f)
            : focused ? UiTheme::FocusRing : hovered ? UiTheme::withAlpha(UiTheme::ElectricCyan, 0.12f) : UiTheme::withAlpha(UiTheme::BorderSubtle, 0.04f);
        const ImVec4 text = !tab.enabled
            ? UiTheme::withAlpha(UiTheme::TextMuted, 0.52f)
            : selected ? UiTheme::TextPrimary : hovered ? UiTheme::TextSecondary : UiTheme::TextMuted;

        ImDrawList* drawList = ImGui::GetWindowDrawList();
        drawList->AddRectFilled(min, max, ImGui::GetColorU32(fill), 10.0f);
        drawList->AddRect(min, max, ImGui::GetColorU32(border), 10.0f);
        if (selected) {
            drawList->AddRectFilled(ImVec2(min.x + 14.0f, max.y - 3.0f), ImVec2(max.x - 14.0f, max.y - 1.0f), ImGui::GetColorU32(UiTheme::withAlpha(UiTheme::NeonMagenta, 0.72f)), 2.0f);
        } else if (hovered && tab.enabled) {
            drawList->AddRectFilled(ImVec2(min.x + 18.0f, max.y - 2.0f), ImVec2(max.x - 18.0f, max.y - 1.0f), ImGui::GetColorU32(UiTheme::withAlpha(UiTheme::ElectricCyan, 0.28f)), 2.0f);
        }
        drawList->AddText(ImVec2(min.x + (tabSize.x - textSize.x) * 0.5f, min.y + (tabSize.y - textSize.y) * 0.5f), ImGui::GetColorU32(text), tab.label);

        if (tab.enabled && clicked) {
            navigateTo(tab.target);
        }
        if (!tab.enabled && hovered && tab.unavailableMessage != nullptr) {
            ImGui::SetTooltip("%s", tab.unavailableMessage);
        }
        ImGui::PopID();
        firstTab = false;
    }
}

void App::renderAccountColumn()
{
    constexpr float FooterHeight = 260.0f;

    UiTheme::pushPanelStyle();
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(10.0f, 10.0f));
    ImGui::BeginChild("MorningSnapshotSidebar", ImVec2(AccountColumnWidth, 0.0f), true);
    ImGui::PopStyleVar();
    ImGui::BeginChild("MorningSnapshotSections", ImVec2(0.0f, -FooterHeight), false);
    renderSidebarNavigation();
    ImGui::Spacing();
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
    UiTheme::popPanelStyle();
}

void App::renderSidebarNavigation()
{
    beginFinancePanel("SidebarNavigationPanel", "Navigation", ImVec2(0.0f, 394.0f), UiTheme::NeonMagenta);
    renderSidebarNavigationItem(AppSection::Dashboard, "DB", "Dashboard");
    renderSidebarNavigationItem(AppSection::Accounts, "AC", "Accounts");
    renderSidebarNavigationItem(AppSection::Holdings, "HD", "Holdings");
    renderSidebarNavigationItem(AppSection::Transactions, "TX", "Transactions");
    renderSidebarNavigationItem(AppSection::Dividends, "DV", "Dividends");
    renderSidebarNavigationItem(AppSection::Goals, "GL", "Goals");
    renderSidebarNavigationItem(AppSection::Watchlist, "WL", "Watchlist");
    renderSidebarNavigationItem(AppSection::StockResearch, "SR", "Stock Research");
    renderSidebarNavigationItem(AppSection::Reports, "RP", "Reports");
    renderSidebarNavigationItem(AppSection::ImportCsv, "CSV", "Import CSV");
    renderSidebarNavigationItem(AppSection::Settings, "ST", "Settings");
    endFinancePanel();
}

void App::renderSidebarNavigationItem(AppSection section, const char* icon, const char* label)
{
    const bool selected = state_.currentSection == section;
    const float width = ImGui::GetContentRegionAvail().x;
    const ImVec2 itemSize(width, 34.0f);

    ImGui::PushID(label);
    const ImVec2 min = ImGui::GetCursorScreenPos();
    const bool clicked = ImGui::InvisibleButton("SidebarNavigationItem", itemSize);
    const bool hovered = ImGui::IsItemHovered();
    const bool focused = ImGui::IsItemFocused();
    const ImVec2 max(min.x + itemSize.x, min.y + itemSize.y);

    ImDrawList* drawList = ImGui::GetWindowDrawList();
    const ImVec4 fill = selected
        ? UiTheme::withAlpha(UiTheme::NeonMagenta, 0.08f)
        : hovered ? UiTheme::withAlpha(UiTheme::ElectricCyan, 0.07f) : UiTheme::withAlpha(UiTheme::SurfaceMain, 0.08f);
    const ImVec4 border = focused
        ? UiTheme::FocusRing
        : selected ? UiTheme::withAlpha(UiTheme::NeonMagenta, 0.18f) : hovered ? UiTheme::withAlpha(UiTheme::ElectricCyan, 0.10f) : UiTheme::withAlpha(UiTheme::BorderSubtle, 0.02f);
    const ImVec4 iconColor = selected ? UiTheme::NeonMagenta : hovered ? UiTheme::ElectricCyan : UiTheme::withAlpha(UiTheme::ElectricCyan, 0.72f);
    const ImVec4 textColor = selected ? UiTheme::TextPrimary : hovered ? UiTheme::TextSecondary : UiTheme::TextSecondary;

    drawList->AddRectFilled(min, max, ImGui::GetColorU32(fill), 10.0f);
    drawList->AddRect(min, max, ImGui::GetColorU32(border), 10.0f);
    if (selected) {
        drawList->AddRectFilled(ImVec2(min.x, min.y + 6.0f), ImVec2(min.x + 3.0f, max.y - 6.0f), ImGui::GetColorU32(UiTheme::withAlpha(UiTheme::NeonMagenta, 0.72f)), 3.0f);
    }

    const ImVec2 iconMin(min.x + 10.0f, min.y + 5.0f);
    const ImVec2 iconMax(iconMin.x + 31.0f, iconMin.y + 24.0f);
    const ImVec2 iconTextSize = ImGui::CalcTextSize(icon);
    const ImVec2 labelTextSize = ImGui::CalcTextSize(label);
    drawList->AddRectFilled(iconMin, iconMax, ImGui::GetColorU32(UiTheme::withAlpha(iconColor, selected ? 0.12f : hovered ? 0.10f : 0.05f)), 8.0f);
    if (selected || hovered) {
        drawList->AddRect(iconMin, iconMax, ImGui::GetColorU32(UiTheme::withAlpha(iconColor, selected ? 0.22f : 0.16f)), 8.0f);
    }
    drawList->AddText(ImVec2(iconMin.x + (31.0f - iconTextSize.x) * 0.5f, iconMin.y + (24.0f - iconTextSize.y) * 0.5f), ImGui::GetColorU32(iconColor), icon);
    drawList->AddText(ImVec2(min.x + 52.0f, min.y + (itemSize.y - labelTextSize.y) * 0.5f), ImGui::GetColorU32(textColor), label);

    if (clicked) {
        navigateTo(section);
    }
    ImGui::PopID();
}

void App::renderAccountInfoPanel()
{
    const std::string today = Date::todayIso8601();
    const DashboardData data = DashboardService::build(state_, today, Date::currentMonthPrefix(), Date::currentYearPrefix());
    const ImportBatch* latestImport = DashboardService::latestImportBatch(state_);
    const std::string latestPriceRefresh = latestPriceRefreshText(state_);

    beginFinancePanel("AccountInfoPanel", "Portfolio Snapshot", ImVec2(0.0f, 292.0f), UiTheme::ElectricCyan);
    ImGui::TextColored(UiTheme::TextMuted, "TOTAL PORTFOLIO VALUE");
    ImGui::SetWindowFontScale(1.18f);
    UiTheme::pushNumericFont();
    ImGui::TextColored(UiTheme::TextPrimary, "%s", Money::format(data.portfolio.accountBalance).c_str());
    UiTheme::popNumericFont();
    ImGui::SetWindowFontScale(1.0f);
    ImGui::TextColored(UiTheme::TextSecondary, "Holdings plus cash");
    const ImVec2 separator = ImGui::GetCursorScreenPos();
    const float separatorWidth = ImGui::GetContentRegionAvail().x;
    ImGui::GetWindowDrawList()->AddRectFilled(separator, ImVec2(separator.x + separatorWidth, separator.y + 1.0f), ImGui::GetColorU32(UiTheme::withAlpha(UiTheme::BorderSubtle, 0.08f)));
    ImGui::Dummy(ImVec2(separatorWidth, 8.0f));
    compactMetric("Holdings Value", Money::format(data.portfolio.holdingsMarketValue), UiTheme::SoftBlue);
    compactMetric("Cash Balance", Money::format(data.portfolio.cashBalance), UiTheme::ElectricCyan);
    compactMetric("Unrealized G/L", Money::format(data.portfolio.gainLossDollar), UiTheme::moneyColor(data.portfolio.gainLossDollar));
    compactMetric("Realized G/L", Money::format(data.realizedGains.thisYear), UiTheme::moneyColor(data.realizedGains.thisYear));
    compactMetric("Daily Movement", data.performance.hasDaily ? Money::format(data.performance.daily) : "N/A", data.performance.hasDaily ? UiTheme::moneyColor(data.performance.daily) : UiTheme::TextMuted);
    compactMetric("Last CSV Import", latestImport == nullptr ? "None" : latestImport->importDate, UiTheme::MutedText, false);
    compactMetric("Last Price Refresh", latestPriceRefresh, UiTheme::MutedText, false);
    endFinancePanel();
}

void App::renderAccountsPanel()
{
    beginFinancePanel("AccountsColumnPanel", "Accounts", ImVec2(0.0f, 206.0f), UiTheme::SoftBlue);

    if (state_.accounts.empty()) {
        ImGui::TextColored(UiTheme::MutedText, "No accounts yet.");
    } else {
        UiTheme::pushTableStyle();
        if (ImGui::BeginTable("AccountColumnTable", 4, ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerH | ImGuiTableFlags_SizingStretchProp)) {
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
                UiTheme::textRightAligned(Money::format(metrics.calculatedBalance).c_str());
                ImGui::TableNextColumn();
                UiTheme::textRightAligned(Money::format(account.cashBalance).c_str(), account.status == "Active" ? UiTheme::ElectricCyan : UiTheme::TextMuted);
                ImGui::TableNextColumn();
                UiTheme::badge(account.status.empty() ? "Active" : account.status.c_str(), isActiveAccount(account) ? UiTheme::Gain : UiTheme::TextMuted);
            }
            ImGui::EndTable();
        }
        UiTheme::popTableStyle();
    }

    endFinancePanel();
}

void App::renderWatchlistPanel(int sidebarSlot)
{
    const Watchlist* assignedWatchlist = sidebarWatchlistForSlot(state_, sidebarSlot);
    const std::string fallbackTitle = sidebarSlot == 1 ? "Watchlist 1" : "Watchlist 2";
    const std::string title = assignedWatchlist == nullptr ? fallbackTitle : assignedWatchlist->name;
    const std::string panelId = "SidebarWatchlistSlot" + std::to_string(sidebarSlot) + "ColumnPanel";

    beginFinancePanel(panelId.c_str(), title.c_str(), ImVec2(0.0f, 246.0f), sidebarSlot == 1 ? UiTheme::NeonMagenta : UiTheme::ElectricCyan);

    if (assignedWatchlist == nullptr) {
        ImGui::TextColored(UiTheme::MutedText, "No watchlist selected.");
        ImGui::TextWrapped("Assign one from the Watchlist Manager.");
        endFinancePanel();
        return;
    }

    const std::vector<WatchlistItem> items = sidebarWatchlistItems(state_, assignedWatchlist->id, *technicalIndicatorService_);
    if (items.empty()) {
        ImGui::TextColored(UiTheme::MutedText, "No items.");
    } else {
        UiTheme::pushTableStyle();
        if (ImGui::BeginTable((panelId + "Table").c_str(), 3, ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerH | ImGuiTableFlags_SizingStretchProp)) {
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
                UiTheme::textRightAligned(currentPrice.c_str());
                ImGui::TableNextColumn();
                const WatchlistSignalResult signal = WatchlistSignalService::calculateSignal(item, cachedIndicatorsFor(*technicalIndicatorService_, item));
                UiTheme::badge(signal.signal.c_str(), sidebarSignalColor(signal));
                if (ImGui::IsItemHovered() && !signal.reasonText.empty()) {
                    ImGui::SetTooltip("%s", signal.reasonText.c_str());
                }
            }
            ImGui::EndTable();
        }
        UiTheme::popTableStyle();
    }

    endFinancePanel();
}

void App::renderSidebarFooter()
{
    beginFinancePanel("SidebarVersionDatabaseInfo", "Data Status", ImVec2(0.0f, 0.0f), UiTheme::Amber);
    compactMetric("Provider", marketDataService_ == nullptr ? "N/A" : marketDataService_->providerName(), UiTheme::ElectricCyan, false);
    compactMetric("Last Refresh", latestPriceRefreshText(state_), UiTheme::TextMuted, false);
    compactMetric("App Version", AppVersion, UiTheme::TextSecondary, false);
    compactMetric("SQLite Status", database_.handle() == nullptr ? "Disconnected" : "Connected", database_.handle() == nullptr ? UiTheme::TextDanger : UiTheme::TextSuccess, false);
    const std::string reminderText = DatabaseBackupService::reminderStatusText(state_.databaseBackupSettings, Date::todayIso8601());
    const bool backupDue = DatabaseBackupService::isReminderDue(state_.databaseBackupSettings, Date::todayIso8601());
    compactMetric("Backup", reminderText, backupDue ? UiTheme::Amber : UiTheme::TextMuted, false);
    UiTheme::pushButtonStyle(UiTheme::ElectricCyan);
    if (ImGui::Button("Back Up Now", ImVec2(120.0f, 0.0f))) {
        backupDatabaseNow();
    }
    UiTheme::popButtonStyle();
    if (state_.databaseBackupSettings.backupFolder.empty()) {
        ImGui::SameLine();
        ImGui::TextColored(UiTheme::TextWarning, "Set folder in Settings");
    }
    ImGui::TextColored(UiTheme::TextMuted, "Current Database Path");
    ImGui::TextWrapped("%s", activeDatabasePath_.c_str());
    if (isPathInsideRepository(activeDatabasePath_)) {
        ImGui::TextColored(UiTheme::TextWarning, "Database is inside the repository. Consider moving it outside Git.");
    }
    endFinancePanel();
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
        watchlistView_.render(state_, *watchlistRepository_, *marketDataService_, *technicalIndicatorService_, reload);
        break;
    case AppSection::ImportCsv:
        importCsvView_.render(state_, *csvImportService_, reload);
        break;
    case AppSection::StockResearch:
        stockResearchView_.render(state_, *marketDataService_, *technicalIndicatorService_, *watchlistRepository_, reload);
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
    UiTheme::pushPanelStyle();
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
    UiTheme::popPanelStyle();
}

void App::renderStatusBar()
{
    if (state_.statusMessage.empty()) {
        return;
    }

    ImGui::Separator();
    ImGui::TextColored(state_.statusIsError ? UiTheme::Loss : UiTheme::Gain, "%s", state_.statusMessage.c_str());
}
