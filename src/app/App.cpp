// SPDX-License-Identifier: MIT
#include "app/App.hpp"

#include "db/Migrations.hpp"
#include "services/DashboardService.hpp"
#include "services/PortfolioCalculator.hpp"
#include "ui/UiTheme.hpp"
#include "util/Date.hpp"
#include "util/Money.hpp"

#include <imgui.h>

#include <algorithm>

namespace {

constexpr const char* DatabasePath = "data/investor_command_center.db";
constexpr const char* AppVersion = "0.3.0";
constexpr float AccountColumnWidth = 340.0f;
constexpr const char* ThemeSettingKey = "theme";

void compactMetric(const char* label, const std::string& value, ImVec4 color)
{
    ImGui::TextColored(UiTheme::MutedText, "%s", label);
    ImGui::SameLine(170.0f);
    ImGui::TextColored(color, "%s", value.c_str());
}

}

bool App::initialize()
{
    if (!database_.open(DatabasePath)) {
        startupError_ = database_.lastError();
        return false;
    }

    std::string migrationError;
    if (!Migrations::run(database_, migrationError)) {
        startupError_ = migrationError;
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
    portfolioSnapshotRepository_ = std::make_unique<PortfolioSnapshotRepository>(database_);
    dashboardLayoutRepository_ = std::make_unique<DashboardLayoutRepository>(database_);
    dashboardChartSettingsRepository_ = std::make_unique<DashboardChartSettingsRepository>(database_);
    appSettingsRepository_ = std::make_unique<AppSettingsRepository>(database_);
    capitalGainAllocationRepository_ = std::make_unique<CapitalGainAllocationRepository>(database_);
    csvImportService_ = std::make_unique<CsvImportService>(database_, *holdingRepository_, *importBatchRepository_, *portfolioSnapshotRepository_);

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

    return true;
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
    ImGui::BeginChild("AccountColumn", ImVec2(AccountColumnWidth, 0.0f), true);
    ImGui::SetWindowFontScale(1.08f);
    ImGui::Text("Investor Command Center");
    ImGui::SetWindowFontScale(1.0f);
    ImGui::TextColored(UiTheme::MutedText, "Portfolio terminal - v%s", AppVersion);
    ImGui::TextColored(UiTheme::Accent, "Current page: %s", sectionTitle(state_.currentSection));
    ImGui::Separator();
    ImGui::TextColored(UiTheme::MutedText, "Use the top menu bar for navigation.");
    ImGui::Spacing();
    renderAccountInfoPanel();
    renderAccountsPanel();
    renderWatchlistPanel();

    ImGui::Separator();
    ImGui::TextColored(UiTheme::MutedText, "SQLite");
    ImGui::TextColored(UiTheme::MutedText, "%s", DatabasePath);
    ImGui::TextColored(UiTheme::Amber, "Local data. Back up monthly.");
    ImGui::EndChild();
}

void App::renderAccountInfoPanel()
{
    const std::string today = Date::todayIso8601();
    const DashboardData data = DashboardService::build(state_, today, Date::currentMonthPrefix(), Date::currentYearPrefix());
    const PortfolioSnapshot* latestSnapshot = DashboardService::latestSnapshot(state_);
    const ImportBatch* latestImport = DashboardService::latestImportBatch(state_);

    ImGui::BeginChild("AccountInfoPanel", ImVec2(0.0f, 210.0f), true);
    ImGui::TextColored(UiTheme::Accent, "Account Info");
    ImGui::Separator();
    compactMetric("Total Portfolio", Money::format(data.portfolio.accountBalance), UiTheme::Gain);
    compactMetric("Holdings Value", Money::format(data.portfolio.holdingsMarketValue), UiTheme::Gain);
    compactMetric("Cash Balance", Money::format(data.portfolio.cashBalance), UiTheme::Amber);
    compactMetric("Unrealized G/L", Money::format(data.portfolio.gainLossDollar), UiTheme::moneyColor(data.portfolio.gainLossDollar));
    compactMetric("Realized G/L YTD", Money::format(data.realizedGains.thisYear), UiTheme::moneyColor(data.realizedGains.thisYear));
    compactMetric("Last CSV Import", latestImport == nullptr ? "None" : latestImport->importDate, UiTheme::MutedText);
    compactMetric("Last Snapshot", latestSnapshot == nullptr ? "None" : latestSnapshot->snapshotDate, UiTheme::MutedText);
    ImGui::EndChild();
}

void App::renderAccountsPanel()
{
    ImGui::BeginChild("AccountsColumnPanel", ImVec2(0.0f, 180.0f), true);
    ImGui::TextColored(UiTheme::Accent, "Accounts");
    ImGui::Separator();

    if (state_.accounts.empty()) {
        ImGui::TextColored(UiTheme::MutedText, "No accounts yet.");
    } else if (ImGui::BeginTable("AccountColumnTable", 3, ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerH | ImGuiTableFlags_SizingStretchProp)) {
        ImGui::TableSetupColumn("Account");
        ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthFixed, 92.0f);
        ImGui::TableSetupColumn("Cash", ImGuiTableColumnFlags_WidthFixed, 78.0f);
        ImGui::TableHeadersRow();

        for (const Account& account : state_.accounts) {
            const AccountMetrics metrics = PortfolioCalculator::calculateAccount(account, state_.holdings);
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::Text("%s", account.accountName.c_str());
            ImGui::TableNextColumn();
            ImGui::Text("%s", Money::format(metrics.calculatedBalance).c_str());
            ImGui::TableNextColumn();
            ImGui::TextColored(account.status == "Active" ? UiTheme::Amber : UiTheme::MutedText, "%s", Money::format(account.cashBalance).c_str());
        }
        ImGui::EndTable();
    }

    ImGui::EndChild();
}

