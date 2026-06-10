// SPDX-License-Identifier: MIT
#include "app/App.hpp"

#include "app/SidebarModel.hpp"
#include "db/Migrations.hpp"
#include "services/DashboardService.hpp"
#include "services/WatchlistSignalService.hpp"
#include "ui/UiTheme.hpp"
#include "util/Date.hpp"

#include <imgui.h>

#include <algorithm>
#include <array>
#include <cctype>
#include <filesystem>
#include <optional>
#include <set>
#include <string>
#include <vector>

namespace {

constexpr const char* DatabasePath = "data/investor_command_center.db";
constexpr const char* AppVersion = "0.3.0";
constexpr float AccountColumnWidth = 340.0f;
constexpr float SidebarValueColumnX = 170.0f;
constexpr ImGuiWindowFlags SidebarCardFlags = ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse;
constexpr const char* ThemeSettingKey = "theme";
constexpr const char* DatabasePathSettingKey = "database.path";

void compactMetric(const char* label, const std::string& value, ImVec4 color, bool numericValue = true)
{
    ImGui::TextColored(UiTheme::MutedText, "%s", label);
    ImGui::SameLine(SidebarValueColumnX);
    ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + ImGui::GetContentRegionAvail().x);
    if (numericValue) {
        UiTheme::pushNumericFont();
    }
    ImGui::TextColored(color, "%s", value.c_str());
    if (numericValue) {
        UiTheme::popNumericFont();
    }
    ImGui::PopTextWrapPos();
}

void drawPanelChrome(const char* title, ImVec4 accent)
{
    const ImVec2 min = ImGui::GetWindowPos();
    const ImVec2 size = ImGui::GetWindowSize();
    const ImVec2 max(min.x + size.x, min.y + size.y);
    ImDrawList* drawList = ImGui::GetWindowDrawList();

    drawList->AddRectFilled(min, ImVec2(max.x, min.y + 36.0f), ImGui::GetColorU32(UiTheme::withAlpha(UiTheme::SurfaceElevated, 0.16f)), 14.0f, ImDrawFlags_RoundCornersTop);
    drawList->AddRectFilled(ImVec2(min.x + 13.0f, min.y + 9.0f), ImVec2(min.x + 58.0f, min.y + 11.0f), ImGui::GetColorU32(UiTheme::withAlpha(accent, 0.38f)), 2.0f);
    drawList->AddRect(min, max, ImGui::GetColorU32(UiTheme::withAlpha(accent, 0.10f)), 14.0f);

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

float sidebarRowHeight()
{
    return std::max(18.0f, ImGui::GetTextLineHeightWithSpacing());
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
        return "Tracked symbols and saved price signals.";
    case AppSection::ImportCsv:
        return "CSV import workflow for local portfolio updates.";
    case AppSection::StockResearch:
        return "Symbol research and explicit market data refreshes.";
    case AppSection::Reports:
        return "Local reporting workspace for portfolio records.";
    case AppSection::Settings:
        return "Application preferences and local data controls.";
    }

    return "Local-first portfolio intelligence.";
}

ImVec4 sidebarToneColor(const SidebarModel::MetricRow& metric)
{
    switch (metric.tone) {
    case SidebarModel::Tone::Primary:
        return UiTheme::TextPrimary;
    case SidebarModel::Tone::Success:
        return UiTheme::Gain;
    case SidebarModel::Tone::Warning:
        return UiTheme::Amber;
    case SidebarModel::Tone::Danger:
        return UiTheme::Loss;
    case SidebarModel::Tone::Money:
        return UiTheme::moneyColor(metric.toneValue);
    case SidebarModel::Tone::Secondary:
    default:
        return UiTheme::MutedText;
    }
}

SidebarModel::RefreshSummary sidebarRefreshSummary(const DashboardPriceRefreshStatus& status)
{
    return SidebarModel::RefreshSummary {
        status.hasRun,
        status.provider,
        status.lastRefreshedAt,
        status.refreshedSymbols,
        status.failedSymbols,
        status.cachedSymbols,
        status.warning,
    };
}

SidebarModel::RefreshSummary sidebarRefreshSummary(const WatchlistPriceRefreshStatus& status)
{
    return SidebarModel::RefreshSummary {
        status.hasRun,
        status.provider,
        status.lastRefreshedAt,
        status.refreshedSymbols,
        status.failedSymbols,
        status.cachedSymbols,
        status.warning,
    };
}

