// SPDX-License-Identifier: MIT
#include "ui/HoldingsView.hpp"

#include "app/AppState.hpp"
#include "repositories/HoldingRepository.hpp"
#include "services/PortfolioCalculator.hpp"
#include "ui/UiTheme.hpp"
#include "util/Money.hpp"

#include <imgui.h>
#include <imgui_stdlib.h>

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <map>
#include <string>
#include <vector>

namespace {

constexpr const char* HoldingEditorPopup = "Holding Editor";
constexpr const char* DeleteHoldingPopup = "Delete Holding";
constexpr const char* NewHoldingEditorPopup = "Holding Editor###holding_edit_popup_new";
constexpr double FullTargetAllocationPercent = 100.0;
constexpr float Pi = 3.14159265358979323846f;
constexpr int AllocationStatusColumn = 16;

struct AllocationSlice {
    std::string label;
    double value = 0.0;
    ImVec4 color = UiTheme::ChartCyan;
};

struct HoldingTableRow {
    const Holding* holding = nullptr;
    std::string accountName;
    HoldingMetrics metrics;
    HoldingAllocationMetrics allocation;
};

std::string holdingEditorPopupId(int holdingId)
{
    return "Holding Editor###holding_edit_popup_" + std::to_string(holdingId);
}

std::string holdingDeletePopupId(int holdingId)
{
    return "Delete Holding###holding_delete_popup_" + std::to_string(holdingId);
}

std::string lowerCopy(std::string value)
{
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char character) {
        return static_cast<char>(std::tolower(character));
    });
    return value;
}

bool containsCaseInsensitive(const std::string& value, const std::string& query)
{
    return lowerCopy(value).find(lowerCopy(query)) != std::string::npos;
}

bool matchesFilter(const Holding& holding, const std::string& query, const char* accountName)
{
    if (query.empty()) {
        return true;
    }

    return containsCaseInsensitive(holding.ticker, query) ||
        containsCaseInsensitive(holding.assetName, query) ||
        containsCaseInsensitive(holding.assetType, query) ||
        containsCaseInsensitive(accountName, query) ||
        containsCaseInsensitive(holding.notes, query);
}

bool isActiveAccount(const Account& account)
{
    return account.status.empty() || account.status == "Active";
}

bool isActiveHolding(const Holding& holding)
{
    return holding.status.empty() || holding.status == "Active";
}

ImVec4 allocationStatusColor(AllocationStatus status)
{
    switch (status) {
    case AllocationStatus::Overweight:
        return UiTheme::Loss;
    case AllocationStatus::Underweight:
        return UiTheme::Amber;
    case AllocationStatus::OnTarget:
        return UiTheme::Gain;
    }

    return UiTheme::MutedText;
}

int allocationStatusPriority(AllocationStatus status)
{
    switch (status) {
    case AllocationStatus::Overweight:
        return 0;
    case AllocationStatus::Underweight:
        return 1;
    case AllocationStatus::OnTarget:
        return 2;
    }

    return 3;
}

std::array<ImVec4, 8> allocationPalette()
{
    return {
        UiTheme::ChartCyan,
        UiTheme::ChartMagenta,
        UiTheme::ChartGreen,
        UiTheme::ChartAmber,
        UiTheme::SoftBlue,
        ImVec4(0.490f, 0.408f, 1.0f, 1.0f),
        ImVec4(0.157f, 0.827f, 0.773f, 1.0f),
        ImVec4(1.0f, 0.506f, 0.286f, 1.0f),
    };
}

std::vector<const Account*> selectableAccounts(const AppState& state)
{
    std::vector<const Account*> accounts;
    for (const Account& account : state.accounts) {
        if (isActiveAccount(account)) {
            accounts.push_back(&account);
        }
    }

    if (accounts.empty()) {
        for (const Account& account : state.accounts) {
            accounts.push_back(&account);
        }
    }

    return accounts;
}

const Account* accountForId(const std::vector<const Account*>& accounts, int accountId)
{
    for (const Account* account : accounts) {
        if (account != nullptr && account->id == accountId) {
            return account;
        }
    }

    return nullptr;
}

const Account* ensureSelectedAccount(const AppState& state, int& selectedAccountId)
{
    const std::vector<const Account*> accounts = selectableAccounts(state);
    if (accounts.empty()) {
        selectedAccountId = 0;
        return nullptr;
    }

    if (const Account* selectedAccount = accountForId(accounts, selectedAccountId)) {
        return selectedAccount;
    }

    selectedAccountId = accounts.front()->id;
    return accounts.front();
}

