// SPDX-License-Identifier: MIT
#include "ui/DashboardView.hpp"

#include "app/AppState.hpp"
#include "repositories/DashboardChartSettingsRepository.hpp"
#include "repositories/DashboardLayoutRepository.hpp"
#include "repositories/PortfolioSnapshotRepository.hpp"
#include "services/DashboardService.hpp"
#include "services/DashboardLayoutService.hpp"
#include "services/PortfolioCalculator.hpp"
#include "ui/UiTheme.hpp"
#include "ui/widgets/TerminalPanel.hpp"
#include "util/Date.hpp"
#include "util/Money.hpp"

#include <imgui.h>

#include <algorithm>
#include <cfloat>
#include <cmath>
#include <ctime>
#include <functional>
#include <map>
#include <numeric>
#include <sstream>
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

bool isActiveHolding(const Holding& holding)
{
    return holding.status.empty() || holding.status == "Active";
}

std::vector<Holding> dashboardHoldings(const AppState& state)
{
    return DashboardService::holdingsWithDashboardPrices(state);
}

struct ChartPoint {
    std::string label;
    double value = 0.0;
};

const std::vector<const char*>& timeRangeOptions()
{
    static const std::vector<const char*> options = {
        "Latest",
        "1D",
        "1W",
        "1M",
        "3M",
        "6M",
        "YTD",
        "1Y",
        "All",
    };
    return options;
}

const std::vector<const char*>& chartRangeOptions()
{
    static const std::vector<const char*> options = {
        "1M",
        "3M",
        "6M",
        "YTD",
        "1Y",
        "All",
    };
    return options;
}

bool hasOption(const std::vector<const char*>& options, const std::string& value)
{
    return std::any_of(options.begin(), options.end(), [&value](const char* option) {
        return value == option;
    });
}

DashboardChartSetting defaultChartSetting(const std::string& chartKey)
{
    if (chartKey == "performance_panel") {
        return DashboardChartSetting { 0, "performance_panel", "Portfolio Value", "3M", "Line Chart" };
    }
    if (chartKey == "income_gains_panel") {
        return DashboardChartSetting { 0, "income_gains_panel", "Dividends", "YTD", "Bar Chart" };
    }
    return DashboardChartSetting { 0, "allocation_panel", "Asset Type", "Latest", "Allocation Bars" };
}

DashboardChartSetting chartSettingFor(const AppState& state, const std::string& chartKey)
{
    DashboardChartSetting defaultSetting = defaultChartSetting(chartKey);
    for (const DashboardChartSetting& savedSetting : state.dashboardChartSettings) {
        if (savedSetting.chartKey == chartKey) {
            DashboardChartSetting normalized = savedSetting;
            const DashboardChartSetting fallback = defaultChartSetting(chartKey);
            if (chartKey == "allocation_panel") {
                if (!hasOption({ "Asset Type", "Account", "Ticker / Holding" }, normalized.dataMode)) {
                    normalized.dataMode = fallback.dataMode;
                }
                if (!hasOption(timeRangeOptions(), normalized.timeRange)) {
                    normalized.timeRange = fallback.timeRange;
                }
                if (!hasOption({ "Allocation Bars" }, normalized.chartType)) {
                    normalized.chartType = fallback.chartType;
                }
            } else if (chartKey == "performance_panel") {
                if (!hasOption({ "Portfolio Value", "Holdings Value", "Cash Balance", "Unrealized Gain/Loss" }, normalized.dataMode)) {
                    normalized.dataMode = fallback.dataMode;
                }
                if (!hasOption(chartRangeOptions(), normalized.timeRange)) {
                    normalized.timeRange = fallback.timeRange;
                }
                if (!hasOption({ "Line Chart" }, normalized.chartType)) {
                    normalized.chartType = fallback.chartType;
                }
            } else if (chartKey == "income_gains_panel") {
                if (!hasOption({ "Dividends", "Realized Gains" }, normalized.dataMode)) {
                    normalized.dataMode = fallback.dataMode;
                }
                if (!hasOption(chartRangeOptions(), normalized.timeRange)) {
                    normalized.timeRange = fallback.timeRange;
                }
                if (!hasOption({ "Bar Chart" }, normalized.chartType)) {
                    normalized.chartType = fallback.chartType;
                }
            }
            return normalized;
        }
    }

    return defaultSetting;
}

void updateStateChartSetting(AppState& state, const DashboardChartSetting& setting)
{
    for (DashboardChartSetting& existing : state.dashboardChartSettings) {
        if (existing.chartKey == setting.chartKey) {
            existing = setting;
            return;
        }
    }

    state.dashboardChartSettings.push_back(setting);
}