ImVec4 sidebarWatchlistSymbolColor(const std::string& signal)
{
    if (signal == "Buy Signal" || signal == "Buy") {
        return UiTheme::Gain;
    }
    if (signal == "Sell Signal" || signal == "Sell") {
        return UiTheme::Loss;
    }
    if (signal == "Check Signals") {
        return UiTheme::Amber;
    }
    return UiTheme::TextPrimary;
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
    marketPriceHistoryRepository_ = std::make_unique<MarketPriceHistoryRepository>(database_);
    marketQuoteCacheRepository_ = std::make_unique<MarketQuoteCacheRepository>(database_);
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
    if (!std::filesystem::exists(configuredAbsolutePath, filesystemError) ||
        filesystemError ||
        std::filesystem::is_directory(configuredAbsolutePath, filesystemError)) {
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

bool App::saveConfiguredDatabasePath(const std::string& configuredPath, std::string& message)
{
    message.clear();

    std::string normalizedPath;
    if (!configuredPath.empty()) {
        const std::filesystem::path absoluteConfiguredPath = absolutePath(configuredPath);
        std::error_code filesystemError;
        if (!std::filesystem::exists(absoluteConfiguredPath, filesystemError) ||
            filesystemError ||
            std::filesystem::is_directory(absoluteConfiguredPath, filesystemError)) {
            message = "Choose an existing SQLite database file, or clear the path to use the default database.";
            return false;
        }

        normalizedPath = absoluteConfiguredPath.string();
    }

    if (appSettingsRepository_ == nullptr) {
        message = "Application settings are not available yet.";
        return false;
    }

    std::string error;
    const std::filesystem::path defaultPath = absolutePath(DatabasePath);
    const bool activeIsDefault = !activeDatabasePath_.empty() && pathsEquivalent(activeDatabasePath_, defaultPath);
    if (!activeIsDefault && !writeDatabasePathSettingToFile(defaultPath.string(), normalizedPath, error)) {
        message = "Could not update the startup database pointer: " + error;
        return false;
    }

    if (!appSettingsRepository_->setString(DatabasePathSettingKey, normalizedPath, error)) {
        message = "Could not save the database path setting: " + error;
        return false;
    }

    message = normalizedPath.empty()
        ? "Default database path saved. Restart Investor Command Center to use the default database."
        : "Database path saved. Restart Investor Command Center to use: " + normalizedPath;
    return true;
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
    WatchlistPriceRefreshStatus refreshStatus = WatchlistSignalService::refreshPrices(state_.watchlist, *marketDataService_, *technicalIndicatorService_, *watchlistRepository_, error);
    reloadData();
    state_.watchlistPriceRefreshStatus = refreshStatus;
    navigateTo(AppSection::Watchlist);

    if (refreshStatus.refreshedSymbols > 0) {
        state_.setStatus("Watchlist prices refreshed: " + std::to_string(refreshStatus.refreshedSymbols) + " updated, " +
            std::to_string(refreshStatus.failedSymbols) + " failed.");
    } else {
        state_.setStatus(error.empty() ? "Watchlist price refresh did not update any symbols." : error, true);
    }
}

void App::renderTopMenuBar()
{
    ImGui::PushStyleColor(ImGuiCol_MenuBarBg, UiTheme::withAlpha(UiTheme::SurfaceMain, 0.96f));
    ImGui::PushStyleColor(ImGuiCol_PopupBg, UiTheme::withAlpha(UiTheme::GlassPanel, 0.98f));
    if (!ImGui::BeginMainMenuBar()) {
        ImGui::PopStyleColor(2);
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
        if (ImGui::MenuItem("Capital Gains Allocation Settings")) {
            navigateTo(AppSection::Settings);
            state_.setStatus("Capital gains allocation rules are managed in Settings.");
        }
        if (ImGui::MenuItem("Data Privacy / Local Data")) {
            showPrivacyPopup_ = true;
        }
        ImGui::BeginDisabled();
        ImGui::MenuItem("Backup");
        ImGui::EndDisabled();
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
    ImGui::PopStyleColor(2);
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
        { "Research", AppSection::StockResearch, true, false, nullptr },
        { "Import", AppSection::ImportCsv, true, false, nullptr },
        { "Reports", AppSection::Reports, true, false, nullptr },
        { "Settings", AppSection::Settings, true, false, nullptr },
        { "Analytics", AppSection::Dashboard, false, false, "Analytics is currently shown inside Dashboard." },
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
    UiTheme::pushPanelStyle();
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(10.0f, 10.0f));
    ImGui::BeginChild("AccountColumn", ImVec2(AccountColumnWidth, 0.0f), true);
    ImGui::PopStyleVar();
    renderPortfolioSummaryCard();
    ImGui::Spacing();
    renderWatchlistPanel();
    ImGui::Spacing();
    renderDataStatusCard();
    ImGui::EndChild();
    UiTheme::popPanelStyle();
}

void App::renderPortfolioSummaryCard()
{
    const std::string today = Date::todayIso8601();
    const DashboardData data = DashboardService::build(state_, today, Date::currentMonthPrefix(), Date::currentYearPrefix());
    const std::vector<Holding> holdings = DashboardService::holdingsWithDashboardPrices(state_);
    const SidebarModel::PortfolioSummaryCard card = SidebarModel::buildPortfolioSummaryCard(
        data.portfolio,
        data.realizedGains,
        data.performance,
        state_.accounts,
        holdings);
    const int visibleAccountCount = static_cast<int>(card.accounts.size());
    const bool hasMoreAccounts = card.hiddenAccountCount > 0;
    const float rowHeight = sidebarRowHeight();
    const float cardHeight = 224.0f +
        static_cast<float>(std::max(visibleAccountCount, 1)) * rowHeight +
        (hasMoreAccounts ? rowHeight + 6.0f : 0.0f);

    beginFinancePanel("PortfolioSummaryCard", card.title.c_str(), ImVec2(0.0f, cardHeight), UiTheme::ElectricCyan, SidebarCardFlags);
    ImGui::SetWindowFontScale(1.18f);
    UiTheme::pushNumericFont();
    ImGui::TextColored(UiTheme::TextPrimary, "%s", card.totalValue.c_str());
    UiTheme::popNumericFont();
    ImGui::SetWindowFontScale(1.0f);
    ImGui::TextColored(UiTheme::MutedText, "%s", card.subtitle.c_str());
    ImGui::Spacing();

    for (const SidebarModel::MetricRow& metric : card.metrics) {
        compactMetric(metric.label.c_str(), metric.value, sidebarToneColor(metric));
    }

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::TextColored(UiTheme::Accent, "Accounts");

    if (card.accounts.empty()) {
        ImGui::TextColored(UiTheme::MutedText, "No accounts yet.");
    } else {
        for (const SidebarModel::AccountRow& row : card.accounts) {
            const std::string fullLabel = row.showStatus ? row.name + " (" + row.status + ")" : row.name;
            const std::string label = SidebarModel::truncate(fullLabel, 20);
            ImGui::TextColored(row.showStatus ? UiTheme::TextSecondary : UiTheme::TextPrimary, "%s", label.c_str());
            if (ImGui::IsItemHovered() && label != fullLabel) {
                ImGui::SetTooltip("%s", fullLabel.c_str());
            }
            ImGui::SameLine(SidebarValueColumnX);
            ImGui::TextColored(row.showStatus ? UiTheme::MutedText : UiTheme::TextPrimary, "%s", row.value.c_str());
        }

        if (hasMoreAccounts) {
            ImGui::TextColored(UiTheme::MutedText, "+%d more", card.hiddenAccountCount);
            ImGui::SameLine();
            if (ImGui::SmallButton("View all accounts")) {
                navigateTo(AppSection::Accounts);
            }
        }
    }

    endFinancePanel();
}

void App::renderWatchlistPanel()
{
    std::vector<Watchlist> sidebarWatchlists;
    for (const Watchlist& watchlist : state_.watchlists) {
        if (watchlist.isActive && watchlist.showInSidebar) {
            sidebarWatchlists.push_back(watchlist);
        }
    }
    std::sort(sidebarWatchlists.begin(), sidebarWatchlists.end(), [](const Watchlist& left, const Watchlist& right) {
        if (left.sidebarSlot != right.sidebarSlot) {
            return left.sidebarSlot < right.sidebarSlot;
        }
        if (left.sortOrder != right.sortOrder) {
            return left.sortOrder < right.sortOrder;
        }
        return left.name < right.name;
    });

    if (sidebarWatchlists.empty()) {
        for (const Watchlist& watchlist : state_.watchlists) {
            if (watchlist.isActive) {
                sidebarWatchlists.push_back(watchlist);
            }
        }
    }

    std::vector<SidebarModel::WatchlistGroup> groups;
    for (const Watchlist& watchlist : sidebarWatchlists) {
        SidebarModel::WatchlistGroup group;
        group.name = watchlist.name;
        for (const WatchlistItem& item : state_.watchlist) {
            if (item.watchlistId == watchlist.id) {
                group.items.push_back(item);
            }
        }
        groups.push_back(group);
    }

    if (groups.empty()) {
        groups = SidebarModel::defaultWatchlistGroups(state_.watchlist);
    }

    std::vector<MarketQuote> cachedQuotes;
    cachedQuotes.reserve(state_.watchlist.size());
    for (const WatchlistItem& item : state_.watchlist) {
        std::string quoteError;
        if (std::optional<MarketQuote> quote = marketDataService_->cachedQuote(item.ticker, quoteError)) {
            cachedQuotes.push_back(*quote);
        }
    }

    selectedSidebarWatchlistTab_ = std::clamp(selectedSidebarWatchlistTab_, 0, static_cast<int>(groups.size()) - 1);
    SidebarModel::WatchlistsCard card = SidebarModel::buildWatchlistsCard(groups, selectedSidebarWatchlistTab_, cachedQuotes);
    const int initialVisibleSymbolCount = static_cast<int>(card.rows.size());
    const bool initialHasMoreSymbols = card.hiddenSymbolCount > 0;
    const float rowHeight = sidebarRowHeight();
    const float cardHeight = 126.0f +
        static_cast<float>(std::max(initialVisibleSymbolCount, 1)) * rowHeight +
        (initialHasMoreSymbols ? rowHeight : 0.0f);

    beginFinancePanel("SidebarWatchlistsCard", card.title.c_str(), ImVec2(0.0f, cardHeight), UiTheme::NeonMagenta, SidebarCardFlags);

    const float tabLineRight = ImGui::GetCursorPosX() + ImGui::GetContentRegionAvail().x;
    for (int index = 0; index < static_cast<int>(card.tabs.size()); ++index) {
        const std::string displayName = SidebarModel::truncate(card.tabs[static_cast<std::size_t>(index)], 12);
        const std::string label = displayName + "##sidebar_watchlist_tab_" + std::to_string(index);
        const float tabWidth = ImGui::CalcTextSize(displayName.c_str()).x + ImGui::GetStyle().FramePadding.x * 2.0f;
        if (index > 0 && ImGui::GetCursorPosX() + ImGui::GetStyle().ItemSpacing.x + tabWidth <= tabLineRight) {
            ImGui::SameLine();
        }

        const bool selected = index == selectedSidebarWatchlistTab_;
        if (selected) {
            ImGui::PushStyleColor(ImGuiCol_Button, UiTheme::withAlpha(UiTheme::NeonMagenta, 0.20f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, UiTheme::withAlpha(UiTheme::NeonMagenta, 0.28f));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, UiTheme::withAlpha(UiTheme::NeonMagenta, 0.34f));
            ImGui::PushStyleColor(ImGuiCol_Border, UiTheme::withAlpha(UiTheme::NeonMagenta, 0.35f));
        }
        if (ImGui::SmallButton(label.c_str())) {
            selectedSidebarWatchlistTab_ = index;
        }
        if (selected) {
            ImGui::PopStyleColor(4);
        }
    }

    const float createTabWidth = ImGui::CalcTextSize("+").x + ImGui::GetStyle().FramePadding.x * 2.0f;
    if (ImGui::GetCursorPosX() + ImGui::GetStyle().ItemSpacing.x + createTabWidth <= tabLineRight) {
        ImGui::SameLine();
    }
    ImGui::BeginDisabled();
        ImGui::SmallButton("+##sidebar_watchlist_create_unavailable");
    ImGui::EndDisabled();
    ImGui::Spacing();

    card = SidebarModel::buildWatchlistsCard(groups, selectedSidebarWatchlistTab_, cachedQuotes);

    if (card.empty) {
        ImGui::TextColored(UiTheme::MutedText, "%s", card.emptyText.c_str());
    } else if (ImGui::BeginTable("SidebarWatchlistsRows", 3, ImGuiTableFlags_SizingStretchProp)) {
        ImGui::TableSetupColumn("Symbol", ImGuiTableColumnFlags_WidthStretch, 1.2f);
        ImGui::TableSetupColumn("Price", ImGuiTableColumnFlags_WidthFixed, 84.0f);
        ImGui::TableSetupColumn("Change", ImGuiTableColumnFlags_WidthFixed, 70.0f);

        for (const SidebarModel::WatchlistRow& row : card.rows) {
            const std::string symbol = SidebarModel::truncate(row.symbol, 10);
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::TextColored(sidebarWatchlistSymbolColor(row.signalStatus), "%s", symbol.c_str());
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("%s", row.signalStatus.c_str());
            }
            ImGui::TableNextColumn();
            ImGui::TextColored(UiTheme::TextPrimary, "%s", SidebarModel::truncate(row.price, 11).c_str());
            ImGui::TableNextColumn();
            ImGui::TextColored(row.change.rfind("-", 0) == 0 ? UiTheme::Loss : (row.change.rfind("+", 0) == 0 ? UiTheme::Gain : UiTheme::MutedText), "%s", SidebarModel::truncate(row.change, 10).c_str());
        }
        ImGui::EndTable();

        if (card.hiddenSymbolCount > 0) {
            ImGui::TextColored(UiTheme::MutedText, "+%d more", card.hiddenSymbolCount);
        }
    }

    ImGui::Spacing();
    if (ImGui::SmallButton("View full watchlist")) {
        navigateTo(AppSection::Watchlist);
    }

    endFinancePanel();
}

void App::renderDataStatusCard()
{
    const SidebarModel::DataStatusCard card = SidebarModel::buildDataStatusCard(
        sidebarRefreshSummary(state_.dashboardPriceRefreshStatus),
        sidebarRefreshSummary(state_.watchlistPriceRefreshStatus),
        marketDataService_ == nullptr ? "Yahoo Finance" : marketDataService_->providerName(),
        AppVersion,
        startupError_,
        Date::todayIso8601());
    const float cardHeight = card.abnormal ? 152.0f : 112.0f;

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8.0f, 6.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(6.0f, 3.0f));
    beginFinancePanel("SidebarDataStatusCard", card.title.c_str(), ImVec2(0.0f, cardHeight), UiTheme::Amber, SidebarCardFlags);

    ImGui::TextColored(card.databaseIssue ? UiTheme::Loss : UiTheme::TextSecondary, "%s", card.connectionLine.c_str());
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("SQLite: %s", activeDatabasePath_.empty() ? DatabasePath : activeDatabasePath_.c_str());
    }

    ImGui::TextColored(card.refreshIssue ? UiTheme::Amber : UiTheme::MutedText, "%s", card.lastRefreshLine.c_str());
    ImGui::TextColored(UiTheme::MutedText, "%s", card.versionLine.c_str());

    if (card.databaseIssue) {
        ImGui::TextColored(UiTheme::Loss, "%s", SidebarModel::truncate(card.issueDetail, 44).c_str());
    } else if (card.refreshIssue) {
        ImGui::TextColored(card.issueSummary.find("failed") != std::string::npos ? UiTheme::Loss : UiTheme::Amber, "%s", card.issueSummary.c_str());
        if (!card.issueDetail.empty()) {
            ImGui::TextColored(UiTheme::MutedText, "%s", card.issueDetail.c_str());
        }
    }

    endFinancePanel();
    ImGui::PopStyleVar(2);
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
        stockResearchView_.render(state_, *marketDataService_, *watchlistRepository_, reload);
        break;
    case AppSection::Reports:
        renderPlaceholder("Reports", "Reports will summarize local records without advice or recommendations.");
        break;
    case AppSection::Settings:
        settingsView_.render(state_, *appSettingsRepository_, *capitalGainAllocationRepository_, activeDatabasePath_, AppVersion, [this](const std::string& configuredPath, std::string& message) {
            return saveConfiguredDatabasePath(configuredPath, message);
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
    endFinancePanel();
}

void App::renderStatusBar()
{
    if (state_.statusMessage.empty()) {
        return;
    }

    ImGui::Separator();
    ImGui::TextColored(state_.statusIsError ? UiTheme::Loss : UiTheme::Gain, "%s", state_.statusMessage.c_str());
}
