// SPDX-License-Identifier: MIT
#include "ui/ReportsView.hpp"

#include "app/AppState.hpp"
#include "services/StockPickReportDocxExporter.hpp"
#include "services/StockPickReportExportCommon.hpp"
#include "services/StockPickReportPdfExporter.hpp"
#include "services/StockPickReportService.hpp"
#include "ui/UiTheme.hpp"
#include "util/Money.hpp"

#include <imgui.h>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <commdlg.h>
#endif

#include <algorithm>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace {

bool isActiveAccount(const Account& account)
{
    return account.status.empty() || account.status == "Active";
}

std::vector<const Account*> activeAccounts(const AppState& state)
{
    std::vector<const Account*> accounts;
    for (const Account& account : state.accounts) {
        if (isActiveAccount(account)) {
            accounts.push_back(&account);
        }
    }
    return accounts;
}

const Account* accountById(const AppState& state, int accountId)
{
    for (const Account& account : state.accounts) {
        if (account.id == accountId) {
            return &account;
        }
    }
    return nullptr;
}

std::string displayAccountName(const Account& account)
{
    if (account.institutionName.empty()) {
        return account.accountName;
    }
    return account.accountName + " - " + account.institutionName;
}

void beginReportPanel(const char* id, const char* title, ImVec2 size, ImVec4 accent, ImGuiWindowFlags flags = 0)
{
    UiTheme::pushPanelStyle();
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(16.0f, 14.0f));
    ImGui::BeginChild(id, size, true, flags);
    ImGui::PopStyleVar();

    const ImVec2 min = ImGui::GetWindowPos();
    const ImVec2 max(min.x + ImGui::GetWindowSize().x, min.y + ImGui::GetWindowSize().y);
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    drawList->AddRectFilled(min, ImVec2(max.x, min.y + 38.0f), ImGui::GetColorU32(UiTheme::withAlpha(UiTheme::SurfaceElevated, 0.18f)), 16.0f, ImDrawFlags_RoundCornersTop);
    drawList->AddRectFilled(ImVec2(min.x + 14.0f, min.y + 9.0f), ImVec2(min.x + 62.0f, min.y + 11.0f), ImGui::GetColorU32(UiTheme::withAlpha(accent, 0.42f)), 2.0f);
    drawList->AddRect(min, max, ImGui::GetColorU32(UiTheme::withAlpha(accent, 0.10f)), 16.0f);

    ImGui::TextColored(UiTheme::TextSecondary, "%s", title);
    const ImVec2 cursor = ImGui::GetCursorScreenPos();
    const float width = ImGui::GetContentRegionAvail().x;
    drawList->AddRectFilled(cursor, ImVec2(cursor.x + width, cursor.y + 1.0f), ImGui::GetColorU32(UiTheme::withAlpha(UiTheme::BorderSubtle, 0.08f)));
    ImGui::Dummy(ImVec2(width, 8.0f));
}

void endReportPanel()
{
    ImGui::EndChild();
    UiTheme::popPanelStyle();
}

bool drawAccountSelector(const AppState& state, int& selectedAccountId)
{
    const std::vector<const Account*> accounts = activeAccounts(state);
    if (accounts.empty()) {
        ImGui::BeginDisabled();
        ImGui::Button("No Accounts Available", ImVec2(220.0f, 0.0f));
        ImGui::EndDisabled();
        return false;
    }

    if (accountById(state, selectedAccountId) == nullptr) {
        selectedAccountId = accounts.front()->id;
    }

    const Account* selected = accountById(state, selectedAccountId);
    const std::string preview = selected == nullptr ? "Select account" : displayAccountName(*selected);
    bool changed = false;

    ImGui::SetNextItemWidth(320.0f);
    if (ImGui::BeginCombo("Selected Account", preview.c_str())) {
        for (const Account* account : accounts) {
            const bool isSelected = account->id == selectedAccountId;
            const std::string label = displayAccountName(*account);
            if (ImGui::Selectable(label.c_str(), isSelected)) {
                selectedAccountId = account->id;
                changed = true;
            }
            if (isSelected) {
                ImGui::SetItemDefaultFocus();
            }
        }
        ImGui::EndCombo();
    }

    return changed;
}

