// SPDX-License-Identifier: MIT
#include "ui/SettingsView.hpp"

#include "app/AppState.hpp"
#include "repositories/AppSettingsRepository.hpp"
#include "repositories/CapitalGainAllocationRepository.hpp"
<<<<<<< Updated upstream
=======
#include "services/DatabaseBackupService.hpp"
#include "services/SignalRulesService.hpp"
>>>>>>> Stashed changes
#include "ui/UiTheme.hpp"
#include "util/Money.hpp"

#include <imgui.h>
#include <imgui_stdlib.h>

#include <algorithm>
#include <cfloat>
#include <cmath>
#include <filesystem>
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

void signalRuleSummaryLine(const char* label, const std::string& value, ImVec4 color)
{
    ImGui::TextColored(UiTheme::MutedText, "%s", label);
    ImGui::SameLine(210.0f);
    ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + ImGui::GetContentRegionAvail().x);
    ImGui::TextColored(color, "%s", value.c_str());
    ImGui::PopTextWrapPos();
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

}

void SettingsView::render(AppState& state,
    AppSettingsRepository& settingsRepository,
    CapitalGainAllocationRepository& allocationRepository,
    const std::string& databasePath,
    const char* appVersion,
    const std::function<void()>& reloadData)
{
    const std::string absoluteDatabasePath = std::filesystem::absolute(databasePath).string();

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

    ImGui::BeginChild("DatabaseLocation", ImVec2(panelWidth, 260.0f), true);
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
    renderSignalRules(state, settingsRepository);

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

void SettingsView::renderSignalRules(AppState& state, AppSettingsRepository& settingsRepository)
{
    if (!signalRulesDraftInitialized_ || (!signalRulesDraftDirty_ && !(signalRulesDraft_ == state.signalRules))) {
        signalRulesDraft_ = state.signalRules;
        signalRulesDraftInitialized_ = true;
        signalRulesDraftDirty_ = false;
    }

    UiTheme::pushPanelStyle();
    ImGui::BeginChild("SignalRules", ImVec2(0.0f, 470.0f), true);
    ImGui::Text("Signal Rules");
    ImGui::Separator();
    ImGui::TextWrapped("Review and fine-tune the thresholds used by the existing Watchlist Buy/Sell/Hold signal engine. Defaults match the original hardcoded behavior.");

    ImGui::Spacing();
    const std::string rsiRange = Money::formatNumber(signalRulesDraft_.rsiBuyMin, 1) + " to " + Money::formatNumber(signalRulesDraft_.rsiBuyMax, 1);
    const std::string macdRule = "MACD histogram greater than " + Money::formatNumber(signalRulesDraft_.macdHistogramMin, 4);
    signalRuleSummaryLine("Buy rule", "Price is at or below Buy Level, RSI is within " + rsiRange + ", and " + macdRule + ".", UiTheme::Gain);
    signalRuleSummaryLine("Sell rule", "Price is at or above Sell Level. Sell remains price-only.", UiTheme::Loss);
    signalRuleSummaryLine("Hold rule", "Current price is missing, levels are not reached, indicators are missing, or technical confirmation is not met.", UiTheme::ElectricCyan);
    const std::string momentumLabel = "Momentum " + std::to_string(signalRulesDraft_.momentumLookbackSessions) + "D";
    signalRuleSummaryLine(momentumLabel.c_str(), "Watchlist display-only: latest close is compared with the close from the configured lookback. It does not change Buy/Sell/Hold.", UiTheme::TextSecondary);

    ImGui::Spacing();
    UiTheme::pushTableStyle();
    if (ImGui::BeginTable("SignalRulesInputs", 3, ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_SizingStretchProp)) {
        ImGui::TableSetupColumn("RSI");
        ImGui::TableSetupColumn("MACD");
        ImGui::TableSetupColumn("Momentum");
        ImGui::TableNextRow();

        ImGui::TableNextColumn();
        ImGui::TextColored(UiTheme::TextPrimary, "RSI Rule");
        ImGui::TextWrapped("Buy confirmation accepts RSI only inside this range.");
        ImGui::SetNextItemWidth(140.0f);
        if (ImGui::InputDouble("Lower##SignalRulesRsiLower", &signalRulesDraft_.rsiBuyMin, 0.0, 0.0, "%.1f")) {
            signalRulesDraftDirty_ = true;
            signalRulesMessage_.clear();
        }
        ImGui::SetNextItemWidth(140.0f);
        if (ImGui::InputDouble("Upper##SignalRulesRsiUpper", &signalRulesDraft_.rsiBuyMax, 0.0, 0.0, "%.1f")) {
            signalRulesDraftDirty_ = true;
            signalRulesMessage_.clear();
        }

        ImGui::TableNextColumn();
        ImGui::TextColored(UiTheme::TextPrimary, "MACD Rule");
        ImGui::TextWrapped("Buy confirmation uses the MACD histogram threshold, not the MACD signal line.");
        ImGui::SetNextItemWidth(170.0f);
        if (ImGui::InputDouble("Histogram Minimum##SignalRulesMacdHistogram", &signalRulesDraft_.macdHistogramMin, 0.0, 0.0, "%.4f")) {
            signalRulesDraftDirty_ = true;
            signalRulesMessage_.clear();
        }
        ImGui::TextColored(UiTheme::MutedText, "Default 0.0000 means histogram must be positive.");

        ImGui::TableNextColumn();
        ImGui::TextColored(UiTheme::TextPrimary, "Momentum Rule");
        ImGui::TextWrapped("Watchlist Momentum 7D is display-only by default and uses historical closes.");
        ImGui::SetNextItemWidth(170.0f);
        if (ImGui::InputInt("Lookback Sessions##SignalRulesMomentumLookback", &signalRulesDraft_.momentumLookbackSessions)) {
            signalRulesDraftDirty_ = true;
            signalRulesMessage_.clear();
        }
        ImGui::TextColored(UiTheme::MutedText, "Rising when latest close is greater than the comparison close.");

        ImGui::EndTable();
    }
    UiTheme::popTableStyle();

    ImGui::Spacing();
    signalRuleSummaryLine("Signal output", "Buy, Sell, and Hold labels keep the same decision structure. Saved watchlist signal statuses update on the next Watchlist price refresh.", UiTheme::TextSecondary);

    std::string validationMessage;
    const bool draftValid = SignalRulesService::validate(signalRulesDraft_, validationMessage);
    if (!signalRulesMessage_.empty()) {
        ImGui::TextColored(signalRulesMessageIsError_ ? UiTheme::Loss : UiTheme::Gain, "%s", signalRulesMessage_.c_str());
    } else if (signalRulesDraftDirty_ && !draftValid) {
        ImGui::TextColored(UiTheme::Loss, "%s", validationMessage.c_str());
    } else if (signalRulesDraftDirty_) {
        ImGui::TextColored(UiTheme::Amber, "Unsaved signal rule changes.");
    } else {
        ImGui::TextColored(UiTheme::MutedText, "Active signal rules are saved locally in app settings.");
    }

    ImGui::Spacing();
    const bool hasChanges = signalRulesDraftDirty_ && !(signalRulesDraft_ == state.signalRules);
    if (!hasChanges) {
        ImGui::BeginDisabled();
    }
    UiTheme::pushButtonStyle(UiTheme::ElectricCyan);
    if (ImGui::Button("Save Changes", ImVec2(140.0f, 0.0f))) {
        std::string error;
        if (SignalRulesService::save(settingsRepository, signalRulesDraft_, error)) {
            state.signalRules = signalRulesDraft_;
            signalRulesDraftDirty_ = false;
            signalRulesMessage_ = "Signal rules saved. Watchlist badges use them immediately; stored statuses update on the next price refresh.";
            signalRulesMessageIsError_ = false;
            state.setStatus("Signal rules saved locally.");
        } else {
            signalRulesMessage_ = error;
            signalRulesMessageIsError_ = true;
        }
    }
    UiTheme::popButtonStyle();
    if (!hasChanges) {
        ImGui::EndDisabled();
    }

    ImGui::SameLine();
    if (!hasChanges) {
        ImGui::BeginDisabled();
    }
    if (ImGui::Button("Cancel", ImVec2(100.0f, 0.0f))) {
        signalRulesDraft_ = state.signalRules;
        signalRulesDraftDirty_ = false;
        signalRulesMessage_ = "Signal rule changes discarded.";
        signalRulesMessageIsError_ = false;
    }
    if (!hasChanges) {
        ImGui::EndDisabled();
    }

    ImGui::SameLine();
    UiTheme::pushButtonStyle(UiTheme::NeonMagenta);
    if (ImGui::Button("Reset to Defaults", ImVec2(160.0f, 0.0f))) {
        std::string error;
        if (SignalRulesService::resetToDefaults(settingsRepository, error)) {
            signalRulesDraft_ = SignalRulesService::defaults();
            state.signalRules = signalRulesDraft_;
            signalRulesDraftDirty_ = false;
            signalRulesMessage_ = "Signal rules reset to defaults. Watchlist badges use them immediately; stored statuses update on the next price refresh.";
            signalRulesMessageIsError_ = false;
            state.setStatus("Signal rules reset to defaults.");
        } else {
            signalRulesMessage_ = error;
            signalRulesMessageIsError_ = true;
        }
    }
    UiTheme::popButtonStyle();

    ImGui::EndChild();
    UiTheme::popPanelStyle();
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
