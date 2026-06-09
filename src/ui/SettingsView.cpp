// SPDX-License-Identifier: MIT
#include "ui/SettingsView.hpp"

#include "app/AppState.hpp"
#include "repositories/AppSettingsRepository.hpp"
#include "repositories/CapitalGainAllocationRepository.hpp"
#include "services/DatabaseBackupService.hpp"
#include "ui/UiTheme.hpp"
#include "util/Date.hpp"
#include "util/Money.hpp"

#include <imgui.h>
#include <imgui_stdlib.h>

#ifdef _WIN32
#include <objbase.h>
#include <shellapi.h>
#include <shobjidl.h>
#include <windows.h>
#endif

#include <algorithm>
#include <cfloat>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>

namespace {

constexpr const char* BackupFolderSettingKey = "database.backup_folder";
constexpr const char* BackupReminderEnabledSettingKey = "database.backup_reminder_enabled";
constexpr const char* BackupReminderFrequencySettingKey = "database.backup_reminder_frequency";

void disabledAction(const char* label, const char* note)
{
    ImGui::BeginDisabled();
    UiTheme::pushButtonStyle(UiTheme::TextMuted);
    ImGui::Button(label, ImVec2(180.0f, 0.0f));
    UiTheme::popButtonStyle();
    ImGui::EndDisabled();
    ImGui::SameLine();
    ImGui::TextColored(UiTheme::MutedText, "%s", note);
}

double activeAllocationTotal(const std::vector<CapitalGainAllocationRule>& rules)
{
    double total = 0.0;
    for (const CapitalGainAllocationRule& rule : rules) {
        if (rule.isActive) {
            total += rule.percentage;
        }
    }
    return total;
}

int nextSortOrder(const std::vector<CapitalGainAllocationRule>& rules)
{
    int next = 1;
    for (const CapitalGainAllocationRule& rule : rules) {
        next = std::max(next, rule.sortOrder + 1);
    }
    return next;
}

bool saveDatabaseBackupSettings(AppState& state, AppSettingsRepository& settingsRepository, std::string& error)
{
    error.clear();
    state.databaseBackupSettings.reminderFrequency = DatabaseBackupService::normalizeFrequency(state.databaseBackupSettings.reminderFrequency);

    if (!settingsRepository.setString(BackupFolderSettingKey, state.databaseBackupSettings.backupFolder, error)) {
        return false;
    }
    if (!settingsRepository.setString(BackupReminderEnabledSettingKey, state.databaseBackupSettings.reminderEnabled ? "1" : "0", error)) {
        return false;
    }
    if (!settingsRepository.setString(BackupReminderFrequencySettingKey, state.databaseBackupSettings.reminderFrequency, error)) {
        return false;
    }
    return true;
}

std::filesystem::path absolutePath(const std::string& path)
{
    std::error_code error;
    const std::filesystem::path absolute = std::filesystem::absolute(std::filesystem::path(path), error);
    return error ? std::filesystem::path(path) : absolute.lexically_normal();
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

std::optional<std::string> chooseFolder(const wchar_t* title, std::string& error)
{
    error.clear();

#ifdef _WIN32
    const HRESULT initializeResult = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
    const bool shouldUninitialize = SUCCEEDED(initializeResult);
    if (FAILED(initializeResult) && initializeResult != RPC_E_CHANGED_MODE) {
        error = "Could not initialize the Windows folder picker.";
        return std::nullopt;
    }

    IFileDialog* dialog = nullptr;
    HRESULT result = CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&dialog));
    if (FAILED(result) || dialog == nullptr) {
        if (shouldUninitialize) {
            CoUninitialize();
        }
        error = "Could not open the Windows folder picker.";
        return std::nullopt;
    }

    DWORD options = 0;
    dialog->GetOptions(&options);
    dialog->SetOptions(options | FOS_PICKFOLDERS | FOS_FORCEFILESYSTEM);
    dialog->SetTitle(title);