bool drawReportTypeSelector()
{
    ImGui::SetNextItemWidth(320.0f);
    if (ImGui::BeginCombo("Report Type", "Stock Pick Helper Research Report")) {
        ImGui::Selectable("Stock Pick Helper Research Report", true);
        ImGui::EndCombo();
    }
    return false;
}

bool drawPeriodSelector(StockPickReportPeriod& period)
{
    bool changed = false;
    ImGui::TextColored(UiTheme::TextMuted, "Report Period");
    UiTheme::pushButtonStyle(period == StockPickReportPeriod::Weekly ? UiTheme::NeonMagenta : UiTheme::ElectricCyan);
    if (ImGui::Button("Weekly", ImVec2(104.0f, 0.0f))) {
        period = StockPickReportPeriod::Weekly;
        changed = true;
    }
    UiTheme::popButtonStyle();
    ImGui::SameLine();
    UiTheme::pushButtonStyle(period == StockPickReportPeriod::Monthly ? UiTheme::NeonMagenta : UiTheme::ElectricCyan);
    if (ImGui::Button("Monthly", ImVec2(104.0f, 0.0f))) {
        period = StockPickReportPeriod::Monthly;
        changed = true;
    }
    UiTheme::popButtonStyle();
    return changed;
}

void drawSummaryMetric(const char* label, const std::string& value, ImVec4 color)
{
    ImGui::TextColored(UiTheme::TextMuted, "%s", label);
    UiTheme::pushNumericFont();
    ImGui::TextColored(color, "%s", value.c_str());
    UiTheme::popNumericFont();
}

ImVec4 signalColor(const std::string& signal)
{
    if (signal == "Buy") {
        return UiTheme::Gain;
    }
    if (signal == "Sell") {
        return UiTheme::Loss;
    }
    if (signal == "Not Tracked") {
        return UiTheme::MutedText;
    }
    return UiTheme::ElectricCyan;
}

const char* optionalMoney(double value, bool available)
{
    static std::string formatted;
    formatted = available ? Money::format(value) : "N/A";
    return formatted.c_str();
}

const char* optionalPercent(double value, bool available, bool includeSign = false)
{
    static std::string formatted;
    formatted = available ? Money::formatPercent(value, includeSign) : "N/A";
    return formatted.c_str();
}

std::wstring widenUtf8(const std::string& value)
{
#ifdef _WIN32
    if (value.empty()) {
        return {};
    }

    const int length = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, value.c_str(), -1, nullptr, 0);
    if (length <= 0) {
        std::wstring fallback;
        fallback.reserve(value.size());
        for (const unsigned char character : value) {
            fallback.push_back(static_cast<wchar_t>(character));
        }
        return fallback;
    }

    std::wstring result(static_cast<std::size_t>(length), L'\0');
    MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, value.c_str(), -1, result.data(), length);
    if (!result.empty() && result.back() == L'\0') {
        result.pop_back();
    }
    return result;
#else
    return std::wstring(value.begin(), value.end());
#endif
}

std::optional<std::filesystem::path> chooseReportExportPath(
    const std::string& defaultFileName,
    const wchar_t* filter,
    const wchar_t* defaultExtension,
    const wchar_t* title,
    const char* reportKind,
    std::string& error)
{
    error.clear();

#ifdef _WIN32
    wchar_t selectedPath[MAX_PATH] {};
    const std::wstring defaultName = widenUtf8(defaultFileName);
    wcsncpy_s(selectedPath, defaultName.c_str(), _TRUNCATE);

    OPENFILENAMEW dialog {};
    dialog.lStructSize = sizeof(dialog);
    dialog.lpstrFile = selectedPath;
    dialog.nMaxFile = MAX_PATH;
    dialog.lpstrFilter = filter;
    dialog.lpstrDefExt = defaultExtension;
    dialog.lpstrTitle = title;
    dialog.Flags = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;

    if (GetSaveFileNameW(&dialog) == TRUE) {
        return std::filesystem::path(selectedPath);
    }

    if (CommDlgExtendedError() != 0) {
        error = std::string("Could not open the Windows ") + reportKind + " export file picker.";
    }
    return std::nullopt;
#else
    error = std::string(reportKind) + " export file picker is only available in the Windows desktop build.";
    return std::nullopt;
#endif
}

std::optional<std::filesystem::path> chooseWordExportPath(const std::string& defaultFileName, std::string& error)
{
    return chooseReportExportPath(
        defaultFileName,
        L"Word Documents (*.docx)\0*.docx\0All Files (*.*)\0*.*\0",
        L"docx",
        L"Export Stock Pick Helper Research Report as Word",
        "Word",
        error);
}

