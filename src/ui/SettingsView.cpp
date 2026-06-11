// SPDX-License-Identifier: MIT
#include "ui/SettingsView.hpp"

#include "app/AppState.hpp"
#include "repositories/AppSettingsRepository.hpp"
#include "repositories/CapitalGainAllocationRepository.hpp"
#include "services/TechnicalIndicatorSettingsService.hpp"
#include "ui/UiTheme.hpp"
#include "util/Money.hpp"

#include <imgui.h>
#include <imgui_stdlib.h>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <commdlg.h>
#endif

#include <algorithm>
#include <cfloat>
#include <cmath>
#include <filesystem>
#include <optional>
#include <string>

namespace {

void disabledAction(const char* label, const char* note)
{
    ImGui::BeginDisabled();
    ImGui::Button(label, ImVec2(180.0f, 0.0f));
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

std::filesystem::path absolutePath(const std::string& path)
{
    std::error_code error;
    const std::filesystem::path absolute = std::filesystem::absolute(std::filesystem::path(path), error);
    return error ? std::filesystem::path(path) : absolute.lexically_normal();
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

bool isPathInsideWorkingTree(const std::string& path)
{
    std::error_code error;
    const std::filesystem::path workingDirectory = std::filesystem::current_path(error);
    if (error || path.empty()) {
        return false;
    }

    return isPathInside(absolutePath(path), workingDirectory);
}

std::optional<std::string> chooseDatabaseFile(std::string& error)
{
    error.clear();

#ifdef _WIN32
    wchar_t selectedPath[MAX_PATH] = L"";
    OPENFILENAMEW dialog {};
    dialog.lStructSize = sizeof(dialog);
    dialog.lpstrFile = selectedPath;
    dialog.nMaxFile = MAX_PATH;
    dialog.lpstrFilter = L"SQLite database files\0*.db;*.sqlite;*.sqlite3\0All files\0*.*\0";
    dialog.lpstrTitle = L"Choose Investor Command Center Database";
    dialog.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;

    if (GetOpenFileNameW(&dialog)) {
        return std::filesystem::path(selectedPath).string();
    }

    if (CommDlgExtendedError() != 0) {
        error = "Could not open the Windows database file picker.";
    }
    return std::nullopt;
#else
    error = "Database file picker is only available in the Windows desktop build.";
    return std::nullopt;
#endif
}

}

void SettingsView::render(AppState& state,
    AppSettingsRepository& settingsRepository,
    CapitalGainAllocationRepository& allocationRepository,
    const std::string& activeDatabasePath,
    const char* appVersion,
    const std::function<bool(const std::string&, std::string&)>& saveDatabasePath,
    const std::function<void()>& reloadData)
{
    UiTheme::sectionHeading("Settings", "Local storage, privacy, exports, and maintenance reminders.");

    const float availableWidth = ImGui::GetContentRegionAvail().x;
    const float gap = ImGui::GetStyle().ItemSpacing.x;
    const float panelWidth = (availableWidth - gap) / 2.0f;

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

    ImGui::SameLine();

    renderDatabaseLocation(activeDatabasePath, saveDatabasePath);

    ImGui::Spacing();

    ImGui::BeginChild("Privacy", ImVec2(panelWidth, 220.0f), true);
    ImGui::Text("Data Privacy");
    ImGui::Separator();
    ImGui::TextWrapped("Investor Command Center stores records locally and does not connect to brokerage accounts or cloud services. Stock Research fetches informational market data only when explicitly requested.");
    ImGui::Spacing();
    ImGui::TextColored(UiTheme::Gain, "Local-first");
    ImGui::TextColored(UiTheme::MutedText, "No login");
    ImGui::TextColored(UiTheme::MutedText, "No cloud sync");
    ImGui::TextColored(UiTheme::MutedText, "No automatic market data refresh");
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

    ImGui::Spacing();
    renderTechnicalIndicatorSettings(state, settingsRepository);

    ImGui::Spacing();
    renderCapitalGainAllocation(state, allocationRepository, reloadData);

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

void SettingsView::renderDatabaseLocation(const std::string& activeDatabasePath, const std::function<bool(const std::string&, std::string&)>& saveDatabasePath)
{
    const std::filesystem::path activePath = absolutePath(activeDatabasePath);
    if (!databasePathDraftInitialized_) {
        databasePathDraft_ = activePath.string();
        databasePathDraftInitialized_ = true;
    }

    ImGui::BeginChild("DatabaseLocation", ImVec2(0.0f, 260.0f), true);
    ImGui::Text("Database Location");
    ImGui::Separator();

    ImGui::TextColored(UiTheme::MutedText, "Active database path");
    ImGui::TextWrapped("%s", activePath.string().c_str());
    if (isPathInsideWorkingTree(activePath.string())) {
        ImGui::TextColored(UiTheme::TextWarning, "This database is inside the current working tree. Personal databases should stay ignored by Git.");
    }

    ImGui::Spacing();
    ImGui::TextColored(UiTheme::MutedText, "Database file for next launch");
    ImGui::SetNextItemWidth(-FLT_MIN);
    ImGui::InputText("##DatabasePathDraft", &databasePathDraft_);

    if (databasePathDraft_.empty()) {
        ImGui::TextColored(UiTheme::TextMuted, "Empty path means the default local database will be used after restart.");
    } else {
        const std::filesystem::path draftPath = absolutePath(databasePathDraft_);
        std::error_code filesystemError;
        const bool exists = std::filesystem::exists(draftPath, filesystemError) && !filesystemError;
        const bool isDirectory = std::filesystem::is_directory(draftPath, filesystemError) && !filesystemError;
        if (!exists || isDirectory) {
            ImGui::TextColored(UiTheme::TextWarning, "Choose an existing SQLite database file.");
        } else if (isPathInsideWorkingTree(draftPath.string())) {
            ImGui::TextColored(UiTheme::TextWarning, "Selected database is inside the current working tree.");
        } else {
            ImGui::TextColored(UiTheme::TextMuted, "Restart required after saving a different path.");
        }
    }

    if (ImGui::Button("Choose Database File", ImVec2(180.0f, 0.0f))) {
        std::string error;
        const std::optional<std::string> chosenPath = chooseDatabaseFile(error);
        if (chosenPath.has_value()) {
            databasePathDraft_ = absolutePath(*chosenPath).string();
            databaseLocationMessage_.clear();
        } else if (!error.empty()) {
            databaseLocationMessage_ = error;
            databaseLocationMessageIsError_ = true;
        }
    }

    ImGui::SameLine();
    if (ImGui::Button("Use Default On Restart", ImVec2(190.0f, 0.0f))) {
        databasePathDraft_.clear();
        databaseLocationMessage_.clear();
    }

    if (ImGui::Button("Save Path For Restart", ImVec2(180.0f, 0.0f))) {
        std::string message;
        const bool saved = saveDatabasePath(databasePathDraft_.empty() ? std::string {} : absolutePath(databasePathDraft_).string(), message);
        databaseLocationMessage_ = message;
        databaseLocationMessageIsError_ = !saved;
    }

    if (!databaseLocationMessage_.empty()) {
        ImGui::TextColored(databaseLocationMessageIsError_ ? UiTheme::Loss : UiTheme::Gain, "%s", databaseLocationMessage_.c_str());
    }

    ImGui::TextColored(UiTheme::MutedText, "Saving a database path does not copy, move, delete, or overwrite database files.");
    ImGui::EndChild();
}

void SettingsView::renderTechnicalIndicatorSettings(AppState& state, AppSettingsRepository& settingsRepository)
{
    if (!technicalSettingsDraftInitialized_ || (!technicalSettingsDraftDirty_ && !(technicalSettingsDraft_ == state.technicalIndicatorSettings))) {
        technicalSettingsDraft_ = state.technicalIndicatorSettings;
        technicalSettingsDraftInitialized_ = true;
        technicalSettingsDraftDirty_ = false;
    }

    ImGui::BeginChild("WatchlistTechnicalSettings", ImVec2(0.0f, 330.0f), true);
    ImGui::Text("Watchlist Indicator Tuning");
    ImGui::Separator();
    ImGui::TextWrapped("These settings define RSI, MACD, Momentum, and extra technical column defaults for the Watchlist page. Saved values are stored locally and update the Watchlist display immediately.");

    ImGui::Spacing();
    if (ImGui::BeginTable("WatchlistTechnicalSettingsInputs", 4, ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_SizingStretchProp)) {
        ImGui::TableSetupColumn("RSI");
        ImGui::TableSetupColumn("MACD");
        ImGui::TableSetupColumn("Momentum");
        ImGui::TableSetupColumn("Display");
        ImGui::TableNextRow();

        ImGui::TableNextColumn();
        ImGui::TextColored(UiTheme::TextPrimary, "RSI");
        ImGui::TextWrapped("Period and threshold bounds.");
        ImGui::SetNextItemWidth(-FLT_MIN);
        if (ImGui::InputInt("Period##TechnicalRsiPeriod", &technicalSettingsDraft_.rsiPeriod)) {
            technicalSettingsDraftDirty_ = true;
            technicalSettingsMessage_.clear();
        }
        ImGui::SetNextItemWidth(-FLT_MIN);
        if (ImGui::InputDouble("Oversold##TechnicalRsiOversold", &technicalSettingsDraft_.rsiOversoldThreshold, 0.0, 0.0, "%.2f")) {
            technicalSettingsDraftDirty_ = true;
            technicalSettingsMessage_.clear();
        }
        ImGui::SetNextItemWidth(-FLT_MIN);
        if (ImGui::InputDouble("Overbought##TechnicalRsiOverbought", &technicalSettingsDraft_.rsiOverboughtThreshold, 0.0, 0.0, "%.2f")) {
            technicalSettingsDraftDirty_ = true;
            technicalSettingsMessage_.clear();
        }

        ImGui::TableNextColumn();
        ImGui::TextColored(UiTheme::TextPrimary, "MACD");
        ImGui::TextWrapped("EMA periods for the MACD calculation.");
        ImGui::SetNextItemWidth(-FLT_MIN);
        if (ImGui::InputInt("Fast EMA##TechnicalMacdFast", &technicalSettingsDraft_.macdFastPeriod)) {
            technicalSettingsDraftDirty_ = true;
            technicalSettingsMessage_.clear();
        }
        ImGui::SetNextItemWidth(-FLT_MIN);
        if (ImGui::InputInt("Slow EMA##TechnicalMacdSlow", &technicalSettingsDraft_.macdSlowPeriod)) {
            technicalSettingsDraftDirty_ = true;
            technicalSettingsMessage_.clear();
        }
        ImGui::SetNextItemWidth(-FLT_MIN);
        if (ImGui::InputInt("Signal EMA##TechnicalMacdSignal", &technicalSettingsDraft_.macdSignalPeriod)) {
            technicalSettingsDraftDirty_ = true;
            technicalSettingsMessage_.clear();
        }

        ImGui::TableNextColumn();
        ImGui::TextColored(UiTheme::TextPrimary, "Momentum");
        ImGui::TextWrapped("Lookback and percent thresholds.");
        ImGui::SetNextItemWidth(-FLT_MIN);
        if (ImGui::InputInt("Lookback Days##TechnicalMomentumLookback", &technicalSettingsDraft_.momentumLookbackDays)) {
            technicalSettingsDraftDirty_ = true;
            technicalSettingsMessage_.clear();
        }
        ImGui::SetNextItemWidth(-FLT_MIN);
        if (ImGui::InputDouble("Positive %##TechnicalMomentumPositive", &technicalSettingsDraft_.momentumPositiveThresholdPercent, 0.0, 0.0, "%.2f")) {
            technicalSettingsDraftDirty_ = true;
            technicalSettingsMessage_.clear();
        }
        ImGui::SetNextItemWidth(-FLT_MIN);
        if (ImGui::InputDouble("Negative %##TechnicalMomentumNegative", &technicalSettingsDraft_.momentumNegativeThresholdPercent, 0.0, 0.0, "%.2f")) {
            technicalSettingsDraftDirty_ = true;
            technicalSettingsMessage_.clear();
        }

        ImGui::TableNextColumn();
        ImGui::TextColored(UiTheme::TextPrimary, "Display");
        ImGui::TextWrapped("Watchlist page display defaults.");
        if (ImGui::Checkbox("Show extra technical columns", &technicalSettingsDraft_.showExtraTechnicals)) {
            technicalSettingsDraftDirty_ = true;
            technicalSettingsMessage_.clear();
        }
        ImGui::TextColored(UiTheme::MutedText, "Default is on.");

        ImGui::EndTable();
    }

    ImGui::Spacing();
    std::string validationMessage;
    const bool draftValid = TechnicalIndicatorSettingsService::validate(technicalSettingsDraft_, validationMessage);
    if (!technicalSettingsMessage_.empty()) {
        ImGui::TextColored(technicalSettingsMessageIsError_ ? UiTheme::Loss : UiTheme::Gain, "%s", technicalSettingsMessage_.c_str());
    } else if (technicalSettingsDraftDirty_ && !draftValid) {
        ImGui::TextColored(UiTheme::Loss, "%s", validationMessage.c_str());
    } else if (technicalSettingsDraftDirty_) {
        ImGui::TextColored(UiTheme::Amber, "Unsaved technical indicator settings.");
    } else {
        ImGui::TextColored(UiTheme::MutedText, "Active settings are saved locally. Refresh history to recalculate cached indicator snapshots after changing periods.");
    }

    const bool hasChanges = technicalSettingsDraftDirty_ && !(technicalSettingsDraft_ == state.technicalIndicatorSettings);
    if (!hasChanges || !draftValid) {
        ImGui::BeginDisabled();
    }
    if (ImGui::Button("Save Technical Settings", ImVec2(190.0f, 0.0f))) {
        std::string error;
        if (TechnicalIndicatorSettingsService::save(settingsRepository, technicalSettingsDraft_, error)) {
            state.technicalIndicatorSettings = technicalSettingsDraft_;
            technicalSettingsDraftDirty_ = false;
            technicalSettingsMessage_ = "Technical indicator settings saved.";
            technicalSettingsMessageIsError_ = false;
            state.setStatus("Technical indicator settings saved locally.");
        } else {
            technicalSettingsMessage_ = error;
            technicalSettingsMessageIsError_ = true;
        }
    }
    if (!hasChanges || !draftValid) {
        ImGui::EndDisabled();
    }

    ImGui::SameLine();
    if (!hasChanges) {
        ImGui::BeginDisabled();
    }
    if (ImGui::Button("Cancel", ImVec2(100.0f, 0.0f))) {
        technicalSettingsDraft_ = state.technicalIndicatorSettings;
        technicalSettingsDraftDirty_ = false;
        technicalSettingsMessage_ = "Technical indicator setting changes discarded.";
        technicalSettingsMessageIsError_ = false;
    }
    if (!hasChanges) {
        ImGui::EndDisabled();
    }

    ImGui::SameLine();
    if (ImGui::Button("Reset Technical Defaults", ImVec2(190.0f, 0.0f))) {
        std::string error;
        if (TechnicalIndicatorSettingsService::resetToDefaults(settingsRepository, error)) {
            technicalSettingsDraft_ = TechnicalIndicatorSettingsService::defaults();
            state.technicalIndicatorSettings = technicalSettingsDraft_;
            technicalSettingsDraftDirty_ = false;
            technicalSettingsMessage_ = "Technical indicator settings reset to defaults.";
            technicalSettingsMessageIsError_ = false;
            state.setStatus("Technical indicator settings reset to defaults.");
        } else {
            technicalSettingsMessage_ = error;
            technicalSettingsMessageIsError_ = true;
        }
    }

    ImGui::EndChild();
}

void SettingsView::renderCapitalGainAllocation(AppState& state, CapitalGainAllocationRepository& allocationRepository, const std::function<void()>& reloadData)
{
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

    ImGui::Spacing();

    if (state.capitalGainAllocationRules.empty()) {
        ImGui::TextColored(UiTheme::MutedText, "No allocation categories yet.");
        ImGui::EndChild();
        return;
    }

    bool shouldReload = false;
    if (ImGui::BeginTable("CapitalGainAllocationRulesTable", 7,
            ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_ScrollY,
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
            if (!canMoveUp) {
                ImGui::EndDisabled();
            }
            ImGui::SameLine();
            if (!canMoveDown) {
                ImGui::BeginDisabled();
            }
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
            if (!canMoveDown) {
                ImGui::EndDisabled();
            }

            if (stopRenderingRows) {
                ImGui::PopID();
                break;
            }

            ImGui::TableNextColumn();
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

            ImGui::TableNextColumn();
            if (rule.isActive) {
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
            } else {
                ImGui::TextColored(UiTheme::MutedText, "Inactive");
            }

            ImGui::TableNextColumn();
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

            ImGui::PopID();
            if (stopRenderingRows) {
                break;
            }
        }

        ImGui::EndTable();
    }

    if (shouldReload) {
        reloadData();
    }

    ImGui::TextColored(UiTheme::MutedText, "Allocation rules are stored locally and used only as a display helper for realized gains.");
    ImGui::EndChild();
}