    result = dialog->Show(nullptr);
    if (result == HRESULT_FROM_WIN32(ERROR_CANCELLED)) {
        dialog->Release();
        if (shouldUninitialize) {
            CoUninitialize();
        }
        return std::nullopt;
    }
    if (FAILED(result)) {
        dialog->Release();
        if (shouldUninitialize) {
            CoUninitialize();
        }
        error = "Could not choose a backup folder.";
        return std::nullopt;
    }

    IShellItem* selectedItem = nullptr;
    result = dialog->GetResult(&selectedItem);
    if (FAILED(result) || selectedItem == nullptr) {
        dialog->Release();
        if (shouldUninitialize) {
            CoUninitialize();
        }
        error = "Could not read the selected backup folder.";
        return std::nullopt;
    }

    PWSTR selectedPath = nullptr;
    result = selectedItem->GetDisplayName(SIGDN_FILESYSPATH, &selectedPath);
    if (FAILED(result) || selectedPath == nullptr) {
        selectedItem->Release();
        dialog->Release();
        if (shouldUninitialize) {
            CoUninitialize();
        }
        error = "Could not read the selected backup folder path.";
        return std::nullopt;
    }

    const std::filesystem::path folderPath(selectedPath);
    CoTaskMemFree(selectedPath);
    selectedItem->Release();
    dialog->Release();
    if (shouldUninitialize) {
        CoUninitialize();
    }
    return folderPath.string();
#else
    error = "Folder picker is only available in the Windows desktop build.";
    return std::nullopt;
#endif
}

void openFolderInExplorer(const std::string& folder, std::string& error)
{
    error.clear();

#ifdef _WIN32
    const std::filesystem::path path = absolutePath(folder);
    const HINSTANCE result = ShellExecuteW(nullptr, L"open", path.wstring().c_str(), nullptr, nullptr, SW_SHOWNORMAL);
    if (reinterpret_cast<intptr_t>(result) <= 32) {
        error = "Could not open the folder in Windows Explorer.";
    }
#else
    error = "Open folder is only available in the Windows desktop build.";
#endif
}

}

