// SPDX-License-Identifier: MIT
#include "ui/ImportCsvView.hpp"

#include "app/AppState.hpp"
#include "repositories/HoldingRepository.hpp"
#include "ui/UiTheme.hpp"
#include "util/Money.hpp"

#include <windows.h>
#include <commdlg.h>
#include <imgui.h>
#include <imgui_stdlib.h>

#include <algorithm>
#include <cstring>

namespace {

const char* accountNameFor(const AppState& state, int accountId)
{
    if (accountId <= 0) {
        return "Select account";
    }

    for (const Account& account : state.accounts) {
        if (account.id == accountId) {
            return account.accountName.c_str();
        }
    }

    return "Unknown account";
}

std::string joinErrors(const std::vector<std::string>& errors)
{
    std::string result;
    for (std::size_t index = 0; index < errors.size(); ++index) {
        if (index > 0) {
            result += "; ";
        }
        result += errors[index];
    }
    return result;
}

}

void ImportCsvView::render(AppState& state, HoldingRepository& repository, const std::function<void()>& reloadData)
{
    UiTheme::sectionHeading("Import CSV", "Import holdings from local CSV files with preview, mapping, and validation.");

    ImGui::BeginChild("ImportPrivacy", ImVec2(0.0f, 92.0f), true);
    ImGui::TextColored(UiTheme::Amber, "Privacy");
    ImGui::TextWrapped("CSV files may contain personal financial data. This importer reads the selected local file, does not copy it into the repository, and imports only validated rows into SQLite.");
    ImGui::EndChild();

    ImGui::Spacing();
    ImGui::Text("Import Account");
    drawAccountSelector(state);

    ImGui::Spacing();
    drawFileSection(state);

    if (!table_.headers.empty()) {
        ImGui::Spacing();
        drawHeaderDetection(state);
        ImGui::Spacing();
        drawMappingEditor(state);
        ImGui::Spacing();
        drawPreviewTable();
        ImGui::Spacing();
        drawValidationSummary(state, repository, reloadData);
    }

    if (!importMessage_.empty()) {
        ImGui::TextColored(UiTheme::Gain, "%s", importMessage_.c_str());
    }
}

void ImportCsvView::browseForCsv()
{
    char fileName[MAX_PATH] {};

    OPENFILENAMEA openFileName {};
    openFileName.lStructSize = sizeof(openFileName);
    openFileName.lpstrFile = fileName;
    openFileName.nMaxFile = MAX_PATH;
    openFileName.lpstrFilter = "CSV files (*.csv)\0*.csv\0All files (*.*)\0*.*\0";
    openFileName.nFilterIndex = 1;
    openFileName.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST | OFN_NOCHANGEDIR;

    if (GetOpenFileNameA(&openFileName) == TRUE) {
        csvPath_ = fileName;
        loadError_.clear();
        importMessage_.clear();
    }
}

void ImportCsvView::loadPreview(const AppState& state)
{
    loadError_.clear();
    importMessage_.clear();
    table_ = {};
    previewRows_.clear();

    if (csvPath_.empty()) {
        loadError_ = "Choose a CSV file before loading preview.";
        return;
    }

    if (!HoldingsCsvImport::loadCsvFile(csvPath_, table_, loadError_)) {
        return;
    }

    mapping_ = HoldingsCsvImport::autoMapColumns(table_.headers);
    headerRowInput_ = table_.headerRowNumber;
    rebuildPreview(state);
}

void ImportCsvView::reloadWithHeaderRow(const AppState& state)
{
    loadError_.clear();
    importMessage_.clear();
    table_ = {};
    previewRows_.clear();

    if (csvPath_.empty()) {
        loadError_ = "Choose a CSV file before loading preview.";
        return;
    }

    if (headerRowInput_ <= 0) {
        loadError_ = "Header row must be 1 or greater.";
        return;
    }

    if (!HoldingsCsvImport::loadCsvFile(csvPath_, table_, loadError_, headerRowInput_)) {
        return;
    }

    mapping_ = HoldingsCsvImport::autoMapColumns(table_.headers);
    headerRowInput_ = table_.headerRowNumber;
    rebuildPreview(state);
}

void ImportCsvView::rebuildPreview(const AppState& state)
{
    previewRows_ = HoldingsCsvImport::buildPreview(table_, mapping_, selectedAccountId_, state.holdings);
}

