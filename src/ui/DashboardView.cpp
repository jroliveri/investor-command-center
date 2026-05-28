// SPDX-License-Identifier: MIT
#include "ui/DashboardView.hpp"

#include "app/AppState.hpp"
#include "services/PortfolioCalculator.hpp"
#include "ui/UiTheme.hpp"
#include "util/Date.hpp"
#include "util/Money.hpp"

#include <imgui.h>

#include <algorithm>
#include <map>
#include <string>
#include <vector>

namespace {

void nextCardColumn(int index, int columns)
{
    if (index % columns != columns - 1) {
        ImGui::SameLine();
    }
}

struct PlotArea {
    ImVec2 min;
    ImVec2 max;
};

void drawChartFrame(const char* title, const char* subtitle, ImVec2 size)
{
    ImGui::BeginChild(title, size, true);
    ImGui::Text("%s", title);
    ImGui::TextColored(UiTheme::MutedText, "%s", subtitle);
    ImGui::Separator();
}

PlotArea reservePlotArea(float height)
{
    const ImVec2 cursor = ImGui::GetCursorScreenPos();
    const ImVec2 size(ImGui::GetContentRegionAvail().x, height);
    ImGui::InvisibleButton("plot-area", size);
    return PlotArea { cursor, ImVec2(cursor.x + size.x, cursor.y + size.y) };
}

void drawEmptyPlot(ImDrawList* drawList, const PlotArea& area, const char* message)
{
    const ImU32 border = ImGui::GetColorU32(ImGuiCol_Border);
    const ImU32 muted = ImGui::GetColorU32(UiTheme::MutedText);
    drawList->AddRect(area.min, area.max, border, 6.0f);
    drawList->AddText(ImVec2(area.min.x + 14.0f, area.min.y + 14.0f), muted, message);
}

void drawBars(const std::vector<double>& values, const std::vector<std::string>& labels, ImVec4 color, const char* emptyMessage)
{
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    const PlotArea area = reservePlotArea(150.0f);
    const ImU32 border = ImGui::GetColorU32(ImGuiCol_Border);
    drawList->AddRect(area.min, area.max, border, 6.0f);

    if (values.empty()) {
        drawEmptyPlot(drawList, area, emptyMessage);
        return;
    }

    const double maxValue = std::max(1.0, *std::max_element(values.begin(), values.end()));
    const float gap = 9.0f;
    const float innerLeft = area.min.x + 14.0f;
    const float innerRight = area.max.x - 14.0f;
    const float innerBottom = area.max.y - 28.0f;
    const float innerTop = area.min.y + 16.0f;
    const float barWidth = std::max(8.0f, (innerRight - innerLeft - gap * static_cast<float>(values.size() - 1)) / static_cast<float>(values.size()));
    const ImU32 barColor = ImGui::GetColorU32(color);

    for (std::size_t index = 0; index < values.size(); ++index) {
        const float x = innerLeft + static_cast<float>(index) * (barWidth + gap);
        const float ratio = static_cast<float>(values[index] / maxValue);
        const float y = innerBottom - (innerBottom - innerTop) * ratio;
        drawList->AddRectFilled(ImVec2(x, y), ImVec2(x + barWidth, innerBottom), barColor, 4.0f);

        if (index < labels.size()) {
            drawList->AddText(ImVec2(x, area.max.y - 22.0f), ImGui::GetColorU32(UiTheme::MutedText), labels[index].c_str());
        }
    }
}

void drawAllocationChart(const AppState& state)
{
    std::map<std::string, double> allocation;
    for (const Holding& holding : state.holdings) {
        const HoldingMetrics metrics = PortfolioCalculator::calculateHolding(holding);
        allocation[holding.assetType.empty() ? "Other" : holding.assetType] += metrics.marketValue;
    }

    std::vector<double> values;
    std::vector<std::string> labels;
    for (const auto& [assetType, value] : allocation) {
        if (value <= 0.0) {
            continue;
        }
        labels.push_back(assetType.substr(0, 6));
        values.push_back(value);
    }

    drawBars(values, labels, UiTheme::Gain, "Add holdings with current prices to see allocation.");
}

void drawDividendsByMonth(const AppState& state)
{
    std::map<std::string, double> monthly;
    for (const Dividend& dividend : state.dividends) {
        if (dividend.dateReceived.size() >= 7) {
            monthly[dividend.dateReceived.substr(0, 7)] += dividend.amountReceived;
        }
    }

    std::vector<double> values;
    std::vector<std::string> labels;
    const int skip = std::max<int>(0, static_cast<int>(monthly.size()) - 6);
    int index = 0;
    for (const auto& [month, value] : monthly) {
        if (index++ < skip) {
            continue;
        }
        labels.push_back(month.substr(5, 2));
        values.push_back(value);
    }

    drawBars(values, labels, UiTheme::Amber, "Record dividends to see monthly income bars.");
}

void drawPortfolioValuePlaceholder(const PortfolioSummary& summary)
{
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    const PlotArea area = reservePlotArea(150.0f);
    const ImU32 border = ImGui::GetColorU32(ImGuiCol_Border);
    const ImU32 line = ImGui::GetColorU32(UiTheme::Gain);
    const ImU32 muted = ImGui::GetColorU32(UiTheme::MutedText);
    drawList->AddRect(area.min, area.max, border, 6.0f);

    const float left = area.min.x + 16.0f;
    const float right = area.max.x - 16.0f;
    const float mid = area.min.y + (area.max.y - area.min.y) * 0.52f;
    drawList->AddLine(ImVec2(left, mid + 20.0f), ImVec2(left + (right - left) * 0.35f, mid - 10.0f), line, 2.0f);
    drawList->AddLine(ImVec2(left + (right - left) * 0.35f, mid - 10.0f), ImVec2(left + (right - left) * 0.7f, mid + 4.0f), line, 2.0f);
    drawList->AddLine(ImVec2(left + (right - left) * 0.7f, mid + 4.0f), ImVec2(right, mid - 26.0f), line, 2.0f);
    drawList->AddText(ImVec2(area.min.x + 14.0f, area.min.y + 14.0f), muted, "Snapshot history placeholder");
    const std::string anchor = "Current anchor: " + Money::format(summary.accountBalance + summary.holdingsMarketValue);
    drawList->AddText(ImVec2(area.min.x + 14.0f, area.max.y - 28.0f), muted, anchor.c_str());
}

}