std::optional<std::filesystem::path> choosePdfExportPath(const std::string& defaultFileName, std::string& error)
{
    return chooseReportExportPath(
        defaultFileName,
        L"PDF Files (*.pdf)\0*.pdf\0All Files (*.*)\0*.*\0",
        L"pdf",
        L"Export Stock Pick Helper Research Report as PDF",
        "PDF",
        error);
}

void drawPreviewHeader(const StockPickResearchReport& report)
{
    ImGui::SetWindowFontScale(1.22f);
    ImGui::TextColored(UiTheme::TextPrimary, "%s", report.title.c_str());
    ImGui::SetWindowFontScale(1.0f);
    ImGui::TextColored(UiTheme::MutedText, "Generated: %s", report.generatedAt.c_str());
    ImGui::TextColored(UiTheme::TextSecondary, "Account: %s", report.accountName.c_str());
    ImGui::Spacing();
}

void drawReportSummary(const StockPickResearchReport& report)
{
    const float availableWidth = ImGui::GetContentRegionAvail().x;
    const int columns = availableWidth > 900.0f ? 4 : (availableWidth > 620.0f ? 2 : 1);
    if (!ImGui::BeginTable("ReportSummaryMetrics", columns, ImGuiTableFlags_SizingStretchSame)) {
        return;
    }

    ImGui::TableNextColumn();
    drawSummaryMetric("Total Account Value", Money::format(report.totalAccountValue), UiTheme::TextPrimary);
    ImGui::TableNextColumn();
    drawSummaryMetric("Holdings Value", Money::format(report.holdingsValue), UiTheme::ElectricCyan);
    ImGui::TableNextColumn();
    drawSummaryMetric("Cash Balance", Money::format(report.cashBalance), UiTheme::Amber);
    ImGui::TableNextColumn();
    drawSummaryMetric("Number of Holdings", std::to_string(report.holdingCount), UiTheme::TextPrimary);

    ImGui::EndTable();
}

void drawHoldingsTable(const StockPickResearchReport& report)
{
    if (report.holdings.empty()) {
        UiTheme::emptyState("No holdings for this account", "Add holdings to the selected account before generating this report.");
        return;
    }

    UiTheme::pushTableStyle();
    if (ImGui::BeginTable(
            "StockPickResearchReportHoldings",
            11,
            ImGuiTableFlags_BordersInnerH | ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable | ImGuiTableFlags_Reorderable |
                ImGuiTableFlags_Hideable | ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_ScrollX,
            ImVec2(0.0f, 360.0f))) {
        ImGui::TableSetupColumn("Ticker", ImGuiTableColumnFlags_WidthFixed, 80.0f);
        ImGui::TableSetupColumn("Company / Name", ImGuiTableColumnFlags_WidthStretch, 1.3f);
        ImGui::TableSetupColumn("Shares", ImGuiTableColumnFlags_WidthFixed, 98.0f);
        ImGui::TableSetupColumn("Current Price", ImGuiTableColumnFlags_WidthFixed, 112.0f);
        ImGui::TableSetupColumn("Market Value", ImGuiTableColumnFlags_WidthFixed, 124.0f);
        ImGui::TableSetupColumn("Current Allocation %", ImGuiTableColumnFlags_WidthFixed, 140.0f);
        ImGui::TableSetupColumn("Target Allocation %", ImGuiTableColumnFlags_WidthFixed, 136.0f);
        ImGui::TableSetupColumn("Allocation Difference", ImGuiTableColumnFlags_WidthFixed, 136.0f);
        ImGui::TableSetupColumn("Current Signal", ImGuiTableColumnFlags_WidthFixed, 116.0f);
        ImGui::TableSetupColumn("Buy Level", ImGuiTableColumnFlags_WidthFixed, 104.0f);
        ImGui::TableSetupColumn("Priority", ImGuiTableColumnFlags_WidthFixed, 86.0f);
        ImGui::TableHeadersRow();

        for (const StockPickResearchReportHoldingRow& row : report.holdings) {
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::Text("%s", row.ticker.empty() ? "N/A" : row.ticker.c_str());
            ImGui::TableNextColumn();
            ImGui::TextColored(row.assetName.empty() ? UiTheme::MutedText : UiTheme::TextPrimary, "%s", row.assetName.empty() ? "N/A" : row.assetName.c_str());
            ImGui::TableNextColumn();
            UiTheme::textRightAligned(Money::formatQuantity(row.shares).c_str());
            ImGui::TableNextColumn();
            UiTheme::textRightAligned(optionalMoney(row.currentPrice, row.hasCurrentPrice), row.hasCurrentPrice ? UiTheme::ElectricCyan : UiTheme::MutedText);
            ImGui::TableNextColumn();
            UiTheme::textRightAligned(optionalMoney(row.marketValue, row.hasMarketValue), row.hasMarketValue ? UiTheme::TextPrimary : UiTheme::MutedText);
            ImGui::TableNextColumn();
            UiTheme::textRightAligned(Money::formatPercent(row.currentAllocationPercent).c_str(), UiTheme::TextSecondary);
            ImGui::TableNextColumn();
            UiTheme::textRightAligned(optionalPercent(row.targetAllocationPercent, row.hasTargetAllocation), row.hasTargetAllocation ? UiTheme::TextSecondary : UiTheme::MutedText);
            ImGui::TableNextColumn();
            UiTheme::textRightAligned(optionalPercent(row.allocationDifferencePercent, row.hasAllocationDifference, true), row.hasAllocationDifference ? UiTheme::moneyColor(row.allocationDifferencePercent) : UiTheme::MutedText);
            ImGui::TableNextColumn();
            UiTheme::badge(row.signalStatus.c_str(), signalColor(row.signalStatus));
            ImGui::TableNextColumn();
            UiTheme::textRightAligned(optionalMoney(row.buyLevel, row.hasBuyLevel), row.hasBuyLevel ? UiTheme::Gain : UiTheme::MutedText);
            ImGui::TableNextColumn();
            ImGui::TextColored(row.hasPriority ? UiTheme::TextSecondary : UiTheme::MutedText, "%s", row.hasPriority ? row.priority.c_str() : "N/A");
        }

        ImGui::EndTable();
    }
    UiTheme::popTableStyle();
}