void ImportCsvView::drawAccountSelector(AppState& state)
{
    const int previousAccountId = selectedAccountId_;

    ImGui::BeginChild("ImportAccountSection", ImVec2(0.0f, 74.0f), true);
    if (ImGui::BeginCombo("Import into account", accountNameFor(state, selectedAccountId_))) {
        for (const Account& account : state.accounts) {
            const bool selected = selectedAccountId_ == account.id;
            if (ImGui::Selectable(account.accountName.c_str(), selected)) {
                selectedAccountId_ = account.id;
            }
            if (selected) {
                ImGui::SetItemDefaultFocus();
            }
        }
        ImGui::EndCombo();
    }
    ImGui::EndChild();

    if (selectedAccountId_ != previousAccountId && !table_.headers.empty()) {
        rebuildPreview(state);
    }
}

void ImportCsvView::drawFileSection(const AppState& state)
{
    ImGui::Text("File");
    ImGui::BeginChild("ImportFileSection", ImVec2(0.0f, 92.0f), true);
    ImGui::SetNextItemWidth(520.0f);
    ImGui::InputText("CSV file", &csvPath_);
    ImGui::SameLine();
    if (ImGui::Button("Browse")) {
        browseForCsv();
    }
    ImGui::SameLine();
    if (ImGui::Button("Load Preview")) {
        loadPreview(state);
    }

    if (!loadError_.empty()) {
        ImGui::TextColored(UiTheme::Loss, "%s", loadError_.c_str());
    } else {
        ImGui::TextColored(UiTheme::MutedText, "Source CSV files stay local and are not copied into the repository.");
    }
    ImGui::EndChild();
}

void ImportCsvView::drawHeaderDetection(const AppState& state)
{
    ImGui::Text("Header Detection");
    ImGui::BeginChild("HeaderDetectionSection", ImVec2(0.0f, 106.0f), true);
    ImGui::Text("Detected header row: %d", table_.headerRowNumber);
    ImGui::SameLine();
    ImGui::TextColored(UiTheme::MutedText, "Skipped metadata: %d", table_.skippedMetadataRowCount);
    ImGui::SameLine();
    ImGui::TextColored(UiTheme::MutedText, "Skipped summary: %d", table_.skippedSummaryRowCount);
    ImGui::SameLine();
    ImGui::TextColored(UiTheme::MutedText, "Skipped blank: %d", table_.skippedBlankRowCount);

    ImGui::SetNextItemWidth(150.0f);
    ImGui::InputInt("Header row", &headerRowInput_);
    ImGui::SameLine();
    if (ImGui::Button("Apply Header Row")) {
        reloadWithHeaderRow(state);
    }
    ImGui::TextColored(UiTheme::MutedText, "Use this only when automatic detection picked the wrong row.");
    ImGui::EndChild();
}

bool ImportCsvView::drawImportFieldCombo(std::size_t columnIndex)
{
    bool changed = false;
    const HoldingsCsvImportField currentField = columnIndex < mapping_.fields.size()
        ? mapping_.fields[columnIndex]
        : HoldingsCsvImportField::Ignore;

    ImGui::SetNextItemWidth(190.0f);
    const std::string comboId = std::string("##ImportField") + std::to_string(columnIndex);
    if (ImGui::BeginCombo(comboId.c_str(), HoldingsCsvImport::importFieldLabel(currentField))) {
        static constexpr HoldingsCsvImportField fieldOptions[] {
            HoldingsCsvImportField::Ignore,
            HoldingsCsvImportField::Ticker,
            HoldingsCsvImportField::AssetName,
            HoldingsCsvImportField::Shares,
            HoldingsCsvImportField::CurrentPrice,
            HoldingsCsvImportField::AverageCost,
            HoldingsCsvImportField::TotalCostBasis,
            HoldingsCsvImportField::GainLossDollar,
            HoldingsCsvImportField::GainLossPercent,
            HoldingsCsvImportField::AssetType,
            HoldingsCsvImportField::Notes,
        };

        for (const HoldingsCsvImportField option : fieldOptions) {
            const bool selected = currentField == option;
            if (ImGui::Selectable(HoldingsCsvImport::importFieldLabel(option), selected)) {
                setColumnMapping(columnIndex, option);
                changed = true;
            }
            if (selected) {
                ImGui::SetItemDefaultFocus();
            }
        }

        ImGui::EndCombo();
    }

    return changed;
}