std::vector<Holding> holdingsForAccount(const AppState& state, int accountId)
{
    std::vector<Holding> holdings;
    for (const Holding& holding : state.holdings) {
        if (holding.accountId == accountId) {
            holdings.push_back(holding);
        }
    }
    return holdings;
}

PortfolioSummary accountPortfolioSummary(const AppState& state, const Account* selectedAccount)
{
    if (selectedAccount == nullptr) {
        return {};
    }

    return PortfolioCalculator::calculateSummary(
        std::vector<Account> { *selectedAccount },
        holdingsForAccount(state, selectedAccount->id));
}

double assignedTargetAllocationPercent(const AppState& state, int accountId)
{
    double total = 0.0;
    for (const Holding& holding : state.holdings) {
        if (holding.accountId == accountId && isActiveHolding(holding)) {
            total += holding.targetAllocationPercent;
        }
    }
    return total;
}

std::vector<AllocationSlice> totalAllocationSlices(const AppState& state, int accountId, const PortfolioSummary& summary)
{
    std::map<std::string, double> positionValuesByTicker;
    for (const Holding& holding : state.holdings) {
        if (holding.accountId != accountId || !isActiveHolding(holding)) {
            continue;
        }

        const double marketValue = PortfolioCalculator::calculateHolding(holding).marketValue;
        if (marketValue > 0.0) {
            positionValuesByTicker[holding.ticker] += marketValue;
        }
    }

    std::vector<AllocationSlice> slices;
    const std::array<ImVec4, 8> palette = allocationPalette();
    int colorIndex = 0;
    for (const auto& [ticker, marketValue] : positionValuesByTicker) {
        slices.push_back(AllocationSlice { ticker, marketValue, palette[static_cast<std::size_t>(colorIndex) % palette.size()] });
        ++colorIndex;
    }

    if (summary.cashBalance > 0.0) {
        slices.push_back(AllocationSlice { "Cash", summary.cashBalance, UiTheme::MutedText });
    }

    std::sort(slices.begin(), slices.end(), [](const AllocationSlice& left, const AllocationSlice& right) {
        return left.value > right.value;
    });
    return slices;
}

const Account* drawAccountSelector(const AppState& state, int& selectedAccountId)
{
    const std::vector<const Account*> accounts = selectableAccounts(state);
    if (accounts.empty()) {
        ImGui::TextColored(UiTheme::MutedText, "Open positions, cost basis, cash, and allocation for the selected account.");
        ImGui::TextColored(UiTheme::Amber, "Create an account to view positions.");
        selectedAccountId = 0;
        return nullptr;
    }

    const Account* selectedAccount = ensureSelectedAccount(state, selectedAccountId);

    const float availableWidth = ImGui::GetContentRegionAvail().x;
    ImGui::TextColored(UiTheme::MutedText, "Open positions, cost basis, cash, and allocation for the selected account.");
    if (availableWidth >= 720.0f) {
        ImGui::SameLine();
        ImGui::SetCursorPosX(std::max(ImGui::GetCursorPosX(), ImGui::GetWindowContentRegionMax().x - 340.0f));
    }
    ImGui::SetNextItemWidth(320.0f);
    const char* preview = selectedAccount == nullptr ? "Select account" : selectedAccount->accountName.c_str();
    if (ImGui::BeginCombo("Account##HoldingsAccountSelector", preview)) {
        for (const Account* account : accounts) {
            if (account == nullptr) {
                continue;
            }

            const bool selected = account->id == selectedAccountId;
            const std::string label = account->accountName + " (" + account->accountType + ")";
            if (ImGui::Selectable(label.c_str(), selected)) {
                selectedAccountId = account->id;
                selectedAccount = account;
            }
            if (selected) {
                ImGui::SetItemDefaultFocus();
            }
        }
        ImGui::EndCombo();
    }

    return ensureSelectedAccount(state, selectedAccountId);
}

void drawSummaryMetric(const char* label, const std::string& value, ImVec4 color)
{
    ImGui::TextColored(UiTheme::MutedText, "%s", label);
    UiTheme::pushNumericFont();
    ImGui::TextColored(color, "%s", value.c_str());
    UiTheme::popNumericFont();
}