struct PreviewActions {
    bool exportWord = false;
    bool exportPdf = false;
};

PreviewActions drawReportPreview(const StockPickResearchReport& report)
{
    PreviewActions actions;

    beginReportPanel("StockPickResearchReportPreview", "Report Preview", ImVec2(0.0f, 0.0f), UiTheme::ElectricCyan);
    drawPreviewHeader(report);
    drawReportSummary(report);
    ImGui::Spacing();
    drawHoldingsTable(report);
    ImGui::Spacing();
    ImGui::TextColored(UiTheme::MutedText, "For personal research only. Not financial advice.");
    ImGui::Spacing();
    UiTheme::pushButtonStyle(UiTheme::ElectricCyan);
    actions.exportWord = ImGui::Button("Export DOCX", ImVec2(118.0f, 0.0f));
    UiTheme::popButtonStyle();
    ImGui::SameLine();
    UiTheme::pushButtonStyle(UiTheme::NeonMagenta);
    actions.exportPdf = ImGui::Button("Export PDF", ImVec2(104.0f, 0.0f));
    UiTheme::popButtonStyle();
    endReportPanel();

    return actions;
}

} // namespace

void ReportsView::exportPdfPreview(AppState& state)
{
    if (!preview_.has_value()) {
        message_ = "Generate a report preview before exporting PDF.";
        messageIsError_ = true;
        state.setStatus(message_, true);
        return;
    }

    std::string dialogError;
    const std::optional<std::filesystem::path> outputPath =
        choosePdfExportPath(StockPickReportPdfExporter::defaultFileName(*preview_), dialogError);
    if (!outputPath.has_value()) {
        if (!dialogError.empty()) {
            message_ = dialogError;
            messageIsError_ = true;
            state.setStatus(message_, true);
        }
        return;
    }

    const std::filesystem::path exportedPath = StockPickReportExportCommon::withExtension(*outputPath, ".pdf");
    std::string exportError;
    if (StockPickReportPdfExporter::exportPdf(*preview_, exportedPath, exportError)) {
        message_ = "PDF report exported: " + exportedPath.filename().string();
        messageIsError_ = false;
        state.setStatus("PDF report exported.");
        return;
    }

    message_ = exportError.empty() ? "Could not export the PDF report." : exportError;
    messageIsError_ = true;
    state.setStatus(message_, true);
}

