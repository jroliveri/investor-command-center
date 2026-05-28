// SPDX-License-Identifier: MIT
#include "app/App.hpp"

#include "db/Migrations.hpp"
#include "ui/UiTheme.hpp"

#include <imgui.h>

namespace {

constexpr const char* DatabasePath = "data/investor_command_center.db";
constexpr const char* AppVersion = "0.3.0";
constexpr float SidebarWidth = 270.0f;

void sidebarGroup(const char* label)
{
    ImGui::Spacing();
    ImGui::TextColored(UiTheme::MutedText, "%s", label);
}

bool sidebarItem(AppState& state, AppSection section, const char* label)
{
    const bool selected = state.currentSection == section;
    if (ImGui::Selectable(label, selected, 0, ImVec2(0.0f, 32.0f))) {
        state.currentSection = section;
        state.clearStatus();
        return true;
    }
    return false;
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
    transactionRepository_ = std::make_unique<TransactionRepository>(database_);
    dividendRepository_ = std::make_unique<DividendRepository>(database_);
    goalRepository_ = std::make_unique<GoalRepository>(database_);
    watchlistRepository_ = std::make_unique<WatchlistRepository>(database_);
    reloadData();

    return true;
}

void App::render()
{
    UiTheme::apply();

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

    renderSidebar();
    ImGui::SameLine();

    ImGui::BeginChild("Content", ImVec2(0.0f, 0.0f), false);
    ImGui::Text("Investor Command Center");
    ImGui::TextColored(UiTheme::MutedText, "Long-term progress beats short-term noise.");
    ImGui::Separator();
    ImGui::Spacing();

    const float statusHeight = state_.statusMessage.empty() ? 0.0f : 38.0f;
    ImGui::BeginChild("PageContent", ImVec2(0.0f, -statusHeight), false);
    renderCurrentSection();
    ImGui::EndChild();

    renderStatusBar();

    ImGui::EndChild();
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
    }
}

void App::renderSidebar()
{
    ImGui::BeginChild("Sidebar", ImVec2(SidebarWidth, 0.0f), true);
    ImGui::SetWindowFontScale(1.1f);
    ImGui::Text("Investor Command Center");
    ImGui::SetWindowFontScale(1.0f);
    ImGui::Spacing();
    ImGui::TextColored(UiTheme::MutedText, "Local personal dashboard");
    ImGui::TextColored(UiTheme::MutedText, "v%s", AppVersion);
    ImGui::Separator();

    sidebarGroup("Overview");
    sidebarItem(state_, AppSection::Dashboard, "Dashboard");
    sidebarItem(state_, AppSection::Reports, "Reports");

    sidebarGroup("Records");
    sidebarItem(state_, AppSection::Accounts, "Accounts");
    sidebarItem(state_, AppSection::Holdings, "Holdings");
    sidebarItem(state_, AppSection::Transactions, "Transactions");
    sidebarItem(state_, AppSection::Dividends, "Dividends");

    sidebarGroup("Planning");
    sidebarItem(state_, AppSection::Goals, "Goals");
    sidebarItem(state_, AppSection::Watchlist, "Watchlist");

    sidebarGroup("System");
    sidebarItem(state_, AppSection::Settings, "Settings");

    ImGui::SetCursorPosY(ImGui::GetWindowHeight() - 96.0f);
    ImGui::Separator();
    ImGui::TextColored(UiTheme::MutedText, "SQLite");
    ImGui::TextColored(UiTheme::MutedText, "%s", DatabasePath);
    ImGui::TextColored(UiTheme::Amber, "Back up monthly");
    ImGui::EndChild();
}

void App::renderCurrentSection()
{
    const auto reload = [this]() {
        reloadData();
    };

    switch (state_.currentSection) {
    case AppSection::Dashboard:
        dashboardView_.render(state_);
        break;
    case AppSection::Accounts:
        accountsView_.render(state_, *accountRepository_, reload);
        break;
    case AppSection::Holdings:
        holdingsView_.render(state_, *holdingRepository_, reload);
        break;
    case AppSection::Transactions:
        transactionsView_.render(state_, *transactionRepository_, reload);
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
    case AppSection::Reports:
        renderPlaceholder("Reports", "Reports will summarize local records without advice or recommendations.");
        break;
    case AppSection::Settings:
        settingsView_.render(DatabasePath, AppVersion);
        break;
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