void DashboardView::render(const AppState& state)
{
    const PortfolioSummary summary = PortfolioCalculator::calculateSummary(state.accounts, state.holdings);
    const DividendSummary dividendSummary = PortfolioCalculator::calculateDividends(
        state.dividends,
        Date::currentMonthPrefix(),
        Date::currentYearPrefix());
    const WatchlistSummary watchlistSummary = PortfolioCalculator::calculateWatchlist(state.watchlist);

    UiTheme::sectionHeading("Dashboard", "A quiet read on your manually tracked investing picture.");

    const float availableWidth = ImGui::GetContentRegionAvail().x;
    const int columns = availableWidth > 1120.0f ? 4 : 3;
    const float gap = ImGui::GetStyle().ItemSpacing.x;
    const float cardWidth = (availableWidth - (gap * static_cast<float>(columns - 1))) / static_cast<float>(columns);
    const ImVec2 cardSize(cardWidth, 118.0f);

    UiTheme::metricCard("Account Balance", Money::format(summary.accountBalance).c_str(), "Manual account total", UiTheme::Amber, cardSize);
    nextCardColumn(0, columns);
    UiTheme::metricCard("Holdings Value", Money::format(summary.holdingsMarketValue).c_str(), "Manual prices", UiTheme::Gain, cardSize);
    nextCardColumn(1, columns);
    UiTheme::metricCard("Unrealized Gain/Loss", Money::format(summary.gainLossDollar).c_str(), Money::formatPercent(summary.gainLossPercent, true).c_str(), UiTheme::moneyColor(summary.gainLossDollar), cardSize);
    nextCardColumn(2, columns);
    UiTheme::metricCard("Dividend Income", Money::format(dividendSummary.thisYear).c_str(), (Money::format(dividendSummary.thisMonth) + " this month").c_str(), UiTheme::Amber, cardSize);
    nextCardColumn(3, columns);
    UiTheme::metricCard("Cash Balance", Money::format(summary.cashBalance).c_str(), "Available cash", UiTheme::Amber, cardSize);
    nextCardColumn(4, columns);
    UiTheme::metricCard("Tracked Activity", Money::formatNumber(static_cast<double>(state.transactions.size()), 0).c_str(), "Transactions recorded", UiTheme::MutedText, cardSize);
    nextCardColumn(5, columns);
    UiTheme::metricCard("Goals", Money::formatNumber(static_cast<double>(state.goals.size()), 0).c_str(), "Progress bars below", UiTheme::Gain, cardSize);
    nextCardColumn(6, columns);
    UiTheme::metricCard("Watchlist", Money::formatNumber(static_cast<double>(state.watchlist.size()), 0).c_str(), (std::to_string(watchlistSummary.highPriority) + " high priority").c_str(), UiTheme::Loss, cardSize);

    ImGui::Spacing();

    const float chartWidth = ImGui::GetContentRegionAvail().x;
    const float chartGap = ImGui::GetStyle().ItemSpacing.x;
    const float chartPanelWidth = (chartWidth - chartGap * 2.0f) / 3.0f;

    drawChartFrame("Portfolio Allocation", "By holding asset type", ImVec2(chartPanelWidth, 232.0f));
    drawAllocationChart(state);
    ImGui::EndChild();
    ImGui::SameLine();

    drawChartFrame("Dividends by Month", "Last recorded months", ImVec2(chartPanelWidth, 232.0f));
    drawDividendsByMonth(state);
    ImGui::EndChild();
    ImGui::SameLine();

    drawChartFrame("Portfolio Value Over Time", "Snapshot chart placeholder", ImVec2(chartPanelWidth, 232.0f));
    drawPortfolioValuePlaceholder(summary);
    ImGui::EndChild();

    ImGui::Spacing();

    const float lowerWidth = ImGui::GetContentRegionAvail().x;
    const float panelWidth = (lowerWidth - ImGui::GetStyle().ItemSpacing.x) / 2.0f;

    ImGui::BeginChild("RecentTransactions", ImVec2(panelWidth, 250.0f), true);
    ImGui::Text("Recent Transactions");
    ImGui::Separator();
    if (state.transactions.empty()) {
        UiTheme::emptyState("No transactions yet", "Add manual activity to build a useful account feed.");
    } else {
        const int limit = std::min<int>(5, static_cast<int>(state.transactions.size()));
        for (int index = 0; index < limit; ++index) {
            const Transaction& transaction = state.transactions[static_cast<std::size_t>(index)];
            ImGui::Text("%s", transaction.transactionDate.c_str());
            ImGui::SameLine(112.0f);
            UiTheme::badge(transaction.transactionType.c_str(), UiTheme::Amber);
            ImGui::SameLine(220.0f);
            ImGui::Text("%s", transaction.ticker.empty() ? transaction.assetName.c_str() : transaction.ticker.c_str());
            ImGui::SameLine();
            ImGui::TextColored(UiTheme::MutedText, "%s", Money::format(transaction.totalAmount).c_str());
        }
    }
    ImGui::EndChild();

    ImGui::SameLine();

    ImGui::BeginChild("GoalProgress", ImVec2(panelWidth, 250.0f), true);
    ImGui::Text("Goal Progress");
    ImGui::Separator();
    if (state.goals.empty()) {
        UiTheme::emptyState("No goals yet", "Create milestones to give the dashboard something durable to track.");
    } else {
        const int limit = std::min<int>(5, static_cast<int>(state.goals.size()));
        for (int index = 0; index < limit; ++index) {
            const Goal& goal = state.goals[static_cast<std::size_t>(index)];
            const GoalMetrics metrics = PortfolioCalculator::calculateGoal(goal);
            ImGui::Text("%s", goal.goalName.c_str());
            ImGui::ProgressBar(static_cast<float>(metrics.progressPercent / 100.0), ImVec2(-1.0f, 0.0f), Money::formatPercent(metrics.progressPercent).c_str());
        }
    }
    ImGui::EndChild();

    ImGui::Spacing();

    ImGui::BeginChild("DashboardNotes", ImVec2(0.0f, 0.0f), true);
    ImGui::Text("Dividend Totals");
    ImGui::Separator();
    ImGui::TextColored(UiTheme::Amber, "This month: %s", Money::format(dividendSummary.thisMonth).c_str());
    ImGui::SameLine();
    ImGui::TextColored(UiTheme::Amber, "This year: %s", Money::format(dividendSummary.thisYear).c_str());
    ImGui::SameLine();
    ImGui::TextColored(UiTheme::Amber, "Lifetime: %s", Money::format(dividendSummary.lifetime).c_str());
    ImGui::Spacing();
    ImGui::TextColored(UiTheme::MutedText, "Local SQLite storage only. No brokerage connections, no stock price APIs, no cloud sync.");
    ImGui::EndChild();
}