void drawAllocationSummary(const PortfolioSummary& summary, double assignedTargetPercent)
{
    const float availableWidth = ImGui::GetContentRegionAvail().x;
    const int columnCount = availableWidth < 760.0f ? 2 : 4;
    const float height = columnCount == 2 ? 134.0f : 96.0f;

    UiTheme::pushPanelStyle();
    ImGui::BeginChild("AllocationSummaryStrip", ImVec2(0.0f, height), true, ImGuiWindowFlags_NoScrollbar);
    if (ImGui::BeginTable("AllocationSummaryMetrics", columnCount, ImGuiTableFlags_SizingStretchSame)) {
        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        drawSummaryMetric("Total Account Value", Money::format(summary.accountBalance), UiTheme::TextPrimary);
        ImGui::TableNextColumn();
        drawSummaryMetric("Positions Value", Money::format(summary.holdingsMarketValue), UiTheme::ElectricCyan);
        ImGui::TableNextColumn();
        drawSummaryMetric("Cash Balance", Money::format(summary.cashBalance), UiTheme::TextSecondary);
        ImGui::TableNextColumn();
        drawSummaryMetric("Assigned Target", Money::formatPercent(assignedTargetPercent), assignedTargetPercent > FullTargetAllocationPercent ? UiTheme::Loss : UiTheme::TextSecondary);
        ImGui::EndTable();
    }

    if (assignedTargetPercent > FullTargetAllocationPercent) {
        ImGui::TextColored(UiTheme::Loss, "Assigned target allocations exceed 100%.");
    } else if (assignedTargetPercent < FullTargetAllocationPercent) {
        ImGui::TextColored(UiTheme::MutedText, "Assigned targets are below 100%; unassigned allocation may represent cash or unallocated capital.");
    }
    ImGui::EndChild();
    UiTheme::popPanelStyle();
}

void drawPieSlice(ImDrawList* drawList, ImVec2 center, float radius, float startAngle, float endAngle, ImU32 color)
{
    const float angleSpan = endAngle - startAngle;
    const int segments = std::max(3, static_cast<int>(std::ceil(angleSpan / (2.0f * Pi) * 96.0f)));
    const float step = angleSpan / static_cast<float>(segments);

    for (int segment = 0; segment < segments; ++segment) {
        const float angleA = startAngle + step * static_cast<float>(segment);
        const float angleB = segment == segments - 1 ? endAngle : angleA + step;
        const ImVec2 pointA(center.x + std::cos(angleA) * radius, center.y + std::sin(angleA) * radius);
        const ImVec2 pointB(center.x + std::cos(angleB) * radius, center.y + std::sin(angleB) * radius);
        drawList->AddTriangleFilled(center, pointA, pointB, color);
    }
}

void drawAllocationLegend(const std::vector<AllocationSlice>& slices, double totalValue)
{
    constexpr int MaxLegendRows = 8;
    const int visibleRows = std::min(MaxLegendRows, static_cast<int>(slices.size()));

    for (int index = 0; index < visibleRows; ++index) {
        const AllocationSlice& slice = slices[static_cast<std::size_t>(index)];
        const ImVec2 cursor = ImGui::GetCursorScreenPos();
        ImDrawList* drawList = ImGui::GetWindowDrawList();
        drawList->AddRectFilled(cursor, ImVec2(cursor.x + 10.0f, cursor.y + 10.0f), ImGui::GetColorU32(slice.color), 3.0f);
        ImGui::Dummy(ImVec2(14.0f, 10.0f));
        ImGui::SameLine();
        const double percent = totalValue == 0.0 ? 0.0 : slice.value / totalValue * 100.0;
        ImGui::TextColored(UiTheme::TextSecondary, "%s", slice.label.c_str());
        ImGui::SameLine();
        UiTheme::pushNumericFont();
        ImGui::TextColored(UiTheme::MutedText, "%s", Money::formatPercent(percent).c_str());
        UiTheme::popNumericFont();
    }

    if (static_cast<int>(slices.size()) > MaxLegendRows) {
        ImGui::TextColored(UiTheme::MutedText, "+%d more positions", static_cast<int>(slices.size()) - MaxLegendRows);
    }
}