void ReportsView::exportWordPreview(AppState& state)
{
    if (!preview_.has_value()) {
        message_ = "Generate a report preview before exporting Word.";
        messageIsError_ = true;
        state.setStatus(message_, true);
        return;
    }

    std::string dialogError;
    const std::optional<std::filesystem::path> outputPath =
        chooseWordExportPath(StockPickReportDocxExporter::defaultFileName(*preview_), dialogError);
    if (!outputPath.has_value()) {
        if (!dialogError.empty()) {
            message_ = dialogError;
            messageIsError_ = true;
            state.setStatus(message_, true);
        }
        return;
    }

    const std::filesystem::path exportedPath = StockPickReportExportCommon::withExtension(*outputPath, ".docx");
    std::string exportError;
    if (StockPickReportDocxExporter::exportDocx(*preview_, exportedPath, exportError)) {
        message_ = "Word report exported: " + exportedPath.filename().string();
        messageIsError_ = false;
        state.setStatus("Word report exported.");
        return;
    }

    message_ = exportError.empty() ? "Could not export the Word report." : exportError;
    messageIsError_ = true;
    state.setStatus(message_, true);
}

void ReportsView::render(AppState& state)
{
    beginReportPanel("ReportsWorkflow", "Report Builder", ImVec2(0.0f, 176.0f), UiTheme::NeonMagenta, ImGuiWindowFlags_NoScrollbar);

    const bool accountChanged = drawAccountSelector(state, selectedAccountId_);
    ImGui::SameLine();
    drawReportTypeSelector();
    ImGui::SameLine();
    const bool periodChanged = drawPeriodSelector(selectedPeriod_);
    if (accountChanged || periodChanged) {
        preview_.reset();
        message_.clear();
    }

    ImGui::Spacing();
    const bool canGenerate = !activeAccounts(state).empty() && selectedAccountId_ > 0;
    if (!canGenerate) {
        ImGui::BeginDisabled();
    }
    UiTheme::pushButtonStyle(UiTheme::ElectricCyan);
    if (ImGui::Button("Generate Preview", ImVec2(156.0f, 0.0f))) {
        preview_ = StockPickReportService::buildResearchReport(state, selectedAccountId_, selectedPeriod_);
        if (preview_.has_value()) {
            message_ = "Preview generated for selected account only.";
            messageIsError_ = false;
            state.setStatus("Report preview generated.");
        } else {
            message_ = "Choose a valid account before generating a preview.";
            messageIsError_ = true;
            state.setStatus(message_, true);
        }
    }
    UiTheme::popButtonStyle();
    if (!canGenerate) {
        ImGui::EndDisabled();
    }

    ImGui::SameLine();
    ImGui::TextColored(UiTheme::MutedText, "Single-account report. No all-account rollups.");

    if (!message_.empty()) {
        ImGui::TextColored(messageIsError_ ? UiTheme::Loss : UiTheme::Gain, "%s", message_.c_str());
    } else {
        ImGui::TextColored(UiTheme::MutedText, "Generate a weekly or monthly stock-pick research preview from local account data.");
    }

    endReportPanel();

    ImGui::Spacing();
    if (preview_.has_value()) {
        const PreviewActions actions = drawReportPreview(*preview_);
        if (actions.exportWord) {
            exportWordPreview(state);
        } else if (actions.exportPdf) {
            exportPdfPreview(state);
        }
    } else {
        beginReportPanel("ReportsEmptyPreview", "Report Preview", ImVec2(0.0f, 0.0f), UiTheme::ElectricCyan);
        UiTheme::emptyState("No report preview yet", "Select one account and generate a weekly or monthly Stock Pick Helper Research Report.");
        ImGui::TextColored(UiTheme::MutedText, "For personal research only. Not financial advice.");
        ImGui::Spacing();
        ImGui::BeginDisabled();
        ImGui::Button("Export DOCX", ImVec2(118.0f, 0.0f));
        ImGui::EndDisabled();
        ImGui::SameLine();
        ImGui::BeginDisabled();
        ImGui::Button("Export PDF", ImVec2(104.0f, 0.0f));
        ImGui::EndDisabled();
        ImGui::SameLine();
        ImGui::TextColored(UiTheme::MutedText, "Generate a preview before exporting.");
        endReportPanel();
    }
}