bool stringCombo(const char* label, std::string& value, const std::vector<const char*>& options)
{
    bool changed = false;
    const char* preview = value.empty() ? options.front() : value.c_str();
    if (ImGui::BeginCombo(label, preview)) {
        for (const char* option : options) {
            const bool selected = value == option;
            if (ImGui::Selectable(option, selected)) {
                value = option;
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

void saveChartSetting(AppState& state, DashboardChartSettingsRepository& repository, const DashboardChartSetting& setting)
{
    std::string error;
    if (repository.save(setting, error)) {
        updateStateChartSetting(state, setting);
        state.setStatus("Dashboard chart setting saved locally.");
    } else {
        state.setStatus("Could not save dashboard chart setting: " + error, true);
    }
}

void drawChartControls(
    AppState& state,
    DashboardChartSettingsRepository& repository,
    DashboardChartSetting& setting,
    const std::vector<const char*>& dataOptions,
    const std::vector<const char*>& chartTypeOptions,
    bool allowLatest)
{
    bool changed = false;
    ImGui::SetNextItemWidth(180.0f);
    changed = stringCombo("Data", setting.dataMode, dataOptions) || changed;
    ImGui::SameLine();
    ImGui::SetNextItemWidth(130.0f);
    if (allowLatest) {
        changed = stringCombo("Range", setting.timeRange, timeRangeOptions()) || changed;
    } else {
        changed = stringCombo("Range", setting.timeRange, chartRangeOptions()) || changed;
    }
    ImGui::SameLine();
    ImGui::SetNextItemWidth(160.0f);
    changed = stringCombo("Chart Type", setting.chartType, chartTypeOptions) || changed;

    if (changed) {
        saveChartSetting(state, repository, setting);
    }
}

std::string subtractDays(const std::string& today, int days)
{
    Date::DateParts parts;
    if (!Date::parseIsoDate(today, parts)) {
        return {};
    }

    std::tm time {};
    time.tm_year = parts.year - 1900;
    time.tm_mon = parts.month - 1;
    time.tm_mday = parts.day - days;
    std::mktime(&time);

    return Date::formatIsoDate(time.tm_year + 1900, time.tm_mon + 1, time.tm_mday);
}

std::string rangeStartDate(const std::string& range, const std::string& today)
{
    if (range == "All" || range == "Latest") {
        return {};
    }
    if (range == "YTD") {
        return today.substr(0, 4) + "-01-01";
    }
    if (range == "1D") {
        return subtractDays(today, 1);
    }
    if (range == "1W") {
        return subtractDays(today, 7);
    }
    if (range == "1M") {
        return subtractDays(today, 31);
    }
    if (range == "3M") {
        return subtractDays(today, 93);
    }
    if (range == "6M") {
        return subtractDays(today, 186);
    }
    if (range == "1Y") {
        return subtractDays(today, 366);
    }
    return {};
}

bool inRange(const std::string& date, const std::string& startDate)
{
    return startDate.empty() || date >= startDate;
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

void drawHorizontalAllocationBars(const std::vector<ChartPoint>& points, const char* emptyMessage)
{
    if (points.empty()) {
        ImDrawList* drawList = ImGui::GetWindowDrawList();
        const PlotArea area = reservePlotArea(168.0f);
        drawEmptyPlot(drawList, area, emptyMessage);
        return;
    }

    double total = 0.0;
    for (const ChartPoint& point : points) {
        total += point.value;
    }

    if (total <= 0.0) {
        ImDrawList* drawList = ImGui::GetWindowDrawList();
        const PlotArea area = reservePlotArea(168.0f);
        drawEmptyPlot(drawList, area, emptyMessage);
        return;
    }

    if (ImGui::BeginTable("AllocationBarsTable", 4, ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerH | ImGuiTableFlags_SizingStretchProp, ImVec2(0.0f, 170.0f))) {
        ImGui::TableSetupColumn("Segment", ImGuiTableColumnFlags_WidthStretch, 0.8f);
        ImGui::TableSetupColumn("Weight", ImGuiTableColumnFlags_WidthFixed, 74.0f);
        ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthFixed, 112.0f);
        ImGui::TableSetupColumn("Bar", ImGuiTableColumnFlags_WidthStretch, 1.2f);
        ImGui::TableHeadersRow();

        const int limit = std::min<int>(10, static_cast<int>(points.size()));
        for (int index = 0; index < limit; ++index) {
            const ChartPoint& point = points[static_cast<std::size_t>(index)];
            const double percent = total <= 0.0 ? 0.0 : point.value / total * 100.0;
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::Text("%s", point.label.c_str());
            ImGui::TableNextColumn();
            ImGui::Text("%s", Money::formatPercent(percent).c_str());
            ImGui::TableNextColumn();
            ImGui::Text("%s", Money::format(point.value).c_str());
            ImGui::TableNextColumn();
            ImGui::ProgressBar(static_cast<float>(percent / 100.0), ImVec2(-FLT_MIN, 0.0f), "");
        }

        ImGui::EndTable();
    }
}

void drawLineChart(const std::vector<ChartPoint>& points, ImVec4 color, const char* emptyMessage)
{
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    const PlotArea area = reservePlotArea(178.0f);
    const ImU32 border = ImGui::GetColorU32(ImGuiCol_Border);
    const ImU32 lineColor = ImGui::GetColorU32(color);
    const ImU32 muted = ImGui::GetColorU32(UiTheme::MutedText);
    drawList->AddRect(area.min, area.max, border, 6.0f);

    if (points.size() < 2) {
        drawList->AddText(ImVec2(area.min.x + 14.0f, area.min.y + 14.0f), muted, emptyMessage);
        return;
    }

    double minValue = points.front().value;
    double maxValue = points.front().value;
    for (const ChartPoint& point : points) {
        minValue = std::min(minValue, point.value);
        maxValue = std::max(maxValue, point.value);
    }
    if (std::fabs(maxValue - minValue) < 0.01) {
        maxValue = minValue + 1.0;
    }

    const float left = area.min.x + 16.0f;
    const float right = area.max.x - 16.0f;
    const float top = area.min.y + 22.0f;
    const float bottom = area.max.y - 34.0f;

    ImVec2 previous;
    for (std::size_t index = 0; index < points.size(); ++index) {
        const float x = left + (right - left) * static_cast<float>(index) / static_cast<float>(points.size() - 1);
        const double ratio = (points[index].value - minValue) / (maxValue - minValue);
        const float y = bottom - static_cast<float>(ratio) * (bottom - top);
        const ImVec2 current(x, y);
        drawList->AddCircleFilled(current, 3.5f, lineColor);
        if (index > 0) {
            drawList->AddLine(previous, current, lineColor, 2.0f);
        }
        previous = current;
    }

    const std::string first = points.front().label + ": " + Money::format(points.front().value);
    const std::string last = points.back().label + ": " + Money::format(points.back().value);
    drawList->AddText(ImVec2(area.min.x + 14.0f, area.min.y + 14.0f), muted, first.c_str());
    drawList->AddText(ImVec2(area.min.x + 14.0f, area.max.y - 28.0f), muted, last.c_str());
}

void drawMonthlyBarChart(const std::vector<ChartPoint>& points, ImVec4 color, const char* emptyMessage)
{
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    const PlotArea area = reservePlotArea(178.0f);
    const ImU32 border = ImGui::GetColorU32(ImGuiCol_Border);
    const ImU32 positive = ImGui::GetColorU32(color);
    const ImU32 negative = ImGui::GetColorU32(UiTheme::Loss);
    const ImU32 muted = ImGui::GetColorU32(UiTheme::MutedText);
    drawList->AddRect(area.min, area.max, border, 6.0f);

    if (points.empty()) {
        drawList->AddText(ImVec2(area.min.x + 14.0f, area.min.y + 14.0f), muted, emptyMessage);
        return;
    }

    double maxAbs = 1.0;
    for (const ChartPoint& point : points) {
        maxAbs = std::max(maxAbs, std::fabs(point.value));
    }

    const float left = area.min.x + 14.0f;
    const float right = area.max.x - 14.0f;
    const float top = area.min.y + 18.0f;
    const float bottom = area.max.y - 30.0f;
    const float baseline = (top + bottom) * 0.5f;
    const float gap = 8.0f;
    const float barWidth = std::max(8.0f, (right - left - gap * static_cast<float>(points.size() - 1)) / static_cast<float>(points.size()));

    drawList->AddLine(ImVec2(left, baseline), ImVec2(right, baseline), border, 1.0f);

    for (std::size_t index = 0; index < points.size(); ++index) {
        const ChartPoint& point = points[index];
        const float x = left + static_cast<float>(index) * (barWidth + gap);
        const float height = (bottom - top) * 0.5f * static_cast<float>(std::fabs(point.value) / maxAbs);
        const float y0 = point.value >= 0.0 ? baseline - height : baseline;
        const float y1 = point.value >= 0.0 ? baseline : baseline + height;
        drawList->AddRectFilled(ImVec2(x, y0), ImVec2(x + barWidth, y1), point.value >= 0.0 ? positive : negative, 3.0f);
        drawList->AddText(ImVec2(x, area.max.y - 22.0f), muted, point.label.c_str());
    }
}

void drawAllocationChart(const AppState& state)
{
    std::map<std::string, double> allocation;
    const std::vector<Holding> holdings = dashboardHoldings(state);
    for (const Holding& holding : holdings) {
        if (!isActiveHolding(holding)) {
            continue;
        }
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

void drawPortfolioValueHistory(const AppState& state, const PortfolioSummary& summary)
{
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    const PlotArea area = reservePlotArea(150.0f);
    const ImU32 border = ImGui::GetColorU32(ImGuiCol_Border);
    const ImU32 line = ImGui::GetColorU32(UiTheme::Gain);
    const ImU32 muted = ImGui::GetColorU32(UiTheme::MutedText);
    drawList->AddRect(area.min, area.max, border, 6.0f);

    if (state.portfolioSnapshots.empty()) {
        drawList->AddText(ImVec2(area.min.x + 14.0f, area.min.y + 14.0f), muted, "Create snapshots to see portfolio value history.");
        const std::string anchor = "Current value: " + Money::format(summary.accountBalance);
        drawList->AddText(ImVec2(area.min.x + 14.0f, area.max.y - 28.0f), muted, anchor.c_str());
        return;
    }

    std::vector<PortfolioSnapshot> snapshots = state.portfolioSnapshots;
    std::sort(snapshots.begin(), snapshots.end(), [](const PortfolioSnapshot& left, const PortfolioSnapshot& right) {
        return left.snapshotDate < right.snapshotDate;
    });
    if (snapshots.size() > 8) {
        snapshots.erase(snapshots.begin(), snapshots.end() - 8);
    }

    double minValue = snapshots.front().portfolioValue;
    double maxValue = snapshots.front().portfolioValue;
    for (const PortfolioSnapshot& snapshot : snapshots) {
        minValue = std::min(minValue, snapshot.portfolioValue);
        maxValue = std::max(maxValue, snapshot.portfolioValue);
    }
    if (maxValue == minValue) {
        maxValue = minValue + 1.0;
    }

    const float left = area.min.x + 16.0f;
    const float right = area.max.x - 16.0f;
    const float top = area.min.y + 20.0f;
    const float bottom = area.max.y - 34.0f;
    ImVec2 previous;
    for (std::size_t index = 0; index < snapshots.size(); ++index) {
        const float x = snapshots.size() == 1
            ? left
            : left + (right - left) * static_cast<float>(index) / static_cast<float>(snapshots.size() - 1);
        const double ratio = (snapshots[index].portfolioValue - minValue) / (maxValue - minValue);
        const float y = bottom - static_cast<float>(ratio) * (bottom - top);
        const ImVec2 current(x, y);
        drawList->AddCircleFilled(current, 3.5f, line);
        if (index > 0) {
            drawList->AddLine(previous, current, line, 2.0f);
        }
        previous = current;
    }

    drawList->AddText(ImVec2(area.min.x + 14.0f, area.min.y + 14.0f), muted, "Local snapshot history");
    const std::string anchor = snapshots.back().snapshotDate + ": " + Money::format(snapshots.back().portfolioValue);
    drawList->AddText(ImVec2(area.min.x + 14.0f, area.max.y - 28.0f), muted, anchor.c_str());
}

void drawUnavailableCard(const char* title, ImVec2 size)
{
    UiTheme::metricCard(title, "N/A", "Not enough snapshot data yet", UiTheme::MutedText, size);
}

struct MetricCardData {
    std::string title;
    std::string value;
    std::string caption;
    ImVec4 color;
    bool available = true;
};

void drawMetricGrid(const std::vector<MetricCardData>& cards)
{
    if (cards.empty()) {
        return;
    }

    const float availableWidth = ImGui::GetContentRegionAvail().x;
    const int columns = availableWidth > 1180.0f ? 4 : (availableWidth > 820.0f ? 3 : 2);
    const float gap = ImGui::GetStyle().ItemSpacing.x;
    const float cardWidth = (availableWidth - (gap * static_cast<float>(columns - 1))) / static_cast<float>(columns);
    const ImVec2 cardSize(cardWidth, 112.0f);

    for (std::size_t index = 0; index < cards.size(); ++index) {
        const MetricCardData& card = cards[index];
        if (!card.available) {
            drawUnavailableCard(card.title.c_str(), cardSize);
        } else {
            UiTheme::metricCard(card.title.c_str(), card.value.c_str(), card.caption.c_str(), card.color, cardSize);
        }
        nextCardColumn(static_cast<int>(index), columns);
    }

    ImGui::Spacing();
}

std::string movementCaption(bool available)
{
    return available ? "Local snapshot movement" : "Not enough snapshot data yet";
}

void drawPortfolioSummary(const DashboardData& data)
{
    TerminalPanel::begin("Portfolio Overview", ImVec2(0.0f, 250.0f));
    drawMetricGrid({
        { "Total Portfolio Value", Money::format(data.portfolio.accountBalance), "Holdings plus cash", UiTheme::Gain },
        { "Total Holdings Value", Money::format(data.portfolio.holdingsMarketValue), "Shares x current price", UiTheme::Gain },
        { "Total Cash Balance", Money::format(data.portfolio.cashBalance), "Account cash", UiTheme::Amber },
        { "Total Cost Basis", Money::format(data.portfolio.costBasis), "Shares x average cost", UiTheme::MutedText },
        { "Unrealized Gain/Loss", Money::format(data.portfolio.gainLossDollar), Money::formatPercent(data.portfolio.gainLossPercent, true), UiTheme::moneyColor(data.portfolio.gainLossDollar) },
    });
    TerminalPanel::end();
}

void drawPriceRefreshStatus(const AppState& state, const std::function<void()>& refreshCurrentPrices)
{
    TerminalPanel::begin("Current Price Refresh", ImVec2(0.0f, 150.0f));
    if (ImGui::Button("Refresh Current Prices", ImVec2(180.0f, 0.0f))) {
        refreshCurrentPrices();
    }
    ImGui::SameLine();
    ImGui::BeginDisabled();
    ImGui::Button("Create Snapshot From Refreshed Prices", ImVec2(260.0f, 0.0f));
    ImGui::EndDisabled();
    ImGui::SameLine();
    ImGui::TextColored(UiTheme::MutedText, "Manual snapshot option planned.");

    ImGui::Spacing();
    const DashboardPriceRefreshStatus& status = state.dashboardPriceRefreshStatus;
    ImGui::Text("Provider: %s", status.provider.empty() ? "Yahoo Finance" : status.provider.c_str());
    ImGui::SameLine(230.0f);
    ImGui::Text("Last refresh: %s", status.hasRun && !status.lastRefreshedAt.empty() ? status.lastRefreshedAt.c_str() : "Not refreshed this session");
    ImGui::Text("Price source: %s", status.hasRun ? "Live Quote / Cached Quote overlay" : "CSV Import / Manual");
    ImGui::SameLine(230.0f);
    ImGui::Text("Refreshed: %d   Failed: %d   Cached: %d", status.refreshedSymbols, status.failedSymbols, status.cachedSymbols);
    if (!status.warning.empty()) {
        ImGui::TextColored(status.failedSymbols > 0 ? UiTheme::Amber : UiTheme::MutedText, "%s", status.warning.c_str());
    } else {
        ImGui::TextColored(UiTheme::MutedText, "Current prices may be delayed or unavailable. CSV import remains the primary portfolio workflow.");
    }
    TerminalPanel::end();
}

void drawSingleMetricSection(const MetricCardData& card)
{
    TerminalPanel::begin(card.title.c_str(), ImVec2(0.0f, 150.0f));
    drawMetricGrid({ card });
    TerminalPanel::end();
}

void drawPerformanceMovement(const DashboardData& data, const AppState& state)
{
    TerminalPanel::begin("Daily / Monthly / Yearly Movement", ImVec2(0.0f, 230.0f));
    drawMetricGrid({
        { "Daily Gain/Loss", data.performance.hasDaily ? Money::format(data.performance.daily) : "N/A", movementCaption(data.performance.hasDaily), UiTheme::moneyColor(data.performance.daily), data.performance.hasDaily },
        { "Monthly Gain/Loss", data.performance.hasMonthly ? Money::format(data.performance.monthly) : "N/A", movementCaption(data.performance.hasMonthly), UiTheme::moneyColor(data.performance.monthly), data.performance.hasMonthly },
        { "Yearly Gain/Loss", data.performance.hasYearly ? Money::format(data.performance.yearly) : "N/A", movementCaption(data.performance.hasYearly), UiTheme::moneyColor(data.performance.yearly), data.performance.hasYearly },
    });

    ImGui::TextColored(UiTheme::MutedText, state.portfolioSnapshots.empty()
        ? "Import a CSV to create your first snapshot. Manual snapshots are available as an optional advanced tool."
        : "Performance is based on local snapshots created by CSV imports or manual snapshots and may include deposits or withdrawals. Contribution-adjusted returns are planned for a future update.");
    TerminalPanel::end();
}

void drawRealizedGainsPanel(const DashboardData& data)
{
    TerminalPanel::begin("Realized Gains", ImVec2(0.0f, 190.0f));
    ImGui::TextColored(UiTheme::moneyColor(data.realizedGains.today), "Today: %s", Money::format(data.realizedGains.today).c_str());
    ImGui::TextColored(UiTheme::moneyColor(data.realizedGains.thisMonth), "This month: %s", Money::format(data.realizedGains.thisMonth).c_str());
    ImGui::TextColored(UiTheme::moneyColor(data.realizedGains.thisYear), "This year: %s", Money::format(data.realizedGains.thisYear).c_str());
    ImGui::TextColored(UiTheme::MutedText, "Sell transactions: %d", data.realizedGains.sellCount);
    ImGui::Spacing();
    ImGui::TextWrapped("Realized gain/loss is separate from unrealized holding movement and is calculated from sell transactions using average cost basis.");
    TerminalPanel::end();
}

void drawDividendSummaryPanel(const DashboardData& data)
{
    TerminalPanel::begin("Dividend Summary", ImVec2(0.0f, 190.0f));
    drawMetricGrid({
        { "Dividends This Month", Money::format(data.dividends.thisMonth), "Recorded dividends", UiTheme::Amber },
        { "Dividends This Year", Money::format(data.dividends.thisYear), "Recorded dividends", UiTheme::Amber },
        { "Lifetime Dividends", Money::format(data.dividends.lifetime), "Recorded dividends", UiTheme::Amber },
    });
    TerminalPanel::end();
}

void drawRecentTransactionsPanel(const AppState& state)
{
    TerminalPanel::begin("Recent Activity", ImVec2(0.0f, 235.0f));
    if (state.transactions.empty()) {
        UiTheme::emptyState("No transactions yet", "Add buys, sells, dividends, and cash activity to build the ledger.");
    } else {
        const int limit = std::min<int>(8, static_cast<int>(state.transactions.size()));
        for (int index = 0; index < limit; ++index) {
            const Transaction& transaction = state.transactions[static_cast<std::size_t>(index)];
            ImGui::Text("%s", transaction.transactionDate.c_str());
            ImGui::SameLine(105.0f);
            UiTheme::badge(transaction.transactionType.c_str(), transaction.transactionType == "Sell" ? UiTheme::Loss : UiTheme::Amber);
            ImGui::SameLine(190.0f);
            ImGui::Text("%s", transaction.ticker.empty() ? transaction.assetName.c_str() : transaction.ticker.c_str());
            ImGui::SameLine();
            ImGui::TextColored(UiTheme::MutedText, "%s", Money::format(transaction.totalAmount).c_str());
        }
    }
    TerminalPanel::end();
}

void drawHoldingsTablePanel(const AppState& state)
{
    TerminalPanel::begin("Holdings", ImVec2(0.0f, 290.0f));
    const std::vector<Holding> holdings = dashboardHoldings(state);
    if (holdings.empty()) {
        UiTheme::emptyState("No holdings yet", "Import a positions CSV or add holdings manually.");
        TerminalPanel::end();
        return;
    }

    if (ImGui::BeginTable("DashboardHoldingsTable", 8, ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerH | ImGuiTableFlags_Resizable | ImGuiTableFlags_ScrollY, ImVec2(0.0f, 225.0f))) {
        ImGui::TableSetupColumn("Ticker", ImGuiTableColumnFlags_WidthFixed, 70.0f);
        ImGui::TableSetupColumn("Asset");
        ImGui::TableSetupColumn("Type", ImGuiTableColumnFlags_WidthFixed, 76.0f);
        ImGui::TableSetupColumn("Shares", ImGuiTableColumnFlags_WidthFixed, 82.0f);
        ImGui::TableSetupColumn("Price", ImGuiTableColumnFlags_WidthFixed, 86.0f);
        ImGui::TableSetupColumn("Source", ImGuiTableColumnFlags_WidthFixed, 92.0f);
        ImGui::TableSetupColumn("Market Value", ImGuiTableColumnFlags_WidthFixed, 112.0f);
        ImGui::TableSetupColumn("Unrealized", ImGuiTableColumnFlags_WidthFixed, 104.0f);
        ImGui::TableHeadersRow();

        int rendered = 0;
        for (const Holding& holding : holdings) {
            if (!isActiveHolding(holding)) {
                continue;
            }
            const HoldingMetrics metrics = PortfolioCalculator::calculateHolding(holding);
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::Text("%s", holding.ticker.c_str());
            ImGui::TableNextColumn();
            ImGui::Text("%s", holding.assetName.c_str());
            ImGui::TableNextColumn();
            ImGui::TextColored(UiTheme::MutedText, "%s", holding.assetType.c_str());
            ImGui::TableNextColumn();
            ImGui::Text("%s", Money::formatQuantity(holding.shares).c_str());
            ImGui::TableNextColumn();
            ImGui::Text("%s", Money::format(holding.currentPrice).c_str());
            ImGui::TableNextColumn();
            const std::string priceSource = DashboardService::priceSourceForHolding(state, holding);
            ImGui::TextColored(priceSource == "Live Quote" ? UiTheme::Gain : (priceSource == "Cached Quote" ? UiTheme::Amber : UiTheme::MutedText), "%s", priceSource.c_str());
            ImGui::TableNextColumn();
            ImGui::Text("%s", Money::format(metrics.marketValue).c_str());
            ImGui::TableNextColumn();
            ImGui::TextColored(UiTheme::moneyColor(metrics.gainLossDollar), "%s", Money::format(metrics.gainLossDollar).c_str());
            ++rendered;
            if (rendered >= 18) {
                break;
            }
        }
        ImGui::EndTable();
    }
    TerminalPanel::end();
}

void drawSnapshotStatusPanel(
    AppState& state,
    PortfolioSnapshotRepository& snapshotRepository,
    const DashboardData& data,
    const std::string& today,
    const std::function<void()>& reloadData,
    const std::function<void(bool)>& createSnapshot)
{
    (void)snapshotRepository;
    (void)data;

    TerminalPanel::begin("Import Status / Snapshot History", ImVec2(0.0f, 230.0f));
    const PortfolioSnapshot* latest = DashboardService::latestSnapshot(state);
    const ImportBatch* latestImport = DashboardService::latestImportBatch(state);
    ImGui::Text("Last CSV import: %s", latestImport == nullptr ? "None" : latestImport->importDate.c_str());
    ImGui::Text("Last snapshot: %s", latest == nullptr ? "None" : latest->snapshotDate.c_str());
    ImGui::Text("Snapshot source: %s", latest == nullptr ? "None" : latest->source.c_str());
    ImGui::Text("Snapshots: %d", static_cast<int>(state.portfolioSnapshots.size()));
    ImGui::Spacing();
    if (ImGui::Button("Create Manual Snapshot")) {
        if (DashboardService::hasSnapshotForDate(state, today)) {
            ImGui::OpenPopup("Replace Today Snapshot");
        } else {
            createSnapshot(false);
        }
    }
    ImGui::TextColored(UiTheme::MutedText, "CSV imports create snapshots automatically.");
    TerminalPanel::end();

    if (ImGui::BeginPopupModal("Replace Today Snapshot", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("A manual snapshot already exists for today.");
        ImGui::TextColored(UiTheme::MutedText, "Replace it with the current dashboard totals?");
        if (ImGui::Button("Replace Snapshot", ImVec2(150.0f, 0.0f))) {
            createSnapshot(true);
            ImGui::CloseCurrentPopup();
            reloadData();
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(100.0f, 0.0f))) {
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
}

std::vector<ChartPoint> allocationPoints(const AppState& state, const std::string& dataMode)
{
    std::map<std::string, double> grouped;
    const std::vector<Holding> holdings = dashboardHoldings(state);

    if (dataMode == "Account") {
        for (const Account& account : state.accounts) {
            const AccountMetrics metrics = PortfolioCalculator::calculateAccount(account, holdings);
            if (metrics.calculatedBalance > 0.0) {
                grouped[account.accountName] += metrics.calculatedBalance;
            }
        }
    } else if (dataMode == "Ticker / Holding") {
        for (const Holding& holding : holdings) {
            if (!isActiveHolding(holding)) {
                continue;
            }
            const HoldingMetrics metrics = PortfolioCalculator::calculateHolding(holding);
            if (metrics.marketValue > 0.0) {
                grouped[holding.ticker.empty() ? holding.assetName : holding.ticker] += metrics.marketValue;
            }
        }
    } else {
        for (const Holding& holding : holdings) {
            if (!isActiveHolding(holding)) {
                continue;
            }
            const HoldingMetrics metrics = PortfolioCalculator::calculateHolding(holding);
            if (metrics.marketValue > 0.0) {
                grouped[holding.assetType.empty() ? "Other" : holding.assetType] += metrics.marketValue;
            }
        }
    }

    std::vector<ChartPoint> points;
    for (const auto& [label, value] : grouped) {
        points.push_back({ label, value });
    }

    std::sort(points.begin(), points.end(), [](const ChartPoint& left, const ChartPoint& right) {
        return left.value > right.value;
    });
    return points;
}

double snapshotValueForMode(const PortfolioSnapshot& snapshot, const std::string& dataMode)
{
    if (dataMode == "Holdings Value") {
        return snapshot.holdingsValue;
    }
    if (dataMode == "Cash Balance") {
        return snapshot.cashBalance;
    }
    if (dataMode == "Unrealized Gain/Loss") {
        return snapshot.unrealizedGain;
    }
    return snapshot.portfolioValue;
}

std::vector<ChartPoint> performancePoints(const AppState& state, const std::string& dataMode, const std::string& timeRange)
{
    const std::string start = rangeStartDate(timeRange, Date::todayIso8601());
    std::vector<PortfolioSnapshot> snapshots = state.portfolioSnapshots;
    snapshots.erase(
        std::remove_if(snapshots.begin(), snapshots.end(), [&start](const PortfolioSnapshot& snapshot) {
            return !inRange(snapshot.snapshotDate, start);
        }),
        snapshots.end());
    std::sort(snapshots.begin(), snapshots.end(), [](const PortfolioSnapshot& left, const PortfolioSnapshot& right) {
        return left.snapshotDate < right.snapshotDate;
    });

    std::vector<ChartPoint> points;
    for (const PortfolioSnapshot& snapshot : snapshots) {
        points.push_back({ snapshot.snapshotDate, snapshotValueForMode(snapshot, dataMode) });
    }
    return points;
}

std::vector<ChartPoint> incomeGainPoints(const AppState& state, const std::string& dataMode, const std::string& timeRange)
{
    const std::string start = rangeStartDate(timeRange, Date::todayIso8601());
    std::map<std::string, double> monthly;

    if (dataMode == "Realized Gains") {
        for (const Transaction& transaction : state.transactions) {
            if (transaction.transactionType != "Sell" || transaction.transactionDate.size() < 7 || !inRange(transaction.transactionDate, start)) {
                continue;
            }
            monthly[transaction.transactionDate.substr(0, 7)] += transaction.realizedGainDollar;
        }
    } else {
        for (const Dividend& dividend : state.dividends) {
            if (dividend.dateReceived.size() < 7 || !inRange(dividend.dateReceived, start)) {
                continue;
            }
            monthly[dividend.dateReceived.substr(0, 7)] += dividend.amountReceived;
        }
    }

    std::vector<ChartPoint> points;
    for (const auto& [month, value] : monthly) {
        points.push_back({ month.substr(5, 2), value });
    }
    return points;
}

void drawAllocationPanel(AppState& state, DashboardChartSettingsRepository& chartSettingsRepository)
{
    TerminalPanel::begin("Allocation", ImVec2(0.0f, 300.0f));
    DashboardChartSetting setting = chartSettingFor(state, "allocation_panel");
    drawChartControls(state,
        chartSettingsRepository,
        setting,
        { "Asset Type", "Account", "Ticker / Holding" },
        { "Allocation Bars" },
        true);

    if (setting.timeRange != "Latest") {
        ImGui::Spacing();
        ImGui::TextColored(UiTheme::Amber, "Not enough snapshot data for this range.");
        ImGui::TextColored(UiTheme::MutedText, "Allocation history by asset, account, or ticker is not stored yet. Use Latest for current allocation.");
    } else {
        ImGui::TextColored(UiTheme::MutedText, "%s", setting.dataMode == "Asset Type"
                ? "Asset allocation by asset type"
                : (setting.dataMode == "Account" ? "Account allocation by calculated account value" : "Holdings allocation by ticker"));
        drawHorizontalAllocationBars(allocationPoints(state, setting.dataMode), "Add active holdings with current prices to see allocation.");
    }

    TerminalPanel::end();
}

void drawPerformanceChartPanel(AppState& state, DashboardChartSettingsRepository& chartSettingsRepository)
{
    TerminalPanel::begin("Performance", ImVec2(0.0f, 300.0f));
    DashboardChartSetting setting = chartSettingFor(state, "performance_panel");
    drawChartControls(state,
        chartSettingsRepository,
        setting,
        { "Portfolio Value", "Holdings Value", "Cash Balance", "Unrealized Gain/Loss" },
        { "Line Chart" },
        false);

    const std::vector<ChartPoint> points = performancePoints(state, setting.dataMode, setting.timeRange);
    drawLineChart(points, UiTheme::Gain, "Not enough snapshot data for this range.");
    ImGui::TextColored(UiTheme::MutedText, "Performance charts use local portfolio snapshots.");
    TerminalPanel::end();
}

void drawIncomeGainsChartPanel(AppState& state, DashboardChartSettingsRepository& chartSettingsRepository)
{
    TerminalPanel::begin("Income / Gains", ImVec2(0.0f, 300.0f));
    DashboardChartSetting setting = chartSettingFor(state, "income_gains_panel");
    drawChartControls(state,
        chartSettingsRepository,
        setting,
        { "Dividends", "Realized Gains" },
        { "Bar Chart" },
        false);

    const std::vector<ChartPoint> points = incomeGainPoints(state, setting.dataMode, setting.timeRange);
    drawMonthlyBarChart(points, setting.dataMode == "Dividends" ? UiTheme::Amber : UiTheme::Gain, "Not enough local records for this range.");
    ImGui::TextColored(UiTheme::MutedText, "Bars group local records by month.");
    TerminalPanel::end();
}

void drawAssetAllocationPanel(const AppState& state)
{
    TerminalPanel::begin("Asset Allocation", ImVec2(0.0f, 250.0f));
    ImGui::TextColored(UiTheme::MutedText, "By holding asset type");
    drawAllocationChart(state);
    TerminalPanel::end();
}

void drawAccountAllocationPanel(const AppState& state)
{
    TerminalPanel::begin("Account Allocation", ImVec2(0.0f, 250.0f));
    std::vector<double> values;
    std::vector<std::string> labels;
    const std::vector<Holding> holdings = dashboardHoldings(state);
    for (const Account& account : state.accounts) {
        const AccountMetrics metrics = PortfolioCalculator::calculateAccount(account, holdings);
        if (metrics.calculatedBalance <= 0.0) {
            continue;
        }
        labels.push_back(account.accountName.substr(0, 8));
        values.push_back(metrics.calculatedBalance);
    }

    ImGui::TextColored(UiTheme::MutedText, "Calculated account balances");
    drawBars(values, labels, UiTheme::Amber, "Add accounts and holdings to see account allocation.");
    TerminalPanel::end();
}

void drawDividendsByMonthPanel(const AppState& state)
{
    TerminalPanel::begin("Dividends by Month", ImVec2(0.0f, 250.0f));
    ImGui::TextColored(UiTheme::MutedText, "Last recorded months");
    drawDividendsByMonth(state);
    TerminalPanel::end();
}

void drawPortfolioValueHistoryPanel(const AppState& state, const PortfolioSummary& summary)
{
    TerminalPanel::begin("Portfolio Value Over Time", ImVec2(0.0f, 250.0f));
    ImGui::TextColored(UiTheme::MutedText, "Local portfolio snapshots");
    drawPortfolioValueHistory(state, summary);
    TerminalPanel::end();
}

void drawTopGainersLosersPanel(const AppState& state)
{
    TerminalPanel::begin("Top Gainers / Top Losers", ImVec2(0.0f, 280.0f));
    struct HoldingRow {
        std::string ticker;
        std::string name;
        HoldingMetrics metrics;
    };

    std::vector<HoldingRow> rows;
    const std::vector<Holding> holdings = dashboardHoldings(state);
    for (const Holding& holding : holdings) {
        if (!isActiveHolding(holding)) {
            continue;
        }
        rows.push_back({ holding.ticker, holding.assetName, PortfolioCalculator::calculateHolding(holding) });
    }

    std::sort(rows.begin(), rows.end(), [](const HoldingRow& left, const HoldingRow& right) {
        return left.metrics.gainLossDollar > right.metrics.gainLossDollar;
    });

    if (rows.empty()) {
        UiTheme::emptyState("No holdings yet", "Import or add holdings to review gain/loss movement.");
        TerminalPanel::end();
        return;
    }

    if (ImGui::BeginTable("TopGainersLosersTable", 5, ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerH | ImGuiTableFlags_Resizable)) {
        ImGui::TableSetupColumn("Ticker", ImGuiTableColumnFlags_WidthFixed, 90.0f);
        ImGui::TableSetupColumn("Asset");
        ImGui::TableSetupColumn("Market Value", ImGuiTableColumnFlags_WidthFixed, 130.0f);
        ImGui::TableSetupColumn("Gain/Loss", ImGuiTableColumnFlags_WidthFixed, 130.0f);
        ImGui::TableSetupColumn("Gain/Loss %", ImGuiTableColumnFlags_WidthFixed, 120.0f);
        ImGui::TableHeadersRow();

        const int limit = std::min<int>(6, static_cast<int>(rows.size()));
        for (int index = 0; index < limit; ++index) {
            const HoldingRow& row = rows[static_cast<std::size_t>(index)];
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::Text("%s", row.ticker.c_str());
            ImGui::TableSetColumnIndex(1);
            ImGui::Text("%s", row.name.c_str());
            ImGui::TableSetColumnIndex(2);
            ImGui::Text("%s", Money::format(row.metrics.marketValue).c_str());
            ImGui::TableSetColumnIndex(3);
            ImGui::TextColored(UiTheme::moneyColor(row.metrics.gainLossDollar), "%s", Money::format(row.metrics.gainLossDollar).c_str());
            ImGui::TableSetColumnIndex(4);
            ImGui::TextColored(UiTheme::moneyColor(row.metrics.gainLossDollar), "%s", Money::formatPercent(row.metrics.gainLossPercent, true).c_str());
        }
        ImGui::EndTable();
    }
    TerminalPanel::end();
}

void drawDashboardWidget(
    const DashboardWidget& widget,
    AppState& state,
    PortfolioSnapshotRepository& snapshotRepository,
    DashboardChartSettingsRepository& chartSettingsRepository,
    const DashboardData& data,
    const std::string& today,
    const std::function<void()>& reloadData,
    const std::function<void(bool)>& createSnapshot)
{
    if (widget.widgetKey == "portfolio_summary") {
        drawPortfolioSummary(data);
    } else if (widget.widgetKey == "holdings_value") {
        drawSingleMetricSection({ "Holdings Value", Money::format(data.portfolio.holdingsMarketValue), "Shares x current price", UiTheme::Gain });
    } else if (widget.widgetKey == "cash_balance") {
        drawSingleMetricSection({ "Cash Balance", Money::format(data.portfolio.cashBalance), "Account cash", UiTheme::Amber });
    } else if (widget.widgetKey == "cost_basis") {
        drawSingleMetricSection({ "Cost Basis", Money::format(data.portfolio.costBasis), "Shares x average cost", UiTheme::MutedText });
    } else if (widget.widgetKey == "unrealized_gain_loss") {
        drawSingleMetricSection({ "Unrealized Gain/Loss", Money::format(data.portfolio.gainLossDollar), Money::formatPercent(data.portfolio.gainLossPercent, true), UiTheme::moneyColor(data.portfolio.gainLossDollar) });
    } else if (widget.widgetKey == "performance_movement") {
        drawPerformanceMovement(data, state);
    } else if (widget.widgetKey == "holdings_table") {
        drawHoldingsTablePanel(state);
    } else if (widget.widgetKey == "realized_gains") {
        drawRealizedGainsPanel(data);
    } else if (widget.widgetKey == "dividend_summary") {
        drawDividendSummaryPanel(data);
    } else if (widget.widgetKey == "recent_transactions") {
        drawRecentTransactionsPanel(state);
    } else if (widget.widgetKey == "snapshot_status") {
        drawSnapshotStatusPanel(state, snapshotRepository, data, today, reloadData, createSnapshot);
    } else if (widget.widgetKey == "allocation_panel") {
        drawAllocationPanel(state, chartSettingsRepository);
    } else if (widget.widgetKey == "performance_panel") {
        drawPerformanceChartPanel(state, chartSettingsRepository);
    } else if (widget.widgetKey == "income_gains_panel") {
        drawIncomeGainsChartPanel(state, chartSettingsRepository);
    } else if (widget.widgetKey == "top_gainers_losers") {
        drawTopGainersLosersPanel(state);
    }
}

}

void DashboardView::render(
    AppState& state,
    PortfolioSnapshotRepository& snapshotRepository,
    DashboardLayoutRepository& layoutRepository,
    DashboardChartSettingsRepository& chartSettingsRepository,
    const std::function<void()>& refreshCurrentPrices,
    const std::function<void()>& reloadData)
{
    const std::string today = Date::todayIso8601();
    const DashboardData data = DashboardService::build(state, today, Date::currentMonthPrefix(), Date::currentYearPrefix());

    UiTheme::sectionHeading("Dashboard", "Terminal-style portfolio review. CSV imports update holdings and create local snapshots.");

    if (ImGui::Button(customizeMode_ ? "Done Customizing" : "Customize Dashboard")) {
        customizeMode_ = !customizeMode_;
    }
    ImGui::SameLine();
    if (ImGui::Button("Refresh Current Prices")) {
        refreshCurrentPrices();
    }
    ImGui::Spacing();
    drawPriceRefreshStatus(state, refreshCurrentPrices);
    ImGui::Spacing();

    if (customizeMode_) {
        renderCustomizePanel(state, layoutRepository, reloadData);
        ImGui::Spacing();
    }

    const std::vector<DashboardWidget> visibleWidgets = DashboardLayoutService::visibleWidgets(state.dashboardWidgets);
    if (visibleWidgets.empty()) {
        UiTheme::emptyState("No dashboard sections are visible", "Open Customize Dashboard and re-enable the sections you want to review.");
        return;
    }

    const auto createSnapshot = [this, &state, &snapshotRepository, &reloadData](bool replaceExisting) {
        createTodaySnapshot(state, snapshotRepository, reloadData, replaceExisting);
    };

    const int columns = ImGui::GetContentRegionAvail().x > 1220.0f ? 2 : 1;
    if (ImGui::BeginTable("DashboardTerminalWorkspace", columns, ImGuiTableFlags_SizingStretchSame)) {
        for (const DashboardWidget& widget : visibleWidgets) {
            ImGui::TableNextColumn();
            drawDashboardWidget(widget, state, snapshotRepository, chartSettingsRepository, data, today, reloadData, createSnapshot);
        }
        ImGui::EndTable();
    }
}

void DashboardView::renderCustomizePanel(AppState& state, DashboardLayoutRepository& layoutRepository, const std::function<void()>& reloadData)
{
    ImGui::BeginChild("DashboardCustomizePanel", ImVec2(0.0f, 440.0f), true);
    ImGui::Text("Customize Dashboard");
    ImGui::TextWrapped("Rearrange your dashboard so it matches the way you review your portfolio.");
    ImGui::TextColored(UiTheme::MutedText, "Dashboard layout is stored locally.");
    ImGui::Spacing();

    if (ImGui::Button("Reset Dashboard Layout")) {
        std::string error;
        if (layoutRepository.resetToDefaults(error)) {
            reloadData();
            state.setStatus("Dashboard layout reset.");
        } else {
            state.setStatus("Could not reset dashboard layout: " + error, true);
        }
    }

    ImGui::Spacing();
    std::vector<DashboardWidget> widgets = DashboardLayoutService::orderedWidgets(state.dashboardWidgets);
    if (ImGui::BeginTable("DashboardCustomizeTable", 4, ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerH | ImGuiTableFlags_SizingStretchProp)) {
        ImGui::TableSetupColumn("Section");
        ImGui::TableSetupColumn("Visible", ImGuiTableColumnFlags_WidthFixed, 90.0f);
        ImGui::TableSetupColumn("Move Up", ImGuiTableColumnFlags_WidthFixed, 105.0f);
        ImGui::TableSetupColumn("Move Down", ImGuiTableColumnFlags_WidthFixed, 115.0f);
        ImGui::TableHeadersRow();

        for (std::size_t index = 0; index < widgets.size(); ++index) {
            DashboardWidget& widget = widgets[index];
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::Text("%s", widget.displayName.c_str());

            ImGui::TableSetColumnIndex(1);
            bool visible = widget.isVisible;
            const std::string checkboxId = "##visible_" + widget.widgetKey;
            if (ImGui::Checkbox(checkboxId.c_str(), &visible)) {
                widget.isVisible = visible;
                std::string error;
                if (layoutRepository.saveAll(widgets, error)) {
                    reloadData();
                    state.setStatus("Dashboard layout saved.");
                } else {
                    state.setStatus("Could not save dashboard layout: " + error, true);
                }
            }

            ImGui::TableSetColumnIndex(2);
            ImGui::BeginDisabled(index == 0);
            const std::string upId = "Up##" + widget.widgetKey;
            if (ImGui::Button(upId.c_str(), ImVec2(72.0f, 0.0f))) {
                std::vector<DashboardWidget> updated = widgets;
                std::string error;
                if (DashboardLayoutService::moveWidget(updated, widget.widgetKey, -1) && layoutRepository.saveAll(updated, error)) {
                    reloadData();
                    state.setStatus("Dashboard layout saved.");
                } else if (!error.empty()) {
                    state.setStatus("Could not save dashboard layout: " + error, true);
                }
            }
            ImGui::EndDisabled();

            ImGui::TableSetColumnIndex(3);
            ImGui::BeginDisabled(index + 1 >= widgets.size());
            const std::string downId = "Down##" + widget.widgetKey;
            if (ImGui::Button(downId.c_str(), ImVec2(82.0f, 0.0f))) {
                std::vector<DashboardWidget> updated = widgets;
                std::string error;
                if (DashboardLayoutService::moveWidget(updated, widget.widgetKey, 1) && layoutRepository.saveAll(updated, error)) {
                    reloadData();
                    state.setStatus("Dashboard layout saved.");
                } else if (!error.empty()) {
                    state.setStatus("Could not save dashboard layout: " + error, true);
                }
            }
            ImGui::EndDisabled();
        }
        ImGui::EndTable();
    }

    ImGui::EndChild();
}

void DashboardView::createTodaySnapshot(AppState& state, PortfolioSnapshotRepository& snapshotRepository, const std::function<void()>& reloadData, bool replaceExisting)
{
    const std::string today = Date::todayIso8601();
    if (!replaceExisting && DashboardService::hasSnapshotForDate(state, today)) {
        state.setStatus("A snapshot already exists for today.", true);
        return;
    }

    const DashboardData data = DashboardService::build(state, today, Date::currentMonthPrefix(), Date::currentYearPrefix());
    PortfolioSnapshot snapshot = DashboardService::buildSnapshot(data, today);

    std::string error;
    if (!snapshotRepository.upsertForDate(snapshot, error)) {
        state.setStatus("Could not save snapshot: " + error, true);
        return;
    }

    reloadData();
    state.setStatus("Today's portfolio snapshot saved.");
}