void drawTotalAllocationCard(const std::vector<AllocationSlice>& slices, double totalValue, ImVec2 size)
{
    ImGui::PushStyleColor(ImGuiCol_ChildBg, UiTheme::CardBackground);
    ImGui::PushStyleColor(ImGuiCol_Border, UiTheme::withAlpha(UiTheme::ElectricCyan, 0.16f));
    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 16.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_ChildBorderSize, 1.0f);
    ImGui::BeginChild("TotalAllocationCard", size, true, ImGuiWindowFlags_NoScrollbar);

    ImGui::TextColored(UiTheme::TextPrimary, "Total Allocation");
    ImGui::TextColored(UiTheme::MutedText, "Positions plus cash");
    ImGui::Spacing();

    if (slices.empty() || totalValue <= 0.0) {
        ImGui::TextColored(UiTheme::MutedText, "No active positions or cash to chart.");
        ImGui::EndChild();
        ImGui::PopStyleVar(2);
        ImGui::PopStyleColor(2);
        return;
    }

    const ImVec2 cardPos = ImGui::GetCursorScreenPos();
    const float cardWidth = ImGui::GetContentRegionAvail().x;
    const float chartRadius = std::min(72.0f, std::max(48.0f, cardWidth * 0.18f));
    const ImVec2 center(cardPos.x + chartRadius + 12.0f, cardPos.y + chartRadius + 8.0f);
    ImDrawList* drawList = ImGui::GetWindowDrawList();

    float startAngle = -Pi * 0.5f;
    for (const AllocationSlice& slice : slices) {
        const float endAngle = startAngle + static_cast<float>(slice.value / totalValue) * 2.0f * Pi;
        drawPieSlice(drawList, center, chartRadius, startAngle, endAngle, ImGui::GetColorU32(slice.color));
        startAngle = endAngle;
    }
    drawList->AddCircle(center, chartRadius, ImGui::GetColorU32(UiTheme::withAlpha(UiTheme::BorderSubtle, 0.42f)), 96, 1.5f);
    drawList->AddCircleFilled(center, chartRadius * 0.42f, ImGui::GetColorU32(UiTheme::SurfaceMain), 64);
    drawList->AddText(ImVec2(center.x - 28.0f, center.y - 8.0f), ImGui::GetColorU32(UiTheme::TextPrimary), "100%");

    ImGui::Dummy(ImVec2(chartRadius * 2.0f + 26.0f, chartRadius * 2.0f + 18.0f));
    ImGui::SameLine();
    ImGui::BeginGroup();
    UiTheme::pushNumericFont();
    ImGui::TextColored(UiTheme::ElectricCyan, "%s", Money::format(totalValue).c_str());
    UiTheme::popNumericFont();
    drawAllocationLegend(slices, totalValue);
    ImGui::EndGroup();

    ImGui::EndChild();
    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor(2);
}

void drawSectorPlaceholderCard(ImVec2 size)
{
    ImGui::PushStyleColor(ImGuiCol_ChildBg, UiTheme::CardBackground);
    ImGui::PushStyleColor(ImGuiCol_Border, UiTheme::withAlpha(UiTheme::NeonMagenta, 0.14f));
    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 16.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_ChildBorderSize, 1.0f);
    ImGui::BeginChild("SectorAllocationCard", size, true, ImGuiWindowFlags_NoScrollbar);

    ImGui::TextColored(UiTheme::TextPrimary, "Industry / Sector Allocation");
    UiTheme::badge("Planned", UiTheme::Amber);
    ImGui::Spacing();

    const ImVec2 origin = ImGui::GetCursorScreenPos();
    const float width = ImGui::GetContentRegionAvail().x;
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    const ImVec2 center(origin.x + 78.0f, origin.y + 82.0f);
    drawList->AddCircle(center, 58.0f, ImGui::GetColorU32(UiTheme::withAlpha(UiTheme::NeonMagenta, 0.34f)), 72, 2.0f);
    drawList->AddCircle(center, 34.0f, ImGui::GetColorU32(UiTheme::withAlpha(UiTheme::ElectricCyan, 0.22f)), 72, 2.0f);
    drawList->AddLine(ImVec2(center.x - 58.0f, center.y), ImVec2(center.x + 58.0f, center.y), ImGui::GetColorU32(UiTheme::withAlpha(UiTheme::BorderSubtle, 0.26f)), 1.0f);
    drawList->AddLine(ImVec2(center.x, center.y - 58.0f), ImVec2(center.x, center.y + 58.0f), ImGui::GetColorU32(UiTheme::withAlpha(UiTheme::BorderSubtle, 0.26f)), 1.0f);

    ImGui::Dummy(ImVec2(156.0f, 154.0f));
    ImGui::SameLine();
    ImGui::BeginGroup();
    ImGui::TextColored(UiTheme::MutedText, "Sector allocation is planned.");
    ImGui::TextColored(UiTheme::TextSecondary, "The card is reserved for future sector and industry data.");
    const float barWidth = std::max(80.0f, width - 210.0f);
    for (int index = 0; index < 3; ++index) {
        const ImVec2 barMin = ImGui::GetCursorScreenPos();
        const float fill = barWidth * (0.72f - static_cast<float>(index) * 0.18f);
        drawList->AddRectFilled(barMin, ImVec2(barMin.x + barWidth, barMin.y + 8.0f), ImGui::GetColorU32(UiTheme::withAlpha(UiTheme::BorderSubtle, 0.10f)), 4.0f);
        drawList->AddRectFilled(barMin, ImVec2(barMin.x + fill, barMin.y + 8.0f), ImGui::GetColorU32(UiTheme::withAlpha(index == 0 ? UiTheme::NeonMagenta : UiTheme::ElectricCyan, 0.30f)), 4.0f);
        ImGui::Dummy(ImVec2(barWidth, 16.0f));
    }
    ImGui::EndGroup();

    ImGui::EndChild();
    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor(2);
}