void SettingsView::render(AppState& state,
    AppSettingsRepository& settingsRepository,
    CapitalGainAllocationRepository& allocationRepository,
    const std::string& activeDatabasePath,
    const char* appVersion,
    const std::function<void()>& backupNow,
    const std::function<bool(const std::string&, std::string&)>& moveDatabaseToFolder,
    const std::function<void()>& reloadData)
{
    UiTheme::sectionHeading("Settings", "Local storage, privacy, exports, and maintenance reminders.");

    const float availableWidth = ImGui::GetContentRegionAvail().x;
    const float gap = ImGui::GetStyle().ItemSpacing.x;
    const float panelWidth = (availableWidth - gap) / 2.0f;

    UiTheme::pushPanelStyle();
    ImGui::BeginChild("AppInfo", ImVec2(panelWidth, 260.0f), true);
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
    ImGui::Spacing();
    const UiTheme::ThemeMode activeTheme = UiTheme::themeFromKey(state.themeKey);
    if (ImGui::BeginCombo("Theme", UiTheme::themeName(activeTheme))) {
        const UiTheme::ThemeMode themes[] = {
            UiTheme::ThemeMode::DarkCommandCenter,
            UiTheme::ThemeMode::LightTradingTerminal,
        };
        for (UiTheme::ThemeMode theme : themes) {
            const bool selected = activeTheme == theme;
            if (ImGui::Selectable(UiTheme::themeName(theme), selected)) {
                std::string error;
                const std::string key = UiTheme::themeKey(theme);
                if (settingsRepository.setString("theme", key, error)) {
                    state.themeKey = key;
                    state.setStatus("Theme preference saved locally.");
                } else {
                    state.setStatus("Could not save theme preference: " + error, true);
                }
            }
            if (selected) {
                ImGui::SetItemDefaultFocus();
            }
        }
        ImGui::EndCombo();
    }
    ImGui::TextColored(UiTheme::MutedText, "Theme preference is stored locally in SQLite.");
    ImGui::EndChild();
    UiTheme::popPanelStyle();

    ImGui::SameLine();

    renderDatabaseLocation(activeDatabasePath, moveDatabaseToFolder);

    ImGui::Spacing();

    UiTheme::pushPanelStyle();
    ImGui::BeginChild("Privacy", ImVec2(panelWidth, 280.0f), true);
    ImGui::Text("Data Privacy");
    ImGui::Separator();
    ImGui::TextWrapped("Investor Command Center stores records locally and does not connect to brokerage accounts or cloud services. Stock Research fetches informational market data only when explicitly requested.");
    ImGui::Spacing();
    ImGui::TextColored(UiTheme::Gain, "Local-first");
    ImGui::TextColored(UiTheme::MutedText, "No login");
    ImGui::TextColored(UiTheme::MutedText, "No cloud sync");
    ImGui::TextColored(UiTheme::MutedText, "No automatic market data refresh");
    ImGui::EndChild();
    UiTheme::popPanelStyle();

    ImGui::SameLine();

    renderDatabaseBackup(state, settingsRepository, backupNow);

    ImGui::Spacing();

    UiTheme::pushPanelStyle();
    ImGui::BeginChild("ResearchSettings", ImVec2(0.0f, 190.0f), true);
    ImGui::Text("Research Settings");
    ImGui::Separator();
    ImGui::TextColored(UiTheme::MutedText, "Data source");
    ImGui::SameLine(180.0f);
    ImGui::Text("Yahoo Finance");
    ImGui::TextColored(UiTheme::MutedText, "Dashboard price refresh");
    ImGui::SameLine(180.0f);
    ImGui::Text("Off");
    ImGui::TextColored(UiTheme::MutedText, "Refresh interval");
    ImGui::SameLine(180.0f);
    ImGui::Text("Manual only");
    ImGui::TextColored(UiTheme::MutedText, "Update holdings current_price");
    ImGui::SameLine(180.0f);
    ImGui::Text("Off");
    ImGui::Spacing();
    ImGui::TextWrapped("Research data is informational, may be delayed or unavailable, and is not financial advice. CSV import remains the primary portfolio update workflow. Dashboard price refreshes are display-only in this pass.");
    ImGui::TextColored(UiTheme::MutedText, "Yahoo Finance quote cache metadata is stored locally for convenience only.");
    ImGui::EndChild();
    UiTheme::popPanelStyle();

    ImGui::Spacing();
    renderCapitalGainAllocation(state, allocationRepository, reloadData);

    ImGui::Spacing();

    UiTheme::pushPanelStyle();
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
    UiTheme::popPanelStyle();
}