void App::renderWatchlistPanel()
{
    ImGui::BeginChild("WatchlistColumnPanel", ImVec2(0.0f, 220.0f), true);
    ImGui::TextColored(UiTheme::Accent, "Watchlist / Quick Symbols");
    ImGui::Separator();

    if (state_.watchlist.empty()) {
        ImGui::TextColored(UiTheme::MutedText, "No watchlist items.");
    } else if (ImGui::BeginTable("WatchlistColumnTable", 4, ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerH | ImGuiTableFlags_SizingStretchProp)) {
        ImGui::TableSetupColumn("Ticker", ImGuiTableColumnFlags_WidthFixed, 66.0f);
        ImGui::TableSetupColumn("Price", ImGuiTableColumnFlags_WidthFixed, 78.0f);
        ImGui::TableSetupColumn("Change", ImGuiTableColumnFlags_WidthFixed, 62.0f);
        ImGui::TableSetupColumn("Priority");
        ImGui::TableHeadersRow();

        const int limit = std::min<int>(12, static_cast<int>(state_.watchlist.size()));
        for (int index = 0; index < limit; ++index) {
            const WatchlistItem& item = state_.watchlist[static_cast<std::size_t>(index)];
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::Text("%s", item.ticker.c_str());
            ImGui::TableNextColumn();
            ImGui::Text("%s", Money::format(item.currentPrice).c_str());
            ImGui::TableNextColumn();
            ImGui::TextColored(UiTheme::MutedText, "--");
            ImGui::TableNextColumn();
            ImGui::TextColored(item.priority == "High" ? UiTheme::Amber : UiTheme::MutedText, "%s", item.priority.c_str());
        }
        ImGui::EndTable();
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
        dashboardView_.render(state_, *portfolioSnapshotRepository_, *dashboardLayoutRepository_, *dashboardChartSettingsRepository_, reload);
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
        watchlistView_.render(state_, *watchlistRepository_, reload);
        break;
    case AppSection::ImportCsv:
        importCsvView_.render(state_, *csvImportService_, reload);
        break;
    case AppSection::Reports:
        renderPlaceholder("Reports", "Reports will summarize local records without advice or recommendations.");
        break;
    case AppSection::Settings:
        settingsView_.render(state_, *appSettingsRepository_, *capitalGainAllocationRepository_, DatabasePath, AppVersion, reload);
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
        ImGui::TextWrapped("Investor Command Center stores personal investing data in a local SQLite database. It does not connect to brokerage accounts, cloud services, or stock price APIs.");
        ImGui::Spacing();
        ImGui::TextColored(UiTheme::Amber, "Do not commit databases, CSV files, exports, backups, or logs.");
        ImGui::TextColored(UiTheme::MutedText, "This app is for personal tracking and does not provide financial advice.");
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