void drawSnapshotArea(const AppState& state, const Account* selectedAccount, const PortfolioSummary& summary)
{
    const int accountId = selectedAccount == nullptr ? 0 : selectedAccount->id;
    drawAllocationSummary(summary, assignedTargetAllocationPercent(state, accountId));
    ImGui::Spacing();

    const std::vector<AllocationSlice> slices = totalAllocationSlices(state, accountId, summary);
    const float availableWidth = ImGui::GetContentRegionAvail().x;
    const bool stacked = availableWidth < 820.0f;
    const float spacing = ImGui::GetStyle().ItemSpacing.x;
    const float cardWidth = stacked ? availableWidth : (availableWidth - spacing) * 0.5f;
    const ImVec2 cardSize(cardWidth, 248.0f);

    drawTotalAllocationCard(slices, summary.accountBalance, cardSize);
    if (!stacked) {
        ImGui::SameLine();
    }
    drawSectorPlaceholderCard(cardSize);
}

std::vector<HoldingTableRow> filteredHoldingRows(const AppState& state, int accountId, const std::string& searchText, double totalInvestmentAccountValue)
{
    std::vector<HoldingTableRow> rows;
    for (const Holding& holding : state.holdings) {
        if (holding.accountId != accountId) {
            continue;
        }

        const char* accountName = "Unknown account";
        for (const Account& account : state.accounts) {
            if (account.id == holding.accountId) {
                accountName = account.accountName.c_str();
                break;
            }
        }

        if (!matchesFilter(holding, searchText, accountName)) {
            continue;
        }

        rows.push_back(HoldingTableRow {
            &holding,
            accountName,
            PortfolioCalculator::calculateHolding(holding),
            PortfolioCalculator::calculateHoldingAllocation(holding, totalInvestmentAccountValue),
        });
    }

    return rows;
}

void sortRowsByAllocationStatus(std::vector<HoldingTableRow>& rows)
{
    std::stable_sort(rows.begin(), rows.end(), [](const HoldingTableRow& left, const HoldingTableRow& right) {
        const int leftPriority = allocationStatusPriority(left.allocation.status);
        const int rightPriority = allocationStatusPriority(right.allocation.status);
        if (leftPriority != rightPriority) {
            return leftPriority < rightPriority;
        }
        return lowerCopy(left.holding->ticker) < lowerCopy(right.holding->ticker);
    });
}

template <std::size_t Size>
void drawStringCombo(const char* label, std::string& value, const std::array<const char*, Size>& options)
{
    const char* preview = value.empty() ? options[0] : value.c_str();
    if (ImGui::BeginCombo(label, preview)) {
        for (const char* option : options) {
            const bool selected = value == option;
            if (ImGui::Selectable(option, selected)) {
                value = option;
            }
            if (selected) {
                ImGui::SetItemDefaultFocus();
            }
        }
        ImGui::EndCombo();
    }
}

}