void ImportCsvView::drawMappingEditor(const AppState& state)
{
    ImGui::Text("Column Mapping");
    ImGui::TextColored(UiTheme::MutedText, "Map each CSV column to the field it should become in Investor Command Center. Columns set to Ignore will not be imported.");

    bool changed = false;
    if (ImGui::BeginTable("ColumnMappingTable", 5, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable | ImGuiTableFlags_ScrollY, ImVec2(0.0f, 330.0f))) {
        ImGui::TableSetupColumn("CSV Column Header", ImGuiTableColumnFlags_WidthFixed, 220.0f);
        ImGui::TableSetupColumn("CSV Sample Value", ImGuiTableColumnFlags_WidthFixed, 180.0f);
        ImGui::TableSetupColumn("Import Field", ImGuiTableColumnFlags_WidthFixed, 220.0f);
        ImGui::TableSetupColumn("Parsed/Calculated Preview", ImGuiTableColumnFlags_WidthStretch, 1.0f);
        ImGui::TableSetupColumn("Status", ImGuiTableColumnFlags_WidthFixed, 92.0f);
        ImGui::TableSetupScrollFreeze(0, 1);
        ImGui::TableHeadersRow();

        for (std::size_t index = 0; index < table_.headers.size(); ++index) {
            const std::string headerLabel = table_.headers[index].empty()
                ? "(blank column " + std::to_string(index + 1) + ")"
                : table_.headers[index];
            const HoldingsCsvImportField field = index < mapping_.fields.size()
                ? mapping_.fields[index]
                : HoldingsCsvImportField::Ignore;

            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::TextWrapped("%s", headerLabel.c_str());
            ImGui::TableNextColumn();
            ImGui::TextWrapped("%s", sampleValueForColumn(index).c_str());
            ImGui::TableNextColumn();
            changed = drawImportFieldCombo(index) || changed;
            ImGui::TableNextColumn();
            ImGui::TextWrapped("%s", mappingPreviewForField(field).c_str());
            ImGui::TableNextColumn();
            const char* status = mappingStatusForColumn(index);
            const ImVec4 color = std::strcmp(status, "OK") == 0
                ? UiTheme::Gain
                : (std::strcmp(status, "Ignored") == 0 ? UiTheme::MutedText : UiTheme::Amber);
            ImGui::TextColored(color, "%s", status);
        }

        ImGui::EndTable();
    }

    if (changed) {
        rebuildPreview(state);
    }
}

