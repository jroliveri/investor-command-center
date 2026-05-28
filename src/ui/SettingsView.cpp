// SPDX-License-Identifier: MIT
#include "ui/SettingsView.hpp"

#include "ui/UiTheme.hpp"

#include <imgui.h>

#include <filesystem>

namespace {

void disabledAction(const char* label, const char* note)
{
    ImGui::BeginDisabled();
    ImGui::Button(label, ImVec2(180.0f, 0.0f));
    ImGui::EndDisabled();
    ImGui::SameLine();
    ImGui::TextColored(UiTheme::MutedText, "%s", note);
}

}

void SettingsView::render(const std::string& databasePath, const char* appVersion)
{
    const std::string absoluteDatabasePath = std::filesystem::absolute(databasePath).string();

    UiTheme::sectionHeading("Settings", "Local storage, privacy, exports, and maintenance reminders.");

    const float availableWidth = ImGui::GetContentRegionAvail().x;
    const float gap = ImGui::GetStyle().ItemSpacing.x;
    const float panelWidth = (availableWidth - gap) / 2.0f;

    ImGui::BeginChild("AppInfo", ImVec2(panelWidth, 210.0f), true);
    ImGui::Text("Application");
    ImGui::Separator();
    ImGui::TextColored(UiTheme::MutedText, "Name");
    ImGui::SameLine(160.0f);
    ImGui::Text("Investor Command Center");
    ImGui::TextColored(UiTheme::MutedText, "Version");
    ImGui::SameLine(160.0f);
    ImGui::Text("%s", appVersion);
    ImGui::TextColored(UiTheme::MutedText, "Storage");
    ImGui::SameLine(160.0f);
    ImGui::Text("Local SQLite");
    ImGui::Spacing();
    ImGui::TextWrapped("Long-term progress beats short-term noise.");
    ImGui::EndChild();

    ImGui::SameLine();

    ImGui::BeginChild("DatabaseLocation", ImVec2(panelWidth, 210.0f), true);
    ImGui::Text("Database Location");
    ImGui::Separator();
    ImGui::TextWrapped("%s", absoluteDatabasePath.c_str());
    ImGui::Spacing();
    ImGui::TextColored(UiTheme::MutedText, "The app reads and writes this local database file. Move or back it up only while the app is closed.");
    ImGui::EndChild();

    ImGui::Spacing();

    ImGui::BeginChild("Privacy", ImVec2(panelWidth, 220.0f), true);
    ImGui::Text("Data Privacy");
    ImGui::Separator();
    ImGui::TextWrapped("Investor Command Center stores records locally and does not connect to brokerage accounts, cloud services, or stock price APIs.");
    ImGui::Spacing();
    ImGui::TextColored(UiTheme::Gain, "Local-first");
    ImGui::TextColored(UiTheme::MutedText, "No login");
    ImGui::TextColored(UiTheme::MutedText, "No cloud sync");
    ImGui::TextColored(UiTheme::MutedText, "No external market data calls");
    ImGui::EndChild();

    ImGui::SameLine();

    ImGui::BeginChild("BackupReminder", ImVec2(panelWidth, 220.0f), true);
    ImGui::Text("Backup Reminder");
    ImGui::Separator();
    ImGui::TextWrapped("Make a periodic copy of the SQLite database to a location you control. A monthly backup is a good default for manual personal tracking.");
    ImGui::Spacing();
    ImGui::TextColored(UiTheme::Amber, "Suggested cadence: monthly");
    ImGui::TextColored(UiTheme::MutedText, "Close the app before copying the database file.");
    ImGui::EndChild();

    ImGui::Spacing();

    ImGui::BeginChild("CsvExports", ImVec2(0.0f, 0.0f), true);
    ImGui::Text("CSV Export Placeholders");
    ImGui::Separator();
    disabledAction("Export Accounts CSV", "Planned local file export");
    disabledAction("Export Holdings CSV", "Planned local file export");
    disabledAction("Export Transactions CSV", "Planned local file export");
    disabledAction("Export Dividends CSV", "Planned local file export");
    disabledAction("Export Goals CSV", "Planned local file export");
    disabledAction("Export Watchlist CSV", "Planned local file export");
    ImGui::EndChild();
}