void HoldingsView::render(AppState& state, HoldingRepository& repository, const std::function<void()>& reloadData)
{
    const Account* selectedAccount = drawAccountSelector(state, selectedAccountId_);
    const PortfolioSummary portfolioSummary = accountPortfolioSummary(state, selectedAccount);
    drawSnapshotArea(state, selectedAccount, portfolioSummary);

    ImGui::Spacing();

    UiTheme::pushButtonStyle(UiTheme::NeonMagenta);
    if (state.accounts.empty()) {
        ImGui::BeginDisabled();
        ImGui::Button("Add Holding");
        ImGui::EndDisabled();
        ImGui::SameLine();
        ImGui::TextColored(UiTheme::MutedText, "Create an account first.");
    } else if (ImGui::Button("Add Holding")) {
        openCreate(state);
        openEditorPopup_ = true;
    }
    UiTheme::popButtonStyle();
    ImGui::SameLine();
    ImGui::SetNextItemWidth(300.0f);
    ImGui::InputTextWithHint("##HoldingSearch", "Search holdings", &searchText_);

    ImGui::Spacing();

    const int selectedAccountId = selectedAccount == nullptr ? 0 : selectedAccount->id;
    std::vector<HoldingTableRow> rows = filteredHoldingRows(state, selectedAccountId, searchText_, portfolioSummary.accountBalance);

    UiTheme::pushPanelStyle();
    ImGui::BeginChild("HoldingsTablePanel", ImVec2(0.0f, 0.0f), true);
    if (state.holdings.empty()) {
        UiTheme::emptyState("No holdings yet", "Add a holding after creating at least one account.");
    } else {
        UiTheme::pushTableStyle();
        if (ImGui::BeginTable("HoldingsTable", 18, ImGuiTableFlags_BordersInnerH | ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable | ImGuiTableFlags_Reorderable | ImGuiTableFlags_Hideable | ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_ScrollX | ImGuiTableFlags_Sortable)) {
        ImGui::TableSetupColumn("Ticker", ImGuiTableColumnFlags_WidthFixed | ImGuiTableColumnFlags_NoSort, 76.0f);
        ImGui::TableSetupColumn("Asset", ImGuiTableColumnFlags_WidthStretch | ImGuiTableColumnFlags_NoSort, 1.2f);
        ImGui::TableSetupColumn("Account", ImGuiTableColumnFlags_WidthStretch | ImGuiTableColumnFlags_NoSort, 1.0f);
        ImGui::TableSetupColumn("Status", ImGuiTableColumnFlags_WidthFixed | ImGuiTableColumnFlags_NoSort, 86.0f);
        ImGui::TableSetupColumn("Type", ImGuiTableColumnFlags_WidthFixed | ImGuiTableColumnFlags_NoSort, 96.0f);
        ImGui::TableSetupColumn("Shares", ImGuiTableColumnFlags_WidthFixed | ImGuiTableColumnFlags_NoSort, 92.0f);
        ImGui::TableSetupColumn("Avg Cost", ImGuiTableColumnFlags_WidthFixed | ImGuiTableColumnFlags_NoSort, 96.0f);
        ImGui::TableSetupColumn("Price", ImGuiTableColumnFlags_WidthFixed | ImGuiTableColumnFlags_NoSort, 96.0f);
        ImGui::TableSetupColumn("Cost Basis", ImGuiTableColumnFlags_WidthFixed | ImGuiTableColumnFlags_NoSort, 110.0f);
        ImGui::TableSetupColumn("Market Value", ImGuiTableColumnFlags_WidthFixed | ImGuiTableColumnFlags_NoSort, 116.0f);
        ImGui::TableSetupColumn("Gain/Loss", ImGuiTableColumnFlags_WidthFixed | ImGuiTableColumnFlags_NoSort, 126.0f);
        ImGui::TableSetupColumn("Target %", ImGuiTableColumnFlags_WidthFixed | ImGuiTableColumnFlags_NoSort, 86.0f);
        ImGui::TableSetupColumn("Actual %", ImGuiTableColumnFlags_WidthFixed | ImGuiTableColumnFlags_NoSort, 86.0f);
        ImGui::TableSetupColumn("Target $", ImGuiTableColumnFlags_WidthFixed | ImGuiTableColumnFlags_NoSort, 104.0f);
        ImGui::TableSetupColumn("Difference $", ImGuiTableColumnFlags_WidthFixed | ImGuiTableColumnFlags_NoSort, 112.0f);
        ImGui::TableSetupColumn("Difference %", ImGuiTableColumnFlags_WidthFixed | ImGuiTableColumnFlags_NoSort, 104.0f);
        ImGui::TableSetupColumn("Allocation Status", ImGuiTableColumnFlags_WidthFixed, 144.0f);
        ImGui::TableSetupColumn("Actions", ImGuiTableColumnFlags_WidthFixed | ImGuiTableColumnFlags_NoSort, 128.0f);
        ImGui::TableHeadersRow();

        ImGuiTableSortSpecs* sortSpecs = ImGui::TableGetSortSpecs();
        if (sortSpecs != nullptr && sortSpecs->SpecsCount > 0 && sortSpecs->Specs[0].ColumnIndex == AllocationStatusColumn) {
            sortRowsByAllocationStatus(rows);
            if (sortSpecs->SpecsDirty) {
                sortSpecs->SpecsDirty = false;
            }
        }

        for (const HoldingTableRow& row : rows) {
            const Holding& holding = *row.holding;
            const HoldingMetrics& metrics = row.metrics;
            const HoldingAllocationMetrics& allocation = row.allocation;
            const ImVec4 allocationColor = allocationStatusColor(allocation.status);

            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::Text("%s", holding.ticker.c_str());
            ImGui::TableNextColumn();
            ImGui::Text("%s", holding.assetName.c_str());
            ImGui::TableNextColumn();
            ImGui::TextColored(UiTheme::MutedText, "%s", row.accountName.c_str());
            ImGui::TableNextColumn();
            UiTheme::badge(holding.status.c_str(), holding.status == "Inactive" ? UiTheme::TextMuted : UiTheme::Gain);
            ImGui::TableNextColumn();
            ImGui::TextColored(UiTheme::MutedText, "%s", holding.assetType.c_str());
            ImGui::TableNextColumn();
            UiTheme::textRightAligned(Money::formatQuantity(holding.shares).c_str());
            ImGui::TableNextColumn();
            UiTheme::textRightAligned(Money::format(holding.averageCost).c_str());
            ImGui::TableNextColumn();
            UiTheme::textRightAligned(Money::format(holding.currentPrice).c_str(), UiTheme::ElectricCyan);
            ImGui::TableNextColumn();
            UiTheme::textRightAligned(Money::format(metrics.costBasis).c_str(), UiTheme::TextSecondary);
            ImGui::TableNextColumn();
            UiTheme::textRightAligned(Money::format(metrics.marketValue).c_str());
            ImGui::TableNextColumn();
            const std::string gainLossText = Money::format(metrics.gainLossDollar) + " / " + Money::formatPercent(metrics.gainLossPercent);
            UiTheme::textRightAligned(gainLossText.c_str(), UiTheme::moneyColor(metrics.gainLossDollar));
            ImGui::TableNextColumn();
            UiTheme::textRightAligned(Money::formatPercent(allocation.targetAllocationPercent).c_str(), UiTheme::TextSecondary);
            ImGui::TableNextColumn();
            UiTheme::textRightAligned(Money::formatPercent(allocation.actualAllocationPercent).c_str(), UiTheme::TextSecondary);
            ImGui::TableNextColumn();
            UiTheme::textRightAligned(Money::format(allocation.targetAmount).c_str(), UiTheme::TextSecondary);
            ImGui::TableNextColumn();
            UiTheme::textRightAligned(Money::format(allocation.differenceDollar).c_str(), allocationColor);
            ImGui::TableNextColumn();
            UiTheme::textRightAligned(Money::formatPercent(allocation.differencePercent, true).c_str(), allocationColor);
            ImGui::TableNextColumn();
            UiTheme::badge(PortfolioCalculator::allocationStatusLabel(allocation.status), allocationColor);
            ImGui::TableNextColumn();
            const std::string editButtonId = "Edit##edit_button_" + std::to_string(holding.id);
            UiTheme::pushButtonStyle(UiTheme::ElectricCyan);
            if (ImGui::SmallButton(editButtonId.c_str())) {
                openEdit(holding);
                openEditorPopup_ = true;
            }
            UiTheme::popButtonStyle();
            ImGui::SameLine();
            const std::string deleteButtonId = "Delete##delete_button_" + std::to_string(holding.id);
            UiTheme::pushButtonStyle(UiTheme::Loss);
            if (ImGui::SmallButton(deleteButtonId.c_str())) {
                deleteId_ = holding.id;
                deleteName_ = holding.ticker + " - " + holding.assetName;
                deletePopupId_ = holdingDeletePopupId(holding.id);
                openDeletePopup_ = true;
            }
            UiTheme::popButtonStyle();
        }

        ImGui::EndTable();
        if (rows.empty()) {
            ImGui::TextColored(UiTheme::MutedText, searchText_.empty()
                    ? "No holdings for the selected account."
                    : "No holdings match the current search.");
        }
        }
        UiTheme::popTableStyle();
    }
    ImGui::EndChild();
    UiTheme::popPanelStyle();

    if (openEditorPopup_) {
        ImGui::OpenPopup(editorPopupId_.empty() ? HoldingEditorPopup : editorPopupId_.c_str());
        openEditorPopup_ = false;
    }
    if (openDeletePopup_) {
        ImGui::OpenPopup(deletePopupId_.empty() ? DeleteHoldingPopup : deletePopupId_.c_str());
        openDeletePopup_ = false;
    }

    drawEditor(state, repository, reloadData);
    drawDeleteConfirmation(state, repository, reloadData);
}