void ImportCsvView::drawPreviewTable()
{
    ImGui::Text("Import Preview");
    if (previewRows_.empty()) {
        UiTheme::emptyState("No rows to preview", "Load a CSV with at least one data row.");
        return;
    }

    if (ImGui::BeginTable("ImportPreviewTable", 14, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable | ImGuiTableFlags_ScrollX | ImGuiTableFlags_ScrollY, ImVec2(0.0f, 360.0f))) {
        ImGui::TableSetupColumn("Row", ImGuiTableColumnFlags_WidthFixed, 52.0f);
        ImGui::TableSetupColumn("Status", ImGuiTableColumnFlags_WidthFixed, 86.0f);
        ImGui::TableSetupColumn("Ticker", ImGuiTableColumnFlags_WidthFixed, 76.0f);
        ImGui::TableSetupColumn("Asset Name", ImGuiTableColumnFlags_WidthFixed, 180.0f);
        ImGui::TableSetupColumn("Asset Type", ImGuiTableColumnFlags_WidthFixed, 104.0f);
        ImGui::TableSetupColumn("Shares", ImGuiTableColumnFlags_WidthFixed, 92.0f);
        ImGui::TableSetupColumn("Current Price", ImGuiTableColumnFlags_WidthFixed, 112.0f);
        ImGui::TableSetupColumn("Total Cost Basis", ImGuiTableColumnFlags_WidthFixed, 132.0f);
        ImGui::TableSetupColumn("Average Cost", ImGuiTableColumnFlags_WidthFixed, 118.0f);
        ImGui::TableSetupColumn("Market Value", ImGuiTableColumnFlags_WidthFixed, 118.0f);
        ImGui::TableSetupColumn("Gain/Loss $", ImGuiTableColumnFlags_WidthFixed, 116.0f);
        ImGui::TableSetupColumn("Gain/Loss %", ImGuiTableColumnFlags_WidthFixed, 104.0f);
        ImGui::TableSetupColumn("Notes", ImGuiTableColumnFlags_WidthFixed, 180.0f);
        ImGui::TableSetupColumn("Errors/Warnings", ImGuiTableColumnFlags_WidthStretch, 1.5f);
        ImGui::TableSetupScrollFreeze(0, 1);
        ImGui::TableHeadersRow();

        for (const HoldingsCsvPreviewRow& row : previewRows_) {
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::Text("%d", row.sourceRowNumber);
            ImGui::TableNextColumn();
            UiTheme::badge(row.valid() ? "Ready" : "Blocked", row.valid() ? UiTheme::Gain : UiTheme::Loss);
            ImGui::TableNextColumn();
            ImGui::Text("%s", row.holding.ticker.c_str());
            ImGui::TableNextColumn();
            ImGui::Text("%s", row.holding.assetName.c_str());
            ImGui::TableNextColumn();
            ImGui::TextColored(UiTheme::MutedText, "%s", row.holding.assetType.c_str());
            ImGui::TableNextColumn();
            ImGui::Text("%s", Money::formatQuantity(row.holding.shares).c_str());
            ImGui::TableNextColumn();
            ImGui::Text("%s", Money::format(row.holding.currentPrice).c_str());
            ImGui::TableNextColumn();
            const std::string totalCostBasisText = row.hasTotalCostBasis ? Money::format(row.totalCostBasis) : "n/a";
            ImGui::Text("%s", totalCostBasisText.c_str());
            ImGui::TableNextColumn();
            ImGui::Text("%s", Money::format(row.holding.averageCost).c_str());
            ImGui::TableNextColumn();
            ImGui::Text("%s", Money::format(row.marketValue).c_str());
            ImGui::TableNextColumn();
            ImGui::TextColored(UiTheme::moneyColor(row.gainLossDollar), "%s", Money::format(row.gainLossDollar).c_str());
            ImGui::TableNextColumn();
            ImGui::TextColored(UiTheme::moneyColor(row.gainLossDollar), "%s", Money::formatPercent(row.gainLossPercent, true).c_str());
            ImGui::TableNextColumn();
            ImGui::TextColored(UiTheme::MutedText, "%s", row.holding.notes.c_str());
            ImGui::TableNextColumn();
            const std::string warnings = joinErrors(row.warnings);
            const std::string errors = joinErrors(row.errors);
            const std::string messages = errors.empty()
                ? warnings
                : (warnings.empty() ? errors : errors + "; " + warnings);
            ImGui::TextColored(row.valid() ? (row.warnings.empty() ? UiTheme::MutedText : UiTheme::Amber) : UiTheme::Loss, "%s", messages.c_str());
        }

        ImGui::EndTable();
    }
}

void ImportCsvView::drawValidationSummary(AppState& state, HoldingRepository& repository, const std::function<void()>& reloadData)
{
    const int validRows = static_cast<int>(std::count_if(previewRows_.begin(), previewRows_.end(), [](const HoldingsCsvPreviewRow& row) {
        return row.valid();
    }));
    const int blockedRows = static_cast<int>(previewRows_.size()) - validRows;
    const int warningRows = static_cast<int>(std::count_if(previewRows_.begin(), previewRows_.end(), [](const HoldingsCsvPreviewRow& row) {
        return !row.warnings.empty();
    }));

    ImGui::Text("Validation Summary");
    ImGui::BeginChild("ImportStats", ImVec2(0.0f, 106.0f), true);
    ImGui::TextColored(UiTheme::Gain, "Valid rows: %d", validRows);
    ImGui::SameLine();
    ImGui::TextColored(blockedRows > 0 ? UiTheme::Loss : UiTheme::MutedText, "Blocked rows: %d", blockedRows);
    ImGui::SameLine();
    ImGui::TextColored(warningRows > 0 ? UiTheme::Amber : UiTheme::MutedText, "Rows with warnings: %d", warningRows);

    ImGui::TextColored(UiTheme::MutedText, "Required: ticker, shares, and current price. Average cost can be imported directly or calculated from total cost basis.");
    ImGui::BeginDisabled(validRows == 0);
    if (ImGui::Button("Import Valid Rows")) {
        importValidRows(state, repository, reloadData);
    }
    ImGui::EndDisabled();
    ImGui::EndChild();
}