void SettingsView::renderDatabaseLocation(const std::string& activeDatabasePath, const std::function<bool(const std::string&, std::string&)>& moveDatabaseToFolder)
{
    const std::filesystem::path activePath = absolutePath(activeDatabasePath);
    const std::filesystem::path activeFolder = activePath.has_parent_path() ? activePath.parent_path() : std::filesystem::path {};
    if (!databaseFolderDraftInitialized_) {
        databaseFolderDraft_ = activeFolder.string();
        databaseFolderDraftInitialized_ = true;
    }

    UiTheme::pushPanelStyle();
    ImGui::BeginChild("DatabaseLocation", ImVec2(0.0f, 260.0f), true);
    ImGui::Text("Database Location");
    ImGui::Separator();
    ImGui::TextColored(UiTheme::MutedText, "Current database path");
    ImGui::TextWrapped("%s", activePath.string().c_str());
    if (isPathInsideRepository(activePath.string())) {
        ImGui::TextColored(UiTheme::TextWarning, "Database is inside the repository. Consider moving it outside Git.");
    }

    ImGui::Spacing();
    ImGui::TextColored(UiTheme::MutedText, "Selected database folder");
    ImGui::SetNextItemWidth(-FLT_MIN);
    ImGui::InputText("##DatabaseFolderPath", &databaseFolderDraft_);
    if (isRepositoryRoot(databaseFolderDraft_)) {
        ImGui::TextColored(UiTheme::TextWarning, "The repository root cannot be used as the database folder.");
    } else if (isPathInsideRepository(databaseFolderDraft_)) {
        ImGui::TextColored(UiTheme::TextWarning, "Selected folder is inside the repository. Personal databases should stay outside Git.");
    }

    UiTheme::pushButtonStyle(UiTheme::ElectricCyan);
    if (ImGui::Button("Choose Database Folder", ImVec2(190.0f, 0.0f))) {
        std::string error;
        const std::optional<std::string> folder = chooseFolder(L"Choose Investor Command Center Database Folder", error);
        if (folder.has_value()) {
            databaseFolderDraft_ = *folder;
            databaseLocationMessage_.clear();
        } else if (!error.empty()) {
            databaseLocationMessage_ = error;
            databaseLocationMessageIsError_ = true;
        }
    }
    UiTheme::popButtonStyle();
    ImGui::SameLine();
    UiTheme::pushButtonStyle(UiTheme::NeonMagenta);
    if (ImGui::Button("Move Database Now", ImVec2(170.0f, 0.0f))) {
        if (databaseFolderDraft_.empty()) {
            databaseLocationMessage_ = "Choose a database folder before moving the database.";
            databaseLocationMessageIsError_ = true;
        } else if (isRepositoryRoot(databaseFolderDraft_)) {
            databaseLocationMessage_ = "The repository root cannot be used as the database folder.";
            databaseLocationMessageIsError_ = true;
        } else {
            openMoveDatabaseConfirmation_ = true;
        }
    }
    UiTheme::popButtonStyle();
    ImGui::SameLine();
    UiTheme::pushButtonStyle(UiTheme::ElectricCyan);
    if (ImGui::Button("Open Database Folder", ImVec2(180.0f, 0.0f))) {
        std::string error;
        openFolderInExplorer(activeFolder.string(), error);
        if (!error.empty()) {
            databaseLocationMessage_ = error;
            databaseLocationMessageIsError_ = true;
        }
    }
    UiTheme::popButtonStyle();

    if (!databaseLocationMessage_.empty()) {
        ImGui::TextColored(databaseLocationMessageIsError_ ? UiTheme::Loss : UiTheme::Gain, "%s", databaseLocationMessage_.c_str());
    }
    ImGui::TextColored(UiTheme::MutedText, "Moving uses copy, verification, and saved path switching. The old database is not deleted automatically.");
    ImGui::EndChild();
    UiTheme::popPanelStyle();

    if (openMoveDatabaseConfirmation_) {
        ImGui::OpenPopup("Move Database Location");
        openMoveDatabaseConfirmation_ = false;
    }

    if (ImGui::BeginPopupModal("Move Database Location", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("Move local database?");
        ImGui::Separator();
        ImGui::TextWrapped("This will move your local database to a new folder. The app will use the new location after restart.");
        ImGui::Spacing();
        ImGui::TextColored(UiTheme::MutedText, "New folder:");
        ImGui::TextWrapped("%s", databaseFolderDraft_.c_str());
        if (isPathInsideRepository(databaseFolderDraft_)) {
            ImGui::TextColored(UiTheme::TextWarning, "This folder is inside the repository. Personal databases should stay outside Git.");
        }
        ImGui::Spacing();
        if (ImGui::Button("Copy And Switch On Restart", ImVec2(210.0f, 0.0f))) {
            std::string message;
            const bool moved = moveDatabaseToFolder(databaseFolderDraft_, message);
            databaseLocationMessage_ = message;
            databaseLocationMessageIsError_ = !moved;
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(100.0f, 0.0f))) {
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
}

void SettingsView::renderDatabaseBackup(AppState& state, AppSettingsRepository& settingsRepository, const std::function<void()>& backupNow)
{
    UiTheme::pushPanelStyle();
    ImGui::BeginChild("DatabaseBackup", ImVec2(0.0f, 280.0f), true);
    ImGui::Text("Database Backup");
    ImGui::Separator();
    ImGui::TextWrapped("Create a local SQLite database backup in a folder you control. This does not use cloud sync or move the database location.");

    ImGui::Spacing();
    ImGui::TextColored(UiTheme::MutedText, "Backup folder path");
    ImGui::SetNextItemWidth(-FLT_MIN);
    ImGui::InputText("##BackupFolderPath", &state.databaseBackupSettings.backupFolder);

    UiTheme::pushButtonStyle(UiTheme::ElectricCyan);
    if (ImGui::Button("Choose Backup Folder", ImVec2(180.0f, 0.0f))) {
        std::string error;
        const std::optional<std::string> folder = chooseFolder(L"Choose Investor Command Center Backup Folder", error);
        if (folder.has_value()) {
            state.databaseBackupSettings.backupFolder = *folder;
            if (saveDatabaseBackupSettings(state, settingsRepository, error)) {
                backupMessage_ = "Backup folder saved.";
                backupMessageIsError_ = false;
                state.setStatus("Backup folder saved locally.");
            } else {
                backupMessage_ = error;
                backupMessageIsError_ = true;
            }
        } else if (!error.empty()) {
            backupMessage_ = error;
            backupMessageIsError_ = true;
        }
    }
    UiTheme::popButtonStyle();

    ImGui::SameLine();
    UiTheme::pushButtonStyle(UiTheme::ElectricCyan);
    if (ImGui::Button("Save Backup Settings", ImVec2(180.0f, 0.0f))) {
        std::string error;
        if (saveDatabaseBackupSettings(state, settingsRepository, error)) {
            backupMessage_ = "Backup settings saved.";
            backupMessageIsError_ = false;
            state.setStatus("Backup settings saved locally.");
        } else {
            backupMessage_ = error;
            backupMessageIsError_ = true;
        }
    }
    UiTheme::popButtonStyle();

    ImGui::SameLine();
    UiTheme::pushButtonStyle(UiTheme::NeonMagenta);
    if (ImGui::Button("Back Up Now", ImVec2(120.0f, 0.0f))) {
        std::string error;
        if (saveDatabaseBackupSettings(state, settingsRepository, error)) {
            backupNow();
            backupMessage_ = "Backup requested.";
            backupMessageIsError_ = false;
        } else {
            backupMessage_ = error;
            backupMessageIsError_ = true;
        }
    }
    UiTheme::popButtonStyle();

    ImGui::Spacing();
    ImGui::Checkbox("Backup reminder enabled", &state.databaseBackupSettings.reminderEnabled);
    ImGui::SameLine();
    if (ImGui::BeginCombo("Frequency", state.databaseBackupSettings.reminderFrequency.c_str())) {
        const char* frequencies[] = { "Daily", "Weekly", "Monthly" };
        for (const char* frequency : frequencies) {
            const bool selected = state.databaseBackupSettings.reminderFrequency == frequency;
            if (ImGui::Selectable(frequency, selected)) {
                state.databaseBackupSettings.reminderFrequency = frequency;
            }
            if (selected) {
                ImGui::SetItemDefaultFocus();
            }
        }
        ImGui::EndCombo();
    }

    ImGui::TextColored(UiTheme::MutedText, "Last backup");
    ImGui::SameLine(160.0f);
    ImGui::Text("%s", state.databaseBackupSettings.lastBackupAt.empty() ? "None" : state.databaseBackupSettings.lastBackupAt.c_str());

    const std::string reminderText = DatabaseBackupService::reminderStatusText(state.databaseBackupSettings, Date::todayIso8601());
    const bool backupDue = DatabaseBackupService::isReminderDue(state.databaseBackupSettings, Date::todayIso8601());
    ImGui::TextColored(UiTheme::MutedText, "Reminder status");
    ImGui::SameLine(160.0f);
    ImGui::TextColored(backupDue ? UiTheme::Amber : UiTheme::TextSecondary, "%s", reminderText.c_str());

    if (!backupMessage_.empty()) {
        ImGui::TextColored(backupMessageIsError_ ? UiTheme::Loss : UiTheme::Gain, "%s", backupMessage_.c_str());
    }

    ImGui::TextColored(UiTheme::MutedText, "Backup files are personal local data and should stay out of GitHub.");
    ImGui::EndChild();
    UiTheme::popPanelStyle();
}

void SettingsView::renderCapitalGainAllocation(AppState& state, CapitalGainAllocationRepository& allocationRepository, const std::function<void()>& reloadData)
{
    UiTheme::pushPanelStyle();
    ImGui::BeginChild("CapitalGainsAllocation", ImVec2(0.0f, 330.0f), true);
    ImGui::Text("Capital Gains Allocation");
    ImGui::Separator();
    ImGui::TextWrapped("Set your own percentage split for realized capital gains. This does not move money or provide financial advice.");

    const double activeTotal = activeAllocationTotal(state.capitalGainAllocationRules);
    ImGui::Spacing();
    ImGui::Text("Active total: %s", Money::formatPercent(activeTotal).c_str());
    if (std::fabs(activeTotal - 100.0) > 0.01) {
        ImGui::SameLine();
        ImGui::TextColored(UiTheme::Amber, "Allocation total is %s. Amounts may not fully allocate the gain.", Money::formatPercent(activeTotal).c_str());
    }

    if (!allocationMessage_.empty()) {
        ImGui::TextColored(allocationMessageIsError_ ? UiTheme::Loss : UiTheme::Gain, "%s", allocationMessage_.c_str());
    }

    UiTheme::pushButtonStyle(UiTheme::NeonMagenta);
    if (ImGui::Button("Add Allocation Category")) {
        CapitalGainAllocationRule rule;
        rule.name = "New Category";
        rule.percentage = 0.0;
        rule.sortOrder = nextSortOrder(state.capitalGainAllocationRules);
        rule.isActive = true;

        std::string error;
        if (allocationRepository.create(rule, error)) {
            reloadData();
            allocationMessage_ = "Allocation category added.";
            allocationMessageIsError_ = false;
        } else {
            allocationMessage_ = error;
            allocationMessageIsError_ = true;
        }
    }
    UiTheme::popButtonStyle();

    ImGui::Spacing();

    if (state.capitalGainAllocationRules.empty()) {
        ImGui::TextColored(UiTheme::MutedText, "No allocation categories yet.");
        ImGui::EndChild();
        UiTheme::popPanelStyle();
        return;
    }

    bool shouldReload = false;
    UiTheme::pushTableStyle();
    if (ImGui::BeginTable("CapitalGainAllocationRulesTable", 7,
            ImGuiTableFlags_BordersInnerH | ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_ScrollY,
            ImVec2(0.0f, 205.0f))) {
        ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthStretch, 1.2f);
        ImGui::TableSetupColumn("Percentage", ImGuiTableColumnFlags_WidthFixed, 120.0f);
        ImGui::TableSetupColumn("Active", ImGuiTableColumnFlags_WidthFixed, 70.0f);
        ImGui::TableSetupColumn("Order", ImGuiTableColumnFlags_WidthFixed, 100.0f);
        ImGui::TableSetupColumn("Save", ImGuiTableColumnFlags_WidthFixed, 64.0f);
        ImGui::TableSetupColumn("Deactivate", ImGuiTableColumnFlags_WidthFixed, 92.0f);
        ImGui::TableSetupColumn("Delete", ImGuiTableColumnFlags_WidthFixed, 70.0f);
        ImGui::TableSetupScrollFreeze(0, 1);
        ImGui::TableHeadersRow();

        for (std::size_t index = 0; index < state.capitalGainAllocationRules.size(); ++index) {
            CapitalGainAllocationRule& rule = state.capitalGainAllocationRules[index];
            bool stopRenderingRows = false;
            ImGui::PushID(rule.id > 0 ? rule.id : static_cast<int>(index));
            ImGui::TableNextRow();

            ImGui::TableNextColumn();
            ImGui::SetNextItemWidth(-FLT_MIN);
            ImGui::InputText("##AllocationName", &rule.name);

            ImGui::TableNextColumn();
            ImGui::SetNextItemWidth(-FLT_MIN);
            ImGui::InputDouble("##AllocationPercent", &rule.percentage, 0.0, 0.0, "%.2f");

            ImGui::TableNextColumn();
            ImGui::Checkbox("##AllocationActive", &rule.isActive);

            ImGui::TableNextColumn();
            const bool canMoveUp = index > 0;
            const bool canMoveDown = index + 1 < state.capitalGainAllocationRules.size();
            if (!canMoveUp) {
                ImGui::BeginDisabled();
            }
            UiTheme::pushButtonStyle(UiTheme::ElectricCyan);
            if (ImGui::SmallButton("Up")) {
                std::swap(state.capitalGainAllocationRules[index], state.capitalGainAllocationRules[index - 1]);
                std::string error;
                if (allocationRepository.saveAll(state.capitalGainAllocationRules, error)) {
                    allocationMessage_ = "Allocation order saved.";
                    allocationMessageIsError_ = false;
                    shouldReload = true;
                    stopRenderingRows = true;
                } else {
                    allocationMessage_ = error;
                    allocationMessageIsError_ = true;
                }
            }
            UiTheme::popButtonStyle();
            if (!canMoveUp) {
                ImGui::EndDisabled();
            }
            ImGui::SameLine();
            if (!canMoveDown) {
                ImGui::BeginDisabled();
            }
            UiTheme::pushButtonStyle(UiTheme::ElectricCyan);
            if (ImGui::SmallButton("Down")) {
                std::swap(state.capitalGainAllocationRules[index], state.capitalGainAllocationRules[index + 1]);
                std::string error;
                if (allocationRepository.saveAll(state.capitalGainAllocationRules, error)) {
                    allocationMessage_ = "Allocation order saved.";
                    allocationMessageIsError_ = false;
                    shouldReload = true;
                    stopRenderingRows = true;
                } else {
                    allocationMessage_ = error;
                    allocationMessageIsError_ = true;
                }
            }
            UiTheme::popButtonStyle();
            if (!canMoveDown) {
                ImGui::EndDisabled();
            }

            if (stopRenderingRows) {
                ImGui::PopID();
                break;
            }

            ImGui::TableNextColumn();
            UiTheme::pushButtonStyle(UiTheme::ElectricCyan);
            if (ImGui::SmallButton("Save")) {
                rule.sortOrder = static_cast<int>(index) + 1;
                std::string error;
                if (allocationRepository.update(rule, error)) {
                    allocationMessage_ = "Allocation category saved.";
                    allocationMessageIsError_ = false;
                    shouldReload = true;
                } else {
                    allocationMessage_ = error;
                    allocationMessageIsError_ = true;
                }
            }
            UiTheme::popButtonStyle();

            ImGui::TableNextColumn();
            if (rule.isActive) {
                UiTheme::pushButtonStyle(UiTheme::Amber);
                if (ImGui::SmallButton("Deactivate")) {
                    rule.isActive = false;
                    std::string error;
                    if (allocationRepository.update(rule, error)) {
                        allocationMessage_ = "Allocation category deactivated.";
                        allocationMessageIsError_ = false;
                        shouldReload = true;
                    } else {
                        allocationMessage_ = error;
                        allocationMessageIsError_ = true;
                    }
                }
                UiTheme::popButtonStyle();
            } else {
                UiTheme::badge("Inactive", UiTheme::TextMuted);
            }

            ImGui::TableNextColumn();
            UiTheme::pushButtonStyle(UiTheme::Loss);
            if (ImGui::SmallButton("Delete")) {
                std::string error;
                if (allocationRepository.remove(rule.id, error)) {
                    allocationMessage_ = "Allocation category deleted.";
                    allocationMessageIsError_ = false;
                    shouldReload = true;
                    stopRenderingRows = true;
                } else {
                    allocationMessage_ = error;
                    allocationMessageIsError_ = true;
                }
            }
            UiTheme::popButtonStyle();

            ImGui::PopID();
            if (stopRenderingRows) {
                break;
            }
        }

        ImGui::EndTable();
    }
    UiTheme::popTableStyle();

    if (shouldReload) {
        reloadData();
    }

    ImGui::TextColored(UiTheme::MutedText, "Allocation rules are stored locally and used only as a display helper for realized gains.");
    ImGui::EndChild();
    UiTheme::popPanelStyle();
}