void HoldingsView::openCreate(const AppState& state)
{
    draft_ = Holding {};
    draft_.assetType = "Stock";
    const Account* selectedAccount = ensureSelectedAccount(state, selectedAccountId_);
    draft_.accountId = selectedAccount == nullptr ? 0 : selectedAccount->id;
    draft_.status = "Active";
    editing_ = false;
    editorPopupId_ = NewHoldingEditorPopup;
    formError_.clear();
}

void HoldingsView::openEdit(const Holding& holding)
{
    draft_ = holding;
    editing_ = true;
    editorPopupId_ = holdingEditorPopupId(holding.id);
    formError_.clear();
}

void HoldingsView::drawEditor(AppState& state, HoldingRepository& repository, const std::function<void()>& reloadData)
{
    const char* popupId = editorPopupId_.empty() ? HoldingEditorPopup : editorPopupId_.c_str();
    if (!ImGui::BeginPopupModal(popupId, nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        return;
    }

    ImGui::Text("%s", editing_ ? "Edit holding" : "Add holding");
    ImGui::Separator();

    const char* accountPreview = accountNameFor(state, draft_.accountId);
    if (ImGui::BeginCombo("Account", accountPreview)) {
        for (const Account& account : state.accounts) {
            const bool selected = draft_.accountId == account.id;
            if (ImGui::Selectable(account.accountName.c_str(), selected)) {
                draft_.accountId = account.id;
            }
            if (selected) {
                ImGui::SetItemDefaultFocus();
            }
        }
        ImGui::EndCombo();
    }

    ImGui::InputText("Ticker", &draft_.ticker, ImGuiInputTextFlags_CharsUppercase | ImGuiInputTextFlags_CharsNoBlank);
    ImGui::InputText("Asset name", &draft_.assetName);
    drawStringCombo("Asset type", draft_.assetType, std::array {
        "Stock",
        "ETF",
        "Mutual Fund",
        "Bond",
        "Cash Equivalent",
        "Crypto",
        "Other",
    });
    ImGui::InputDouble("Shares", &draft_.shares, 0.0, 0.0, "%.4f");
    ImGui::InputDouble("Average cost", &draft_.averageCost, 0.0, 0.0, "%.4f");
    ImGui::InputDouble("Current price", &draft_.currentPrice, 0.0, 0.0, "%.4f");
    ImGui::InputDouble("Target allocation %", &draft_.targetAllocationPercent, 0.0, 0.0, "%.2f");
    drawStringCombo("Status", draft_.status, std::array {
        "Active",
        "Inactive",
    });
    ImGui::InputTextMultiline("Notes", &draft_.notes, ImVec2(440.0f, 86.0f));

    const HoldingMetrics metrics = PortfolioCalculator::calculateHolding(draft_);
    ImGui::Separator();
    ImGui::TextColored(UiTheme::MutedText, "Cost basis: %s", Money::format(metrics.costBasis).c_str());
    ImGui::TextColored(UiTheme::moneyColor(metrics.gainLossDollar), "Gain/Loss: %s (%s)",
        Money::format(metrics.gainLossDollar).c_str(),
        Money::formatPercent(metrics.gainLossPercent).c_str());

    UiTheme::formError(formError_);

    if (ImGui::Button(editing_ ? "Save Changes" : "Create Holding", ImVec2(140.0f, 0.0f))) {
        if (draft_.targetAllocationPercent < 0.0 || draft_.targetAllocationPercent > 100.0) {
            formError_ = "Target allocation must be between 0% and 100%.";
        } else {
            std::string error;
            const bool saved = editing_ ? repository.update(draft_, error) : repository.create(draft_, error);
            if (saved) {
                reloadData();
                state.setStatus(editing_ ? "Holding updated." : "Holding created.");
                ImGui::CloseCurrentPopup();
            } else {
                formError_ = error;
            }
        }
    }

    ImGui::SameLine();
    if (ImGui::Button("Cancel", ImVec2(100.0f, 0.0f))) {
        ImGui::CloseCurrentPopup();
    }

    ImGui::EndPopup();
}

void HoldingsView::drawDeleteConfirmation(AppState& state, HoldingRepository& repository, const std::function<void()>& reloadData)
{
    const char* popupId = deletePopupId_.empty() ? DeleteHoldingPopup : deletePopupId_.c_str();
    if (!ImGui::BeginPopupModal(popupId, nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        return;
    }

    ImGui::Text("Mark holding inactive?");
    ImGui::TextColored(UiTheme::MutedText, "%s", deleteName_.c_str());
    ImGui::TextWrapped("Inactive holdings stay in the local record but no longer count toward account balance or dashboard totals.");

    if (ImGui::Button("Mark Inactive", ImVec2(130.0f, 0.0f))) {
        std::string error;
        if (repository.remove(deleteId_, error)) {
            reloadData();
            state.setStatus("Holding marked inactive.");
            ImGui::CloseCurrentPopup();
        } else {
            state.setStatus("Could not delete holding: " + error, true);
            ImGui::CloseCurrentPopup();
        }
    }

    ImGui::SameLine();
    if (ImGui::Button("Cancel", ImVec2(100.0f, 0.0f))) {
        ImGui::CloseCurrentPopup();
    }

    ImGui::EndPopup();
}

const char* HoldingsView::accountNameFor(const AppState& state, int accountId) const
{
    for (const Account& account : state.accounts) {
        if (account.id == accountId) {
            return account.accountName.c_str();
        }
    }

    return "Unknown account";
}