std::string ImportCsvView::sampleValueForColumn(std::size_t columnIndex) const
{
    for (const CsvDataRow& row : table_.rows) {
        if (columnIndex < row.cells.size() && !row.cells[columnIndex].empty()) {
            return row.cells[columnIndex];
        }
    }

    return "(blank)";
}

std::string ImportCsvView::mappingPreviewForField(HoldingsCsvImportField field) const
{
    if (field == HoldingsCsvImportField::Ignore) {
        return "Ignored";
    }

    if (previewRows_.empty()) {
        return "No preview row";
    }

    const HoldingsCsvPreviewRow& row = previewRows_.front();
    switch (field) {
    case HoldingsCsvImportField::Ticker:
        return row.holding.ticker;
    case HoldingsCsvImportField::AssetName:
        return row.holding.assetName;
    case HoldingsCsvImportField::Shares:
        return Money::formatQuantity(row.holding.shares);
    case HoldingsCsvImportField::CurrentPrice:
        return Money::formatNumber(row.holding.currentPrice, 4);
    case HoldingsCsvImportField::AverageCost:
        return Money::format(row.holding.averageCost);
    case HoldingsCsvImportField::TotalCostBasis:
        if (row.hasTotalCostBasis) {
            return "Average Cost = " + Money::format(row.totalCostBasis) + " / shares = " + Money::format(row.holding.averageCost);
        }
        return "Average cost unavailable";
    case HoldingsCsvImportField::GainLossDollar:
        if (row.hasSourceGainLossDollar) {
            return "CSV reference: " + Money::format(row.sourceGainLossDollar);
        }
        return "Reference only; app recalculates";
    case HoldingsCsvImportField::GainLossPercent:
        if (row.hasSourceGainLossPercent) {
            return "CSV reference: " + Money::formatPercent(row.sourceGainLossPercent, true);
        }
        return "Reference only; app recalculates";
    case HoldingsCsvImportField::AssetType:
        return row.holding.assetType;
    case HoldingsCsvImportField::Notes:
        return row.holding.notes.empty() ? "(blank)" : row.holding.notes;
    }

    return {};
}

const char* ImportCsvView::mappingStatusForColumn(std::size_t columnIndex) const
{
    if (columnIndex >= mapping_.fields.size()) {
        return "Review";
    }

    const HoldingsCsvImportField field = mapping_.fields[columnIndex];
    if (field == HoldingsCsvImportField::Ignore) {
        return "Ignored";
    }

    const std::string sample = sampleValueForColumn(columnIndex);
    if (sample == "(blank)" || sample == "--") {
        return "Review";
    }

    return "OK";
}

void ImportCsvView::setColumnMapping(std::size_t columnIndex, HoldingsCsvImportField field)
{
    if (mapping_.fields.size() < table_.headers.size()) {
        mapping_.fields.resize(table_.headers.size(), HoldingsCsvImportField::Ignore);
    }

    if (field != HoldingsCsvImportField::Ignore) {
        for (std::size_t index = 0; index < mapping_.fields.size(); ++index) {
            if (index != columnIndex && mapping_.fields[index] == field) {
                mapping_.fields[index] = HoldingsCsvImportField::Ignore;
            }
        }
    }

    if (columnIndex < mapping_.fields.size()) {
        mapping_.fields[columnIndex] = field;
    }
}

void ImportCsvView::importValidRows(AppState& state, HoldingRepository& repository, const std::function<void()>& reloadData)
{
    int imported = 0;
    int failed = 0;

    for (const HoldingsCsvPreviewRow& row : previewRows_) {
        if (!row.valid()) {
            continue;
        }

        Holding holding = row.holding;
        std::string error;
        if (repository.create(holding, error)) {
            ++imported;
        } else {
            ++failed;
        }
    }

    reloadData();
    importMessage_ = "Imported " + std::to_string(imported) + " holdings.";
    if (failed > 0) {
        state.setStatus(std::to_string(failed) + " holdings failed during import.", true);
    } else {
        state.setStatus(importMessage_);
    }

    rebuildPreview(state);
}
