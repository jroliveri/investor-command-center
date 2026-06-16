// SPDX-License-Identifier: MIT
#include "ui/DashboardView.hpp"

#include "app/AppState.hpp"
#include "repositories/AppSettingsRepository.hpp"
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
#include <imgui_stdlib.h>

#include <algorithm>
#include <array>
#include <cfloat>
#include <cctype>
#include <cmath>
#include <ctime>
#include <functional>
#include <iomanip>
#include <map>
#include <numeric>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

void nextCardColumn(int index, int columns)
{
    if (index % columns != columns - 1) {
        ImGui::SameLine();
    }
}

std::string uppercaseCopy(std::string value)
{
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char character) {
        return static_cast<char>(std::toupper(character));
    });
    return value;
}

std::string lowerCopy(std::string value)
{
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char character) {
        return static_cast<char>(std::tolower(character));
    });
    return value;
}

std::string trimCopy(const std::string& value)
{
    const auto first = std::find_if_not(value.begin(), value.end(), [](unsigned char character) {
        return std::isspace(character) != 0;
    });
    if (first == value.end()) {
        return {};
    }

    const auto last = std::find_if_not(value.rbegin(), value.rend(), [](unsigned char character) {
        return std::isspace(character) != 0;
    }).base();
    return std::string(first, last);
}

std::optional<double> parseNonNegativeCurrencyDraft(const std::string& raw)
{
    std::string value = trimCopy(raw);
    if (value.empty()) {
        return 0.0;
    }

    value.erase(std::remove(value.begin(), value.end(), ','), value.end());
    if (!value.empty() && value.front() == '$') {
        value.erase(value.begin());
        value = trimCopy(value);
    }

    try {
        std::size_t consumed = 0;
        const double parsed = std::stod(value, &consumed);
        if (consumed != value.size() || !std::isfinite(parsed) || parsed < 0.0) {
            return std::nullopt;
        }
        return parsed;
    } catch (const std::invalid_argument&) {
        return std::nullopt;
    } catch (const std::out_of_range&) {
        return std::nullopt;
    }
}

std::string formatCurrencyDraft(double value)
{
    std::ostringstream stream;
    stream << std::fixed << std::setprecision(2) << std::max(0.0, value);
    return stream.str();
}

std::tm localTimeForDate(const Date::DateParts& parts)
{
    std::tm time {};
    time.tm_year = parts.year - 1900;
    time.tm_mon = parts.month - 1;
    time.tm_mday = parts.day;
    time.tm_isdst = -1;
    std::mktime(&time);
    return time;
}

std::string offsetDate(const Date::DateParts& parts, int dayOffset)
{
    std::tm time = localTimeForDate(parts);
    time.tm_mday += dayOffset;
    std::mktime(&time);
    return Date::formatIsoDate(time.tm_year + 1900, time.tm_mon + 1, time.tm_mday);
}

int sundayBasedWeekday(const Date::DateParts& parts)
{
    return localTimeForDate(parts).tm_wday;
}

std::string currentWeekStartDate(const Date::DateParts& parts)
{
    return offsetDate(parts, -sundayBasedWeekday(parts));
}

std::string currentMonthStartDate(const Date::DateParts& parts)
{
    return Date::formatIsoDate(parts.year, parts.month, 1);
}

double weekElapsedPercent(const Date::DateParts& parts)
{
    // The app stores realized transactions by date, so expected pace uses inclusive calendar days.
    return (static_cast<double>(sundayBasedWeekday(parts)) + 1.0) / 7.0 * 100.0;
}

double monthElapsedPercent(const Date::DateParts& parts)
{
    // The app stores realized transactions by date, so expected pace uses inclusive calendar days.
    return static_cast<double>(parts.day) / static_cast<double>(Date::daysInMonth(parts.year, parts.month)) * 100.0;
}

bool isRealizedCapitalGainTransaction(const Transaction& transaction)
{
    return lowerCopy(transaction.transactionType) == "sell";
}

bool isTransactionInDateRange(const Transaction& transaction, const std::string& startDate, const std::string& endDate)
{
    Date::DateParts ignored;
    return Date::parseIsoDate(transaction.transactionDate, ignored) &&
        transaction.transactionDate >= startDate &&
        transaction.transactionDate <= endDate;
}

double realizedCapitalGainsInRange(const std::vector<Transaction>& transactions, const std::string& startDate, const std::string& endDate)
{
    double total = 0.0;
    for (const Transaction& transaction : transactions) {
        if (!isRealizedCapitalGainTransaction(transaction) || !isTransactionInDateRange(transaction, startDate, endDate)) {
            continue;
        }
        total += transaction.realizedGainDollar;
    }
    return total;
}

void drawPanelChrome(const char* title, ImVec4 accent)
{
    const ImVec2 min = ImGui::GetWindowPos();
    const ImVec2 size = ImGui::GetWindowSize();
    const ImVec2 max(min.x + size.x, min.y + size.y);
    ImDrawList* drawList = ImGui::GetWindowDrawList();

    drawList->AddRectFilled(min, ImVec2(max.x, min.y + 38.0f), ImGui::GetColorU32(UiTheme::withAlpha(UiTheme::SurfaceElevated, 0.16f)), 16.0f, ImDrawFlags_RoundCornersTop);
    drawList->AddRectFilled(ImVec2(min.x + 14.0f, min.y + 9.0f), ImVec2(min.x + 62.0f, min.y + 11.0f), ImGui::GetColorU32(UiTheme::withAlpha(accent, 0.34f)), 2.0f);
    drawList->AddRect(min, max, ImGui::GetColorU32(UiTheme::withAlpha(UiTheme::BorderSubtle, 0.08f)), 16.0f);

    ImGui::TextColored(UiTheme::TextSecondary, "%s", title);
    const ImVec2 cursor = ImGui::GetCursorScreenPos();
    const float width = ImGui::GetContentRegionAvail().x;
    drawList->AddRectFilled(cursor, ImVec2(cursor.x + width, cursor.y + 1.0f), ImGui::GetColorU32(UiTheme::withAlpha(UiTheme::BorderSubtle, 0.08f)));
    ImGui::Dummy(ImVec2(width, 8.0f));
}

void beginDashboardPanel(const char* id, const char* title, ImVec2 size, ImVec4 accent, ImGuiWindowFlags flags = 0)
{
    UiTheme::pushPanelStyle();
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(15.0f, 13.0f));
    ImGui::BeginChild(id, size, true, flags);
    ImGui::PopStyleVar();
    drawPanelChrome(title, accent);
}

void endDashboardPanel()
{
    ImGui::EndChild();
    UiTheme::popPanelStyle();
}

bool isWidgetVisible(const std::vector<DashboardWidget>& widgets, const char* widgetKey)
{
    return std::any_of(widgets.begin(), widgets.end(), [widgetKey](const DashboardWidget& widget) {
        return widget.widgetKey == widgetKey;
    });
}

bool isPromotedDashboardWidget(const std::string& widgetKey)
{
    static constexpr std::array<const char*, 11> PromotedKeys {{
        "portfolio_summary",
        "performance_movement",
        "holdings_table",
        "realized_gains",
        "dividend_summary",
        "recent_transactions",
        "snapshot_status",
        "allocation_panel",
        "performance_panel",
        "income_gains_panel",
        "top_gainers_losers",
    }};

    return std::any_of(PromotedKeys.begin(), PromotedKeys.end(), [&widgetKey](const char* promotedKey) {
        return widgetKey == promotedKey;
    });
}

ImVec4 signalColor(const std::string& signal)
{
    if (signal == "Buy" || signal == "Buy Signal") {
        return UiTheme::Gain;
    }
    if (signal == "Sell" || signal == "Sell Signal") {
        return UiTheme::Loss;
    }
    return UiTheme::ElectricCyan;
}

bool isTransactionCashOutflow(const Transaction& transaction)
{
    return transaction.transactionType == "Buy" ||
        transaction.transactionType == "Withdrawal" ||
        transaction.transactionType == "Fee";
}

bool isTransactionCashInflow(const Transaction& transaction)
{
    return transaction.transactionType == "Sell" ||
        transaction.transactionType == "Dividend" ||
        transaction.transactionType == "Contribution" ||
        transaction.transactionType == "Interest";
}

ImVec4 transactionBadgeColor(const std::string& transactionType)
{
    if (transactionType == "Buy" || transactionType == "Dividend" || transactionType == "Contribution" || transactionType == "Interest") {
        return UiTheme::Gain;
    }
    if (transactionType == "Sell" || transactionType == "Withdrawal" || transactionType == "Fee") {
        return UiTheme::Loss;
    }
    if (transactionType == "Transfer") {
        return UiTheme::ElectricCyan;
    }
    return UiTheme::TextMuted;
}

ImVec4 transactionAmountColor(const Transaction& transaction)
{
    if (transaction.totalAmount < 0.0 || isTransactionCashOutflow(transaction)) {
        return UiTheme::Loss;
    }
    if (transaction.totalAmount > 0.0 && isTransactionCashInflow(transaction)) {
        return UiTheme::Gain;
    }
    return transaction.totalAmount == 0.0 ? UiTheme::TextMuted : UiTheme::TextSecondary;
}

int signalRank(const std::string& signal)
{
    if (signal == "Buy" || signal == "Buy Signal") {
        return 0;
    }
    if (signal == "Sell" || signal == "Sell Signal") {
        return 1;
    }
    return 2;
}

int priorityRank(const std::string& priority)
{
    if (priority == "High") {
        return 0;
    }
    if (priority == "Medium") {
        return 1;
    }
    if (priority == "Low") {
        return 2;
    }
    return 3;
}

std::vector<WatchlistItem> sortedWatchlistItems(const AppState& state)
{
    std::vector<WatchlistItem> items = state.watchlist;
    std::stable_sort(items.begin(), items.end(), [](const WatchlistItem& left, const WatchlistItem& right) {
        return left.ticker < right.ticker;
    });
    std::stable_sort(items.begin(), items.end(), [](const WatchlistItem& left, const WatchlistItem& right) {
        const int leftSignalRank = signalRank(left.signalStatus);
        const int rightSignalRank = signalRank(right.signalStatus);
        if (leftSignalRank != rightSignalRank) {
            return leftSignalRank < rightSignalRank;
        }

        const int leftPriorityRank = priorityRank(left.priority);
        const int rightPriorityRank = priorityRank(right.priority);
        if (leftPriorityRank != rightPriorityRank) {
            return leftPriorityRank < rightPriorityRank;
        }

        return false;
    });
    return items;
}

ImVec4 mixColor(ImVec4 left, ImVec4 right, float amount)
{
    const float t = std::clamp(amount, 0.0f, 1.0f);
    return ImVec4(
        left.x + (right.x - left.x) * t,
        left.y + (right.y - left.y) * t,
        left.z + (right.z - left.z) * t,
        left.w + (right.w - left.w) * t);
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

bool segmentedOptions(const char* id, std::string& value, const std::vector<const char*>& options)
{
    bool changed = false;
    ImGui::PushID(id);
    ImGui::AlignTextToFramePadding();
    ImGui::TextColored(UiTheme::TextMuted, "%s", id);
    ImGui::SameLine();

    const float rowStartX = ImGui::GetCursorPosX();
    const float rowEndX = ImGui::GetWindowContentRegionMax().x;
    for (std::size_t index = 0; index < options.size(); ++index) {
        const char* option = options[index];
        if (index > 0) {
            ImGui::SameLine(0.0f, 5.0f);
        }
        if (ImGui::GetCursorPosX() + 52.0f > rowEndX && ImGui::GetCursorPosX() > rowStartX) {
            ImGui::NewLine();
            ImGui::SetCursorPosX(rowStartX);
        }

        const bool selected = value == option;
        UiTheme::pushButtonStyle(selected ? UiTheme::NeonMagenta : UiTheme::ElectricCyan);
        if (!selected) {
            ImGui::PushStyleColor(ImGuiCol_Text, UiTheme::TextSecondary);
        }
        if (ImGui::Button(option, ImVec2(48.0f, 0.0f))) {
            value = option;
            changed = true;
        }
        if (!selected) {
            ImGui::PopStyleColor();
        }
        UiTheme::popButtonStyle();
    }

    ImGui::PopID();
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
    ImGui::SetNextItemWidth(160.0f);
    changed = stringCombo("Chart Type", setting.chartType, chartTypeOptions) || changed;
    changed = segmentedOptions("Range", setting.timeRange, allowLatest ? timeRangeOptions() : chartRangeOptions()) || changed;

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

void drawPlotFrame(ImDrawList* drawList, const PlotArea& area, ImVec4 accent)
{
    drawList->AddRectFilled(area.min, area.max, ImGui::GetColorU32(UiTheme::withAlpha(UiTheme::SurfaceMain, 0.50f)), 12.0f);
    drawList->AddRect(area.min, area.max, ImGui::GetColorU32(UiTheme::withAlpha(UiTheme::BorderSubtle, 0.08f)), 12.0f);

    constexpr int GridLines = 4;
    for (int index = 1; index < GridLines; ++index) {
        const float y = area.min.y + (area.max.y - area.min.y) * static_cast<float>(index) / static_cast<float>(GridLines);
        drawList->AddLine(ImVec2(area.min.x + 12.0f, y), ImVec2(area.max.x - 12.0f, y), ImGui::GetColorU32(UiTheme::PlotGrid), 1.0f);
    }

    drawList->AddRectFilled(
        ImVec2(area.min.x + 16.0f, area.min.y + 12.0f),
        ImVec2(std::min(area.max.x - 16.0f, area.min.x + 132.0f), area.min.y + 14.0f),
        ImGui::GetColorU32(UiTheme::withAlpha(accent, 0.26f)),
        12.0f,
        0);
}

void drawEmptyPlot(ImDrawList* drawList, const PlotArea& area, const char* message)
{
    const ImU32 muted = ImGui::GetColorU32(UiTheme::MutedText);
    drawPlotFrame(drawList, area, UiTheme::ElectricCyan);
    drawList->AddText(ImVec2(area.min.x + 14.0f, area.min.y + 14.0f), muted, message);
}

void drawGlassTooltip(const ChartPoint& point)
{
    ImGui::PushStyleColor(ImGuiCol_PopupBg, UiTheme::withAlpha(UiTheme::GlassPanel, 0.96f));
    ImGui::PushStyleColor(ImGuiCol_Border, UiTheme::withAlpha(UiTheme::ElectricCyan, 0.42f));
    ImGui::PushStyleVar(ImGuiStyleVar_PopupBorderSize, 1.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(10.0f, 8.0f));
    ImGui::BeginTooltip();
    ImGui::TextColored(UiTheme::TextMuted, "%s", point.label.c_str());
    ImGui::TextColored(UiTheme::TextPrimary, "%s", Money::format(point.value).c_str());
    ImGui::EndTooltip();
    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor(2);
}

void drawBars(const std::vector<double>& values, const std::vector<std::string>& labels, ImVec4 color, const char* emptyMessage)
{
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    const PlotArea area = reservePlotArea(150.0f);

    if (values.empty()) {
        drawEmptyPlot(drawList, area, emptyMessage);
        return;
    }

    drawPlotFrame(drawList, area, color);

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
        drawList->AddRectFilled(ImVec2(x, y), ImVec2(x + barWidth, innerBottom), ImGui::GetColorU32(UiTheme::withAlpha(color, 0.22f)), 5.0f);
        drawList->AddRectFilled(ImVec2(x + 2.0f, y), ImVec2(x + barWidth - 2.0f, innerBottom), barColor, 5.0f);

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

    UiTheme::pushTableStyle();
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
            UiTheme::textRightAligned(Money::formatPercent(percent).c_str());
            ImGui::TableNextColumn();
            UiTheme::textRightAligned(Money::format(point.value).c_str());
            ImGui::TableNextColumn();
            const ImVec2 barMin = ImGui::GetCursorScreenPos();
            const ImVec2 barSize(ImGui::GetContentRegionAvail().x, 14.0f);
            const ImVec2 barMax(barMin.x + barSize.x, barMin.y + barSize.y);
            ImDrawList* drawList = ImGui::GetWindowDrawList();
            drawList->AddRectFilled(barMin, barMax, ImGui::GetColorU32(UiTheme::withAlpha(UiTheme::ElectricCyan, 0.08f)), 7.0f);
            drawList->AddRectFilled(barMin, ImVec2(barMin.x + barSize.x * static_cast<float>(percent / 100.0), barMax.y), ImGui::GetColorU32(UiTheme::withAlpha(UiTheme::ElectricCyan, 0.42f)), 7.0f);
            drawList->AddRect(barMin, barMax, ImGui::GetColorU32(UiTheme::BorderSubtle), 7.0f);
            ImGui::Dummy(barSize);
        }

        ImGui::EndTable();
    }
    UiTheme::popTableStyle();
}

void drawLineChart(const std::vector<ChartPoint>& points, ImVec4 color, const char* emptyMessage, float height = 220.0f, bool gradientLine = false)
{
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    const PlotArea area = reservePlotArea(height);
    const bool hovered = ImGui::IsItemHovered();
    const ImU32 lineColor = ImGui::GetColorU32(color);
    const ImU32 muted = ImGui::GetColorU32(UiTheme::MutedText);
    drawPlotFrame(drawList, area, color);

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

    std::vector<ImVec2> positions;
    positions.reserve(points.size());
    for (std::size_t index = 0; index < points.size(); ++index) {
        const float x = left + (right - left) * static_cast<float>(index) / static_cast<float>(points.size() - 1);
        const double ratio = (points[index].value - minValue) / (maxValue - minValue);
        const float y = bottom - static_cast<float>(ratio) * (bottom - top);
        positions.push_back(ImVec2(x, y));
    }

    for (std::size_t index = 1; index < positions.size(); ++index) {
        const float t = static_cast<float>(index) / static_cast<float>(positions.size() - 1);
        const ImVec4 segmentColor = gradientLine ? mixColor(UiTheme::ChartCyan, UiTheme::ChartMagenta, t) : color;
        drawList->AddLine(positions[index - 1], positions[index], ImGui::GetColorU32(UiTheme::withAlpha(segmentColor, 0.16f)), 7.0f);
        drawList->AddLine(positions[index - 1], positions[index], gradientLine ? ImGui::GetColorU32(segmentColor) : lineColor, 2.4f);
    }

    for (std::size_t index = 0; index < positions.size(); ++index) {
        const bool currentPoint = index + 1 == positions.size();
        const ImVec4 pointColor = currentPoint ? UiTheme::NeonMagenta : (gradientLine ? mixColor(UiTheme::ChartCyan, UiTheme::ChartMagenta, static_cast<float>(index) / static_cast<float>(positions.size() - 1)) : color);
        drawList->AddCircleFilled(positions[index], currentPoint ? 5.6f : 4.0f, ImGui::GetColorU32(UiTheme::SurfaceMain));
        drawList->AddCircleFilled(positions[index], currentPoint ? 3.8f : 2.8f, ImGui::GetColorU32(pointColor));
    }

    const std::string first = points.front().label + ": " + Money::format(points.front().value);
    const std::string last = points.back().label + ": " + Money::format(points.back().value);
    drawList->AddText(ImVec2(area.min.x + 14.0f, area.min.y + 14.0f), muted, first.c_str());
    drawList->AddText(ImVec2(area.min.x + 14.0f, area.max.y - 28.0f), muted, last.c_str());

    if (hovered) {
        const ImVec2 mouse = ImGui::GetIO().MousePos;
        std::size_t nearestIndex = 0;
        float nearestDistance = FLT_MAX;
        for (std::size_t index = 0; index < positions.size(); ++index) {
            const float distance = std::fabs(positions[index].x - mouse.x);
            if (distance < nearestDistance) {
                nearestDistance = distance;
                nearestIndex = index;
            }
        }

        const ImVec2 marker = positions[nearestIndex];
        drawList->AddLine(ImVec2(marker.x, top), ImVec2(marker.x, bottom), ImGui::GetColorU32(UiTheme::withAlpha(UiTheme::NeonMagenta, 0.38f)), 1.0f);
        drawList->AddCircle(marker, 6.8f, ImGui::GetColorU32(UiTheme::NeonMagenta), 24, 1.5f);
        drawGlassTooltip(points[nearestIndex]);
    }
}

void drawMonthlyBarChart(const std::vector<ChartPoint>& points, ImVec4 color, const char* emptyMessage)
{
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    const PlotArea area = reservePlotArea(206.0f);
    const ImU32 positive = ImGui::GetColorU32(color);
    const ImU32 negative = ImGui::GetColorU32(UiTheme::Loss);
    const ImU32 muted = ImGui::GetColorU32(UiTheme::MutedText);
    drawPlotFrame(drawList, area, color);

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

    drawList->AddLine(ImVec2(left, baseline), ImVec2(right, baseline), ImGui::GetColorU32(UiTheme::BorderSubtle), 1.0f);

    for (std::size_t index = 0; index < points.size(); ++index) {
        const ChartPoint& point = points[index];
        const float x = left + static_cast<float>(index) * (barWidth + gap);
        const float height = (bottom - top) * 0.5f * static_cast<float>(std::fabs(point.value) / maxAbs);
        const float y0 = point.value >= 0.0 ? baseline - height : baseline;
        const float y1 = point.value >= 0.0 ? baseline : baseline + height;
        const ImVec4 fill = point.value >= 0.0 ? color : UiTheme::Loss;
        drawList->AddRectFilled(ImVec2(x, y0), ImVec2(x + barWidth, y1), ImGui::GetColorU32(UiTheme::withAlpha(fill, 0.22f)), 4.0f);
        drawList->AddRectFilled(ImVec2(x + 2.0f, y0), ImVec2(x + barWidth - 2.0f, y1), point.value >= 0.0 ? positive : negative, 4.0f);
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

int metricGridColumns(float availableWidth)
{
    return availableWidth > 1180.0f ? 4 : (availableWidth > 820.0f ? 3 : (availableWidth > 520.0f ? 2 : 1));
}

float metricGridHeight(int cardCount, float availableWidth)
{
    if (cardCount <= 0) {
        return 0.0f;
    }

    const int columns = metricGridColumns(availableWidth);
    const int rows = (cardCount + columns - 1) / columns;
    return static_cast<float>(rows) * 112.0f + static_cast<float>(rows - 1) * ImGui::GetStyle().ItemSpacing.y;
}

struct MetricCardData {
    std::string title;
    std::string value;
    std::string caption;
    ImVec4 color;
    bool available = true;
};

struct SummaryMetricData {
    const char* title;
    std::string value;
    std::string caption;
    ImVec4 accent;
    ImVec4 valueColor;
};

void drawSummaryMetricCard(const SummaryMetricData& card, ImVec2 size)
{
    UiTheme::pushPanelStyle();
    ImGui::PushStyleColor(ImGuiCol_Border, UiTheme::withAlpha(card.accent, 0.16f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(15.0f, 14.0f));
    ImGui::BeginChild(card.title, size, true, ImGuiWindowFlags_NoScrollbar);
    ImGui::PopStyleVar();

    const ImVec2 min = ImGui::GetWindowPos();
    const ImVec2 max(min.x + ImGui::GetWindowSize().x, min.y + ImGui::GetWindowSize().y);
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    drawList->AddRectFilled(min, max, ImGui::GetColorU32(UiTheme::withAlpha(UiTheme::SurfaceElevated, 0.36f)), 16.0f);
    drawList->AddRectFilled(min, ImVec2(max.x, min.y + 36.0f), ImGui::GetColorU32(UiTheme::withAlpha(card.accent, 0.030f)), 16.0f, ImDrawFlags_RoundCornersTop);
    drawList->AddRectFilled(ImVec2(min.x + 14.0f, min.y + 10.0f), ImVec2(min.x + 62.0f, min.y + 12.0f), ImGui::GetColorU32(UiTheme::withAlpha(card.accent, 0.44f)), 2.0f);
    drawList->AddRect(min, max, ImGui::GetColorU32(UiTheme::withAlpha(card.accent, 0.14f)), 16.0f);

    const std::string label = uppercaseCopy(card.title);
    ImGui::TextColored(UiTheme::TextMuted, "%s", label.c_str());
    ImGui::SetWindowFontScale(1.24f);
    UiTheme::pushNumericFont();
    ImGui::TextColored(card.valueColor, "%s", card.value.c_str());
    UiTheme::popNumericFont();
    ImGui::SetWindowFontScale(1.0f);
    ImGui::TextColored(UiTheme::TextMuted, "%s", card.caption.c_str());

    ImGui::EndChild();
    ImGui::PopStyleColor();
    UiTheme::popPanelStyle();
}

void drawSummaryMetricRow(const std::vector<SummaryMetricData>& cards)
{
    const float availableWidth = ImGui::GetContentRegionAvail().x;
    const int columns = availableWidth > 1240.0f ? 5 : (availableWidth > 980.0f ? 3 : (availableWidth > 640.0f ? 2 : 1));
    const float gap = ImGui::GetStyle().ItemSpacing.x;
    const float cardWidth = (availableWidth - gap * static_cast<float>(columns - 1)) / static_cast<float>(columns);
    const ImVec2 cardSize(cardWidth, 132.0f);

    for (std::size_t index = 0; index < cards.size(); ++index) {
        drawSummaryMetricCard(cards[index], cardSize);
        nextCardColumn(static_cast<int>(index), columns);
    }
}

void drawMetricGrid(const std::vector<MetricCardData>& cards)
{
    if (cards.empty()) {
        return;
    }

    const float availableWidth = ImGui::GetContentRegionAvail().x;
    const int columns = metricGridColumns(availableWidth);
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

struct TradingGoalProgress {
    const char* title = "";
    const char* realizedLabel = "";
    double goal = 0.0;
    double current = 0.0;
    double remaining = 0.0;
    double progressPercent = 0.0;
    double expectedPercent = 0.0;
    bool goalSet = false;
    const char* statusLabel = "";
    ImVec4 statusColor = UiTheme::MutedText;
};

TradingGoalProgress buildTradingGoalProgress(
    const char* title,
    const char* realizedLabel,
    double goal,
    double current,
    double expectedPercent)
{
    TradingGoalProgress progress;
    progress.title = title;
    progress.realizedLabel = realizedLabel;
    progress.goal = goal;
    progress.current = current;
    progress.expectedPercent = std::clamp(expectedPercent, 0.0, 100.0);
    progress.goalSet = goal > 0.005;
    progress.remaining = progress.goalSet ? std::max(0.0, goal - current) : 0.0;
    progress.progressPercent = progress.goalSet ? (current / goal) * 100.0 : 0.0;

    // Status compares actual progress with date-based expected pace: at pace is green,
    // less than half pace or negative is red, and the middle zone is watch.
    if (!progress.goalSet) {
        progress.statusLabel = "Goal not set";
        progress.statusColor = UiTheme::MutedText;
    } else if (current < -0.005) {
        progress.statusLabel = "Off Track";
        progress.statusColor = UiTheme::Loss;
    } else if (progress.progressPercent + 0.5 >= progress.expectedPercent) {
        progress.statusLabel = "On Track";
        progress.statusColor = UiTheme::Gain;
    } else if (progress.current > 0.005 && progress.progressPercent >= progress.expectedPercent * 0.5) {
        progress.statusLabel = "Watch";
        progress.statusColor = UiTheme::Amber;
    } else {
        progress.statusLabel = "Off Track";
        progress.statusColor = UiTheme::Loss;
    }

    return progress;
}

void drawTradingGoalValue(const char* label, const std::string& value, ImVec4 color)
{
    ImGui::TextColored(UiTheme::TextMuted, "%s", label);
    UiTheme::pushNumericFont();
    ImGui::TextColored(color, "%s", value.c_str());
    UiTheme::popNumericFont();
}

void drawTradingGoalProgressBar(const TradingGoalProgress& goal)
{
    const float width = std::max(1.0f, ImGui::GetContentRegionAvail().x);
    const float height = 12.0f;
    const ImVec2 barMin = ImGui::GetCursorScreenPos();
    const ImVec2 barMax(barMin.x + width, barMin.y + height);
    ImDrawList* drawList = ImGui::GetWindowDrawList();

    drawList->AddRectFilled(barMin, barMax, ImGui::GetColorU32(UiTheme::withAlpha(UiTheme::SurfaceMain, 0.72f)), 6.0f);
    drawList->AddRect(barMin, barMax, ImGui::GetColorU32(UiTheme::withAlpha(UiTheme::BorderSubtle, 0.14f)), 6.0f);

    if (goal.goalSet) {
        const float fillRatio = static_cast<float>(std::clamp(goal.progressPercent / 100.0, 0.0, 1.0));
        if (fillRatio > 0.0f) {
            drawList->AddRectFilled(
                barMin,
                ImVec2(barMin.x + width * fillRatio, barMax.y),
                ImGui::GetColorU32(UiTheme::withAlpha(goal.statusColor, 0.54f)),
                6.0f);
        }

        const float expectedRatio = static_cast<float>(std::clamp(goal.expectedPercent / 100.0, 0.0, 1.0));
        const float expectedX = barMin.x + width * expectedRatio;
        drawList->AddLine(
            ImVec2(expectedX, barMin.y - 2.0f),
            ImVec2(expectedX, barMax.y + 2.0f),
            ImGui::GetColorU32(UiTheme::withAlpha(UiTheme::TextSecondary, 0.72f)),
            1.4f);
    }

    ImGui::Dummy(ImVec2(width, height + 2.0f));
}

void drawTradingGoalCard(const TradingGoalProgress& goal, const char* id, ImVec2 size, const std::function<void()>& openGoalsEditor)
{
    const ImVec4 accent = goal.goalSet ? goal.statusColor : UiTheme::ElectricCyan;

    ImGui::PushID(id);
    ImGui::PushStyleColor(ImGuiCol_ChildBg, UiTheme::withAlpha(UiTheme::SurfaceElevated, 0.34f));
    ImGui::PushStyleColor(ImGuiCol_Border, UiTheme::withAlpha(accent, 0.18f));
    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 14.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_ChildBorderSize, 1.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(14.0f, 12.0f));
    ImGui::BeginChild("TradingGoalCard", size, true, ImGuiWindowFlags_NoScrollbar);
    ImGui::PopStyleVar(3);

    const ImVec2 min = ImGui::GetWindowPos();
    const ImVec2 max(min.x + ImGui::GetWindowSize().x, min.y + ImGui::GetWindowSize().y);
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    drawList->AddRectFilled(min, max, ImGui::GetColorU32(UiTheme::withAlpha(UiTheme::SurfaceElevated, 0.22f)), 14.0f);
    drawList->AddRectFilled(min, ImVec2(max.x, min.y + 34.0f), ImGui::GetColorU32(UiTheme::withAlpha(accent, 0.030f)), 14.0f, ImDrawFlags_RoundCornersTop);
    drawList->AddRectFilled(ImVec2(min.x + 13.0f, min.y + 9.0f), ImVec2(min.x + 58.0f, min.y + 11.0f), ImGui::GetColorU32(UiTheme::withAlpha(accent, 0.44f)), 2.0f);

    const std::string title = uppercaseCopy(goal.title);
    ImGui::TextColored(UiTheme::TextSecondary, "%s", title.c_str());
    const float badgeWidth = ImGui::CalcTextSize(goal.statusLabel).x + 18.0f;
    ImGui::SameLine(std::max(ImGui::GetCursorPosX() + 8.0f, ImGui::GetWindowContentRegionMax().x - badgeWidth));
    UiTheme::badge(goal.statusLabel, goal.statusColor);

    ImGui::SetWindowFontScale(1.20f);
    UiTheme::pushNumericFont();
    ImGui::TextColored(goal.goalSet ? UiTheme::TextPrimary : UiTheme::TextMuted, "%s", goal.goalSet ? Money::format(goal.goal).c_str() : "Goal not set");
    UiTheme::popNumericFont();
    ImGui::SetWindowFontScale(1.0f);
    ImGui::TextColored(UiTheme::TextMuted, "Goal Amount");

    ImGui::Spacing();
    if (!goal.goalSet) {
        ImGui::TextColored(UiTheme::TextMuted, "No realized gains target is saved for this period.");
        UiTheme::pushButtonStyle(UiTheme::ElectricCyan);
        if (ImGui::Button("Set Goal", ImVec2(96.0f, 0.0f))) {
            openGoalsEditor();
        }
        UiTheme::popButtonStyle();
        ImGui::EndChild();
        ImGui::PopStyleColor(2);
        ImGui::PopID();
        return;
    }

    if (ImGui::BeginTable("TradingGoalStats", 2, ImGuiTableFlags_SizingStretchSame)) {
        ImGui::TableNextColumn();
        drawTradingGoalValue(goal.realizedLabel, Money::format(goal.current), UiTheme::moneyColor(goal.current));
        ImGui::TableNextColumn();
        drawTradingGoalValue("Remaining", Money::format(goal.remaining), goal.remaining <= 0.005 ? UiTheme::Gain : UiTheme::TextPrimary);
        ImGui::TableNextColumn();
        drawTradingGoalValue("Progress", Money::formatPercent(goal.progressPercent), goal.statusColor);
        ImGui::TableNextColumn();
        drawTradingGoalValue("Expected", Money::formatPercent(goal.expectedPercent), UiTheme::TextMuted);
        ImGui::EndTable();
    }

    ImGui::Spacing();
    drawTradingGoalProgressBar(goal);

    ImGui::EndChild();
    ImGui::PopStyleColor(2);
    ImGui::PopID();
}

void drawTradingGoalsSection(const AppState& state, const std::string& today, const std::function<void()>& openGoalsEditor)
{
    Date::DateParts todayParts;
    if (!Date::parseIsoDate(today, todayParts)) {
        todayParts = Date::todayParts();
    }
    const std::string todayForRange = Date::formatIsoDate(todayParts.year, todayParts.month, todayParts.day);
    const double weeklyRealized = realizedCapitalGainsInRange(state.transactions, currentWeekStartDate(todayParts), todayForRange);
    const double monthlyRealized = realizedCapitalGainsInRange(state.transactions, currentMonthStartDate(todayParts), todayForRange);

    const TradingGoalProgress weeklyGoal = buildTradingGoalProgress(
        "Weekly Capital Gains Goal",
        "Current Week",
        state.weeklyRealizedCapitalGainsGoal,
        weeklyRealized,
        weekElapsedPercent(todayParts));
    const TradingGoalProgress monthlyGoal = buildTradingGoalProgress(
        "Monthly Capital Gains Goal",
        "Current Month",
        state.monthlyRealizedCapitalGainsGoal,
        monthlyRealized,
        monthElapsedPercent(todayParts));

    const float availableWidth = ImGui::GetContentRegionAvail().x;
    const bool useTwoColumns = availableWidth > 780.0f;
    const float cardHeight = 218.0f;
    const float spacingY = ImGui::GetStyle().ItemSpacing.y;
    const float panelHeight = 64.0f + (useTwoColumns ? cardHeight : cardHeight * 2.0f + spacingY);

    beginDashboardPanel("DashboardTradingGoalsPanel", "Trading Goals", ImVec2(0.0f, panelHeight), UiTheme::ElectricCyan);
    if (useTwoColumns && ImGui::BeginTable("TradingGoalsGrid", 2, ImGuiTableFlags_SizingStretchSame)) {
        ImGui::TableNextColumn();
        drawTradingGoalCard(weeklyGoal, "WeeklyTradingGoal", ImVec2(0.0f, cardHeight), openGoalsEditor);
        ImGui::TableNextColumn();
        drawTradingGoalCard(monthlyGoal, "MonthlyTradingGoal", ImVec2(0.0f, cardHeight), openGoalsEditor);
        ImGui::EndTable();
    } else {
        drawTradingGoalCard(weeklyGoal, "WeeklyTradingGoal", ImVec2(0.0f, cardHeight), openGoalsEditor);
        ImGui::Spacing();
        drawTradingGoalCard(monthlyGoal, "MonthlyTradingGoal", ImVec2(0.0f, cardHeight), openGoalsEditor);
    }
    endDashboardPanel();
}

std::string movementCaption(bool available)
{
    return available ? "Local snapshot movement" : "Not enough snapshot data yet";
}

void drawPortfolioSummary(const DashboardData& data)
{
    const float availableWidth = ImGui::GetContentRegionAvail().x;
    const int columns = availableWidth > 1240.0f ? 5 : (availableWidth > 980.0f ? 3 : (availableWidth > 640.0f ? 2 : 1));
    const int rows = (5 + columns - 1) / columns;
    const float panelHeight = 66.0f + static_cast<float>(rows) * 132.0f + static_cast<float>(rows - 1) * ImGui::GetStyle().ItemSpacing.y;

    beginDashboardPanel("PortfolioOverviewPanel", "Portfolio Summary", ImVec2(0.0f, panelHeight), UiTheme::NeonMagenta);
    drawSummaryMetricRow({
        { "Total Portfolio Value", Money::format(data.portfolio.accountBalance), "Holdings plus cash", UiTheme::NeonMagenta, UiTheme::TextPrimary },
        { "Cash Balance", Money::format(data.portfolio.cashBalance), "Account cash", UiTheme::ElectricCyan, UiTheme::ElectricCyan },
        { "Holdings Value", Money::format(data.portfolio.holdingsMarketValue), "Shares x current price", UiTheme::SoftBlue, UiTheme::TextPrimary },
        { "Unrealized Gain/Loss", Money::format(data.portfolio.gainLossDollar), Money::formatPercent(data.portfolio.gainLossPercent, true), UiTheme::Gain, UiTheme::moneyColor(data.portfolio.gainLossDollar) },
        { "Realized Gain/Loss", Money::format(data.realizedGains.thisYear), "This year realized", UiTheme::Loss, UiTheme::moneyColor(data.realizedGains.thisYear) },
    });
    endDashboardPanel();
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

bool drawDashboardSettingsButton(const char* id, ImVec2 size)
{
    ImGui::PushID(id);
    const ImVec2 min = ImGui::GetCursorScreenPos();
    const bool pressed = ImGui::InvisibleButton("SettingsCog", size);
    const bool hovered = ImGui::IsItemHovered();
    const bool active = ImGui::IsItemActive();
    const ImVec2 max(min.x + size.x, min.y + size.y);
    const ImVec2 center((min.x + max.x) * 0.5f, (min.y + max.y) * 0.5f);
    ImDrawList* drawList = ImGui::GetWindowDrawList();

    const ImVec4 fill = active
        ? UiTheme::withAlpha(UiTheme::ElectricCyan, 0.18f)
        : hovered ? UiTheme::withAlpha(UiTheme::ElectricCyan, 0.12f) : UiTheme::withAlpha(UiTheme::SurfaceElevated, 0.30f);
    const ImVec4 border = hovered
        ? UiTheme::withAlpha(UiTheme::ElectricCyan, 0.50f)
        : UiTheme::withAlpha(UiTheme::BorderSubtle, 0.24f);
    const ImU32 iconColor = ImGui::GetColorU32(hovered ? UiTheme::ElectricCyan : UiTheme::TextSecondary);

    drawList->AddRectFilled(min, max, ImGui::GetColorU32(fill), 8.0f);
    drawList->AddRect(min, max, ImGui::GetColorU32(border), 8.0f);

    constexpr float Pi = 3.14159265359f;
    for (int index = 0; index < 8; ++index) {
        const float angle = (static_cast<float>(index) * Pi) / 4.0f;
        const ImVec2 inner(center.x + std::cos(angle) * 7.0f, center.y + std::sin(angle) * 7.0f);
        const ImVec2 outer(center.x + std::cos(angle) * 9.4f, center.y + std::sin(angle) * 9.4f);
        drawList->AddLine(inner, outer, iconColor, 1.4f);
    }
    drawList->AddCircle(center, 6.2f, iconColor, 18, 1.5f);
    drawList->AddCircleFilled(center, 2.0f, iconColor, 12);

    ImGui::PopID();
    return pressed;
}

void drawDashboardSnapshotReplacePopup(bool& showPopup, const std::function<void(bool)>& createSnapshot)
{
    constexpr const char* PopupId = "Replace Dashboard Snapshot From Settings";

    if (showPopup) {
        ImGui::OpenPopup(PopupId);
        showPopup = false;
    }

    if (ImGui::BeginPopupModal(PopupId, nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("A snapshot already exists for today.");
        ImGui::TextColored(UiTheme::MutedText, "Replace it with the current local portfolio totals?");
        ImGui::Spacing();
        if (ImGui::Button("Replace Snapshot", ImVec2(150.0f, 0.0f))) {
            createSnapshot(true);
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(100.0f, 0.0f))) {
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
}

void drawDashboardSettingsMenu(
    AppState& state,
    DashboardLayoutRepository& layoutRepository,
    bool customizeMode,
    const std::string& today,
    const std::function<void()>& toggleCustomize,
    const std::function<void()>& refreshCurrentPrices,
    const std::function<void()>& reloadData,
    const std::function<void(bool)>& createSnapshot,
    bool& showSnapshotReplacePopup,
    bool& openCapitalGainsGoalsEditor)
{
    constexpr const char* PopupId = "DashboardSettingsMenu";
    const float buttonSize = 34.0f;
    const float currentX = ImGui::GetCursorPosX();
    const float rightX = currentX + ImGui::GetContentRegionAvail().x - buttonSize;
    ImGui::SetCursorPosX(std::max(currentX, rightX));

    if (drawDashboardSettingsButton("DashboardSettings", ImVec2(buttonSize, buttonSize))) {
        ImGui::OpenPopup(PopupId);
    }
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Dashboard settings");
    }

    ImGui::PushStyleColor(ImGuiCol_PopupBg, UiTheme::withAlpha(UiTheme::GlassPanel, 0.98f));
    ImGui::PushStyleColor(ImGuiCol_Border, UiTheme::withAlpha(UiTheme::ElectricCyan, 0.34f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(10.0f, 8.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_PopupBorderSize, 1.0f);
    if (ImGui::BeginPopup(PopupId)) {
        ImGui::TextColored(UiTheme::TextSecondary, "Dashboard Settings");
        ImGui::Separator();

        if (ImGui::MenuItem(customizeMode ? "Done Customizing" : "Customize Dashboard")) {
            toggleCustomize();
        }
        if (ImGui::MenuItem("Refresh Current Prices")) {
            refreshCurrentPrices();
        }
        if (ImGui::MenuItem("Refresh Dashboard")) {
            reloadData();
            state.setStatus("Dashboard refreshed.");
        }
        if (ImGui::MenuItem("Capital Gains Goals")) {
            openCapitalGainsGoalsEditor = true;
        }
        if (ImGui::MenuItem("Create Manual Snapshot")) {
            if (DashboardService::hasSnapshotForDate(state, today)) {
                showSnapshotReplacePopup = true;
            } else {
                createSnapshot(false);
            }
        }

        ImGui::Separator();
        if (ImGui::MenuItem("Reset Dashboard Layout")) {
            std::string error;
            if (layoutRepository.resetToDefaults(error)) {
                reloadData();
                state.setStatus("Dashboard layout reset.");
            } else {
                state.setStatus("Could not reset dashboard layout: " + error, true);
            }
        }

        ImGui::EndPopup();
    }
    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor(2);

    drawDashboardSnapshotReplacePopup(showSnapshotReplacePopup, createSnapshot);
}

void drawDashboardWatchlistPanel(const AppState& state)
{
    beginDashboardPanel("DashboardWatchlistPanel", "Watchlist", ImVec2(0.0f, 330.0f), UiTheme::ElectricCyan);
    const std::vector<WatchlistItem> items = sortedWatchlistItems(state);
    if (items.empty()) {
        ImGui::TextColored(UiTheme::TextMuted, "No watchlist items yet.");
        ImGui::TextWrapped("Create watchlist items from the Watchlist page to monitor local symbols here.");
        endDashboardPanel();
        return;
    }

    UiTheme::pushTableStyle();
    if (ImGui::BeginTable("DashboardWatchlistTable", 4, ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerH | ImGuiTableFlags_SizingStretchProp, ImVec2(0.0f, 250.0f))) {
        ImGui::TableSetupColumn("Ticker", ImGuiTableColumnFlags_WidthFixed, 74.0f);
        ImGui::TableSetupColumn("Price", ImGuiTableColumnFlags_WidthFixed, 96.0f);
        ImGui::TableSetupColumn("Priority", ImGuiTableColumnFlags_WidthFixed, 74.0f);
        ImGui::TableSetupColumn("Signal", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableHeadersRow();

        const int limit = std::min<int>(8, static_cast<int>(items.size()));
        for (int index = 0; index < limit; ++index) {
            const WatchlistItem& item = items[static_cast<std::size_t>(index)];
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::Text("%s", item.ticker.c_str());
            ImGui::TableNextColumn();
            UiTheme::textRightAligned(item.currentPrice > 0.0 ? Money::format(item.currentPrice).c_str() : "N/A");
            ImGui::TableNextColumn();
            UiTheme::badge(item.priority.c_str(), item.priority == "High" ? UiTheme::Amber : UiTheme::TextMuted);
            ImGui::TableNextColumn();
            UiTheme::badge(item.signalStatus.c_str(), signalColor(item.signalStatus));
        }
        ImGui::EndTable();
    }
    UiTheme::popTableStyle();
    endDashboardPanel();
}

void drawTechnicalSignalsPanel(const AppState& state)
{
    beginDashboardPanel("DashboardTechnicalSignalsPanel", "Technical Signals", ImVec2(0.0f, 260.0f), UiTheme::NeonMagenta);

    int buyCount = 0;
    int sellCount = 0;
    int holdCount = 0;
    for (const WatchlistItem& item : state.watchlist) {
        if (item.signalStatus == "Buy" || item.signalStatus == "Buy Signal") {
            ++buyCount;
        } else if (item.signalStatus == "Sell" || item.signalStatus == "Sell Signal") {
            ++sellCount;
        } else {
            ++holdCount;
        }
    }

    const float columnWidth = (ImGui::GetContentRegionAvail().x - ImGui::GetStyle().ItemSpacing.x * 2.0f) / 3.0f;
    UiTheme::metricCard("Buy", std::to_string(buyCount).c_str(), "Watchlist signals", UiTheme::Gain, ImVec2(columnWidth, 82.0f));
    ImGui::SameLine();
    UiTheme::metricCard("Hold", std::to_string(holdCount).c_str(), "Watchlist signals", UiTheme::ElectricCyan, ImVec2(columnWidth, 82.0f));
    ImGui::SameLine();
    UiTheme::metricCard("Sell", std::to_string(sellCount).c_str(), "Watchlist signals", UiTheme::Loss, ImVec2(columnWidth, 82.0f));

    ImGui::Spacing();
    const std::vector<WatchlistItem> items = sortedWatchlistItems(state);
    if (items.empty()) {
        ImGui::TextColored(UiTheme::TextMuted, "Signals appear after watchlist items are added.");
        endDashboardPanel();
        return;
    }

    UiTheme::pushTableStyle();
    if (ImGui::BeginTable("DashboardTechnicalSignalsTable", 3, ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerH | ImGuiTableFlags_SizingStretchProp, ImVec2(0.0f, 92.0f))) {
        ImGui::TableSetupColumn("Ticker", ImGuiTableColumnFlags_WidthFixed, 72.0f);
        ImGui::TableSetupColumn("Buy Level", ImGuiTableColumnFlags_WidthFixed, 94.0f);
        ImGui::TableSetupColumn("Signal");
        ImGui::TableHeadersRow();

        const int limit = std::min<int>(4, static_cast<int>(items.size()));
        for (int index = 0; index < limit; ++index) {
            const WatchlistItem& item = items[static_cast<std::size_t>(index)];
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::Text("%s", item.ticker.c_str());
            ImGui::TableNextColumn();
            UiTheme::textRightAligned(item.buySignalPrice > 0.0 ? Money::format(item.buySignalPrice).c_str() : "N/A", UiTheme::Gain);
            ImGui::TableNextColumn();
            UiTheme::badge(item.signalStatus.c_str(), signalColor(item.signalStatus));
        }
        ImGui::EndTable();
    }
    UiTheme::popTableStyle();
    endDashboardPanel();
}

void drawAccountStatusPanel(
    AppState& state,
    const DashboardData& data,
    const std::string& today,
    const std::function<void()>& refreshCurrentPrices,
    const std::function<void()>& reloadData,
    const std::function<void(bool)>& createSnapshot,
    bool showSnapshotControls)
{
    beginDashboardPanel("DashboardAccountStatusPanel", "Account / Data Status", ImVec2(0.0f, showSnapshotControls ? 310.0f : 238.0f), UiTheme::SoftBlue);

    const DashboardPriceRefreshStatus& status = state.dashboardPriceRefreshStatus;
    const PortfolioSnapshot* latest = DashboardService::latestSnapshot(state);
    const ImportBatch* latestImport = DashboardService::latestImportBatch(state);

    ImGui::TextColored(UiTheme::TextMuted, "Active Accounts");
    ImGui::SameLine(150.0f);
    ImGui::TextColored(UiTheme::TextPrimary, "%d", data.portfolio.activeAccounts);
    ImGui::TextColored(UiTheme::TextMuted, "Holdings");
    ImGui::SameLine(150.0f);
    ImGui::TextColored(UiTheme::TextPrimary, "%d", data.portfolio.holdingCount);
    ImGui::TextColored(UiTheme::TextMuted, "Last CSV Import");
    ImGui::SameLine(150.0f);
    ImGui::TextColored(UiTheme::TextSecondary, "%s", latestImport == nullptr ? "None" : latestImport->importDate.c_str());
    ImGui::TextColored(UiTheme::TextMuted, "Last Snapshot");
    ImGui::SameLine(150.0f);
    ImGui::TextColored(UiTheme::TextSecondary, "%s", latest == nullptr ? "None" : latest->snapshotDate.c_str());
    ImGui::TextColored(UiTheme::TextMuted, "Price Provider");
    ImGui::SameLine(150.0f);
    ImGui::TextColored(UiTheme::ElectricCyan, "%s", status.provider.empty() ? "Yahoo Finance" : status.provider.c_str());
    ImGui::TextColored(UiTheme::TextMuted, "Last Refresh");
    ImGui::SameLine(150.0f);
    ImGui::TextColored(UiTheme::TextSecondary, "%s", status.hasRun && !status.lastRefreshedAt.empty() ? status.lastRefreshedAt.c_str() : "Not refreshed this session");

    ImGui::Spacing();
    UiTheme::pushButtonStyle(UiTheme::ElectricCyan);
    if (ImGui::Button("Refresh Current Prices", ImVec2(178.0f, 0.0f))) {
        refreshCurrentPrices();
    }
    UiTheme::popButtonStyle();
    ImGui::SameLine();
    ImGui::TextColored(UiTheme::TextMuted, "Updated: %d   Failed: %d   Cached: %d", status.refreshedSymbols, status.failedSymbols, status.cachedSymbols);

    if (!status.warning.empty()) {
        ImGui::TextColored(status.failedSymbols > 0 ? UiTheme::Amber : UiTheme::TextMuted, "%s", status.warning.c_str());
    }

    if (showSnapshotControls) {
        ImGui::Spacing();
        UiTheme::pushButtonStyle(UiTheme::NeonMagenta);
        if (ImGui::Button("Create Manual Snapshot", ImVec2(190.0f, 0.0f))) {
            if (DashboardService::hasSnapshotForDate(state, today)) {
                ImGui::OpenPopup("Replace Today Snapshot");
            } else {
                createSnapshot(false);
            }
        }
        UiTheme::popButtonStyle();
        ImGui::SameLine();
        ImGui::TextColored(UiTheme::TextMuted, "CSV imports create snapshots automatically.");
    }

    endDashboardPanel();

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

void drawSingleMetricSection(const MetricCardData& card)
{
    TerminalPanel::begin(card.title.c_str(), ImVec2(0.0f, 150.0f));
    drawMetricGrid({ card });
    TerminalPanel::end();
}

void drawPerformanceMovement(const DashboardData& data, const AppState& state)
{
    const float panelHeight = 92.0f + metricGridHeight(3, ImGui::GetContentRegionAvail().x) + 46.0f;
    beginDashboardPanel("DashboardPerformanceMovementPanel", "Performance Movement", ImVec2(0.0f, panelHeight), UiTheme::Gain);
    drawMetricGrid({
        { "Daily Gain/Loss", data.performance.hasDaily ? Money::format(data.performance.daily) : "N/A", movementCaption(data.performance.hasDaily), UiTheme::moneyColor(data.performance.daily), data.performance.hasDaily },
        { "Monthly Gain/Loss", data.performance.hasMonthly ? Money::format(data.performance.monthly) : "N/A", movementCaption(data.performance.hasMonthly), UiTheme::moneyColor(data.performance.monthly), data.performance.hasMonthly },
        { "Yearly Gain/Loss", data.performance.hasYearly ? Money::format(data.performance.yearly) : "N/A", movementCaption(data.performance.hasYearly), UiTheme::moneyColor(data.performance.yearly), data.performance.hasYearly },
    });

    ImGui::TextColored(UiTheme::MutedText, state.portfolioSnapshots.empty()
        ? "Import a CSV to create your first snapshot. Manual snapshots are available as an optional advanced tool."
        : "Performance is based on local snapshots created by CSV imports or manual snapshots and may include deposits or withdrawals. Contribution-adjusted returns are planned for a future update.");
    endDashboardPanel();
}

void drawRealizedGainsPanel(const DashboardData& data)
{
    beginDashboardPanel("DashboardRealizedGainsPanel", "Realized Gains", ImVec2(0.0f, 198.0f), UiTheme::Loss);
    ImGui::TextColored(UiTheme::moneyColor(data.realizedGains.today), "Today: %s", Money::format(data.realizedGains.today).c_str());
    ImGui::TextColored(UiTheme::moneyColor(data.realizedGains.thisMonth), "This month: %s", Money::format(data.realizedGains.thisMonth).c_str());
    ImGui::TextColored(UiTheme::moneyColor(data.realizedGains.thisYear), "This year: %s", Money::format(data.realizedGains.thisYear).c_str());
    ImGui::TextColored(UiTheme::MutedText, "Sell transactions: %d", data.realizedGains.sellCount);
    ImGui::Spacing();
    ImGui::TextWrapped("Realized gain/loss is separate from unrealized holding movement and is calculated from sell transactions using average cost basis.");
    endDashboardPanel();
}

void drawDividendSummaryPanel(const DashboardData& data)
{
    const float panelHeight = 76.0f + metricGridHeight(3, ImGui::GetContentRegionAvail().x);
    beginDashboardPanel("DashboardDividendSummaryPanel", "Dividend Summary", ImVec2(0.0f, panelHeight), UiTheme::Amber);
    drawMetricGrid({
        { "Dividends This Month", Money::format(data.dividends.thisMonth), "Recorded dividends", UiTheme::Amber },
        { "Dividends This Year", Money::format(data.dividends.thisYear), "Recorded dividends", UiTheme::Amber },
        { "Lifetime Dividends", Money::format(data.dividends.lifetime), "Recorded dividends", UiTheme::Amber },
    });
    endDashboardPanel();
}

void drawRecentTransactionsPanel(const AppState& state)
{
    beginDashboardPanel("DashboardRecentTransactionsPanel", "Recent Transactions", ImVec2(0.0f, 300.0f), UiTheme::ElectricCyan);
    if (state.transactions.empty()) {
        ImGui::TextColored(UiTheme::TextMuted, "No transactions yet.");
        ImGui::TextWrapped("Add buys, sells, dividends, and cash activity to build the ledger.");
    } else {
        UiTheme::pushTableStyle();
        if (ImGui::BeginTable("DashboardRecentTransactionsTable", 4, ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerH | ImGuiTableFlags_SizingStretchProp, ImVec2(0.0f, 222.0f))) {
            ImGui::TableSetupColumn("Date", ImGuiTableColumnFlags_WidthFixed, 92.0f);
            ImGui::TableSetupColumn("Type", ImGuiTableColumnFlags_WidthFixed, 84.0f);
            ImGui::TableSetupColumn("Symbol");
            ImGui::TableSetupColumn("Amount", ImGuiTableColumnFlags_WidthFixed, 112.0f);
            ImGui::TableHeadersRow();

            const int limit = std::min<int>(8, static_cast<int>(state.transactions.size()));
            for (int index = 0; index < limit; ++index) {
                const Transaction& transaction = state.transactions[static_cast<std::size_t>(index)];
                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                ImGui::Text("%s", transaction.transactionDate.c_str());
                ImGui::TableNextColumn();
                UiTheme::badge(transaction.transactionType.c_str(), transactionBadgeColor(transaction.transactionType));
                ImGui::TableNextColumn();
                ImGui::Text("%s", transaction.ticker.empty() ? transaction.assetName.c_str() : transaction.ticker.c_str());
                ImGui::TableNextColumn();
                UiTheme::textRightAligned(Money::format(transaction.totalAmount).c_str(), transactionAmountColor(transaction));
            }
            ImGui::EndTable();
        }
        UiTheme::popTableStyle();
    }
    endDashboardPanel();
}

void drawHoldingsTablePanel(const AppState& state)
{
    beginDashboardPanel("DashboardHoldingsPanel", "Holdings", ImVec2(0.0f, 330.0f), UiTheme::SoftBlue);
    const std::vector<Holding> holdings = dashboardHoldings(state);
    if (holdings.empty()) {
        ImGui::TextColored(UiTheme::TextMuted, "No holdings yet.");
        ImGui::TextWrapped("Import a positions CSV or add holdings manually.");
        endDashboardPanel();
        return;
    }

    UiTheme::pushTableStyle();
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
            UiTheme::textRightAligned(Money::formatQuantity(holding.shares).c_str());
            ImGui::TableNextColumn();
            UiTheme::textRightAligned(Money::format(holding.currentPrice).c_str(), UiTheme::ElectricCyan);
            ImGui::TableNextColumn();
            const std::string priceSource = DashboardService::priceSourceForHolding(state, holding);
            ImGui::TextColored(priceSource == "Live Quote" ? UiTheme::Gain : (priceSource == "Cached Quote" ? UiTheme::Amber : UiTheme::MutedText), "%s", priceSource.c_str());
            ImGui::TableNextColumn();
            UiTheme::textRightAligned(Money::format(metrics.marketValue).c_str());
            ImGui::TableNextColumn();
            UiTheme::textRightAligned(Money::format(metrics.gainLossDollar).c_str(), UiTheme::moneyColor(metrics.gainLossDollar));
            ++rendered;
            if (rendered >= 18) {
                break;
            }
        }
        ImGui::EndTable();
    }
    UiTheme::popTableStyle();
    endDashboardPanel();
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
    beginDashboardPanel("DashboardAllocationPanel", "Allocation", ImVec2(0.0f, 330.0f), UiTheme::SoftBlue);
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

    endDashboardPanel();
}

void drawPerformanceChartPanel(AppState& state, DashboardChartSettingsRepository& chartSettingsRepository)
{
    beginDashboardPanel("DashboardPerformanceChartPanel", "Portfolio Performance", ImVec2(0.0f, 382.0f), UiTheme::ElectricCyan);
    DashboardChartSetting setting = chartSettingFor(state, "performance_panel");
    drawChartControls(state,
        chartSettingsRepository,
        setting,
        { "Portfolio Value", "Holdings Value", "Cash Balance", "Unrealized Gain/Loss" },
        { "Line Chart" },
        false);

    const std::vector<ChartPoint> points = performancePoints(state, setting.dataMode, setting.timeRange);
    const ImVec4 chartColor = setting.dataMode == "Unrealized Gain/Loss" ? UiTheme::moneyColor(points.empty() ? 0.0 : points.back().value) : UiTheme::ChartCyan;
    drawLineChart(points, chartColor, "Not enough snapshot data for this range.", 250.0f, setting.dataMode != "Unrealized Gain/Loss");
    ImGui::TextColored(UiTheme::MutedText, "Performance charts use local portfolio snapshots.");
    endDashboardPanel();
}

void drawIncomeGainsChartPanel(AppState& state, DashboardChartSettingsRepository& chartSettingsRepository)
{
    beginDashboardPanel("DashboardIncomeGainsPanel", "Income / Gains", ImVec2(0.0f, 330.0f), UiTheme::Amber);
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
    endDashboardPanel();
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
    beginDashboardPanel("DashboardTopMoversPanel", "Market Highlights", ImVec2(0.0f, 356.0f), UiTheme::NeonMagenta);
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
        ImGui::TextColored(UiTheme::TextMuted, "No holdings yet.");
        ImGui::TextWrapped("Import or add holdings to review gain/loss movement.");
        endDashboardPanel();
        return;
    }

    const int tileCount = std::min<int>(3, static_cast<int>(rows.size()));
    const float tileWidth = (ImGui::GetContentRegionAvail().x - ImGui::GetStyle().ItemSpacing.x * static_cast<float>(tileCount - 1)) / static_cast<float>(tileCount);
    for (int index = 0; index < tileCount; ++index) {
        const HoldingRow& row = rows[static_cast<std::size_t>(index)];
        const ImVec4 accent = UiTheme::moneyColor(row.metrics.gainLossDollar);
        UiTheme::pushPanelStyle();
        ImGui::PushStyleColor(ImGuiCol_Border, UiTheme::withAlpha(accent, 0.15f));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(12.0f, 10.0f));
        ImGui::BeginChild(("MoverTile" + std::to_string(index)).c_str(), ImVec2(tileWidth, 86.0f), true, ImGuiWindowFlags_NoScrollbar);
        ImGui::PopStyleVar();

        const ImVec2 min = ImGui::GetWindowPos();
        const ImVec2 max(min.x + ImGui::GetWindowSize().x, min.y + ImGui::GetWindowSize().y);
        ImDrawList* drawList = ImGui::GetWindowDrawList();
        drawList->AddRectFilled(min, max, ImGui::GetColorU32(UiTheme::withAlpha(UiTheme::SurfaceMain, 0.46f)), 14.0f);
        drawList->AddRectFilled(ImVec2(min.x + 12.0f, max.y - 9.0f), ImVec2(min.x + 58.0f, max.y - 7.0f), ImGui::GetColorU32(UiTheme::withAlpha(accent, 0.46f)), 2.0f);

        ImGui::TextColored(UiTheme::TextPrimary, "%s", row.ticker.c_str());
        ImGui::SetWindowFontScale(1.08f);
        UiTheme::pushNumericFont();
        ImGui::TextColored(accent, "%s", Money::format(row.metrics.gainLossDollar).c_str());
        UiTheme::popNumericFont();
        ImGui::SetWindowFontScale(1.0f);
        ImGui::TextColored(UiTheme::TextMuted, "%s", Money::formatPercent(row.metrics.gainLossPercent, true).c_str());

        ImGui::EndChild();
        ImGui::PopStyleColor();
        UiTheme::popPanelStyle();
        if (index + 1 < tileCount) {
            ImGui::SameLine();
        }
    }
    ImGui::Spacing();

    UiTheme::pushTableStyle();
    if (ImGui::BeginTable("TopGainersLosersTable", 5, ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerH | ImGuiTableFlags_Resizable, ImVec2(0.0f, 178.0f))) {
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
            UiTheme::textRightAligned(Money::format(row.metrics.marketValue).c_str());
            ImGui::TableSetColumnIndex(3);
            UiTheme::textRightAligned(Money::format(row.metrics.gainLossDollar).c_str(), UiTheme::moneyColor(row.metrics.gainLossDollar));
            ImGui::TableSetColumnIndex(4);
            UiTheme::textRightAligned(Money::formatPercent(row.metrics.gainLossPercent, true).c_str(), UiTheme::moneyColor(row.metrics.gainLossDollar));
        }
        ImGui::EndTable();
    }
    UiTheme::popTableStyle();
    endDashboardPanel();
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
    AppSettingsRepository& settingsRepository,
    DashboardLayoutRepository& layoutRepository,
    DashboardChartSettingsRepository& chartSettingsRepository,
    const std::function<void()>& refreshCurrentPrices,
    const std::function<void()>& reloadData)
{
    const std::string today = Date::todayIso8601();
    const DashboardData data = DashboardService::build(state, today, Date::currentMonthPrefix(), Date::currentYearPrefix());

    const auto toggleCustomize = [this]() {
        customizeMode_ = !customizeMode_;
    };
    const auto createSnapshot = [this, &state, &snapshotRepository, &reloadData](bool replaceExisting) {
        createTodaySnapshot(state, snapshotRepository, reloadData, replaceExisting);
    };
    bool openCapitalGainsGoals = false;
    drawDashboardSettingsMenu(state, layoutRepository, customizeMode_, today, toggleCustomize, refreshCurrentPrices, reloadData, createSnapshot, showSettingsSnapshotReplacePopup_, openCapitalGainsGoals);
    if (openCapitalGainsGoals) {
        openCapitalGainsGoalsEditor(state);
    }
    ImGui::Spacing();

    if (customizeMode_) {
        renderCustomizePanel(state, layoutRepository, reloadData);
        ImGui::Spacing();
    }

    drawTradingGoalsSection(state, today, [this, &state]() {
        openCapitalGainsGoalsEditor(state);
    });
    renderCapitalGainsGoalsEditor(state, settingsRepository);
    ImGui::Spacing();

    const std::vector<DashboardWidget> visibleWidgets = DashboardLayoutService::visibleWidgets(state.dashboardWidgets);
    if (visibleWidgets.empty()) {
        UiTheme::emptyState("No dashboard sections are visible", "Open Customize Dashboard and re-enable the sections you want to review.");
        return;
    }

    if (isWidgetVisible(visibleWidgets, "portfolio_summary")) {
        drawPortfolioSummary(data);
        ImGui::Spacing();
    }

    const float availableWidth = ImGui::GetContentRegionAvail().x;
    const int primaryColumns = availableWidth > 1180.0f ? 2 : 1;
    if (ImGui::BeginTable("DashboardPrimaryWorkspace", primaryColumns, ImGuiTableFlags_SizingStretchProp)) {
        if (primaryColumns == 2) {
            ImGui::TableSetupColumn("Primary", ImGuiTableColumnFlags_WidthStretch, 2.1f);
            ImGui::TableSetupColumn("Watchlist", ImGuiTableColumnFlags_WidthStretch, 0.9f);
        }
        ImGui::TableNextColumn();
        if (isWidgetVisible(visibleWidgets, "performance_panel")) {
            drawPerformanceChartPanel(state, chartSettingsRepository);
            ImGui::Spacing();
        }

        const bool showAllocation = isWidgetVisible(visibleWidgets, "allocation_panel");
        const bool showIncomeGains = isWidgetVisible(visibleWidgets, "income_gains_panel");
        if (showAllocation || showIncomeGains) {
            const int chartColumns = ImGui::GetContentRegionAvail().x > 940.0f && showAllocation && showIncomeGains ? 2 : 1;
            if (ImGui::BeginTable("DashboardSecondaryCharts", chartColumns, ImGuiTableFlags_SizingStretchSame)) {
                if (showAllocation) {
                    ImGui::TableNextColumn();
                    drawAllocationPanel(state, chartSettingsRepository);
                }
                if (showIncomeGains) {
                    ImGui::TableNextColumn();
                    drawIncomeGainsChartPanel(state, chartSettingsRepository);
                }
                ImGui::EndTable();
            }
        }

        ImGui::TableNextColumn();
        drawDashboardWatchlistPanel(state);
        ImGui::Spacing();
        drawTechnicalSignalsPanel(state);
        ImGui::EndTable();
    }

    ImGui::Spacing();
    const int operationsColumns = ImGui::GetContentRegionAvail().x > 1260.0f ? 3 : (ImGui::GetContentRegionAvail().x > 840.0f ? 2 : 1);
    if (ImGui::BeginTable("DashboardOperationsGrid", operationsColumns, ImGuiTableFlags_SizingStretchSame)) {
        if (isWidgetVisible(visibleWidgets, "recent_transactions")) {
            ImGui::TableNextColumn();
            drawRecentTransactionsPanel(state);
        }

        ImGui::TableNextColumn();
        drawAccountStatusPanel(state, data, today, refreshCurrentPrices, reloadData, createSnapshot, isWidgetVisible(visibleWidgets, "snapshot_status"));

        if (isWidgetVisible(visibleWidgets, "performance_movement")) {
            ImGui::TableNextColumn();
            drawPerformanceMovement(data, state);
        }
        if (isWidgetVisible(visibleWidgets, "realized_gains")) {
            ImGui::TableNextColumn();
            drawRealizedGainsPanel(data);
        }
        if (isWidgetVisible(visibleWidgets, "dividend_summary")) {
            ImGui::TableNextColumn();
            drawDividendSummaryPanel(data);
        }
        ImGui::EndTable();
    }

    if (isWidgetVisible(visibleWidgets, "holdings_table")) {
        ImGui::Spacing();
        drawHoldingsTablePanel(state);
    }

    if (isWidgetVisible(visibleWidgets, "top_gainers_losers")) {
        ImGui::Spacing();
        drawTopGainersLosersPanel(state);
    }

    bool hasAdditionalWidgets = false;
    for (const DashboardWidget& widget : visibleWidgets) {
        if (!isPromotedDashboardWidget(widget.widgetKey)) {
            hasAdditionalWidgets = true;
            break;
        }
    }

    if (!hasAdditionalWidgets) {
        return;
    }

    ImGui::Spacing();
    const int additionalColumns = ImGui::GetContentRegionAvail().x > 1220.0f ? 2 : 1;
    if (ImGui::BeginTable("DashboardAdditionalWidgets", additionalColumns, ImGuiTableFlags_SizingStretchSame)) {
        for (const DashboardWidget& widget : visibleWidgets) {
            if (isPromotedDashboardWidget(widget.widgetKey)) {
                continue;
            }
            ImGui::TableNextColumn();
            drawDashboardWidget(widget, state, snapshotRepository, chartSettingsRepository, data, today, reloadData, createSnapshot);
        }
        ImGui::EndTable();
    }
}

void DashboardView::openCapitalGainsGoalsEditor(const AppState& state)
{
    weeklyCapitalGainsGoalDraft_ = formatCurrencyDraft(state.weeklyRealizedCapitalGainsGoal);
    monthlyCapitalGainsGoalDraft_ = formatCurrencyDraft(state.monthlyRealizedCapitalGainsGoal);
    capitalGainsGoalsMessage_.clear();
    capitalGainsGoalsMessageIsError_ = false;
    showCapitalGainsGoalsPopup_ = true;
}

void DashboardView::renderCapitalGainsGoalsEditor(AppState& state, AppSettingsRepository& settingsRepository)
{
    constexpr const char* PopupId = "Capital Gains Goals";

    if (showCapitalGainsGoalsPopup_) {
        ImGui::OpenPopup(PopupId);
        showCapitalGainsGoalsPopup_ = false;
    }

    ImGui::SetNextWindowSize(ImVec2(430.0f, 0.0f), ImGuiCond_Appearing);
    if (!ImGui::BeginPopupModal(PopupId, nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        return;
    }

    ImGui::TextColored(UiTheme::TextSecondary, "Realized capital gains goals");
    ImGui::Separator();
    ImGui::InputText("Weekly Capital Gains Goal", &weeklyCapitalGainsGoalDraft_);
    ImGui::InputText("Monthly Capital Gains Goal", &monthlyCapitalGainsGoalDraft_);

    const std::optional<double> weeklyGoal = parseNonNegativeCurrencyDraft(weeklyCapitalGainsGoalDraft_);
    const std::optional<double> monthlyGoal = parseNonNegativeCurrencyDraft(monthlyCapitalGainsGoalDraft_);
    const std::string activeWeeklyGoal = formatCurrencyDraft(state.weeklyRealizedCapitalGainsGoal);
    const std::string activeMonthlyGoal = formatCurrencyDraft(state.monthlyRealizedCapitalGainsGoal);
    const bool draftValid = weeklyGoal.has_value() && monthlyGoal.has_value();
    const bool hasDraftTextChanges = weeklyCapitalGainsGoalDraft_ != activeWeeklyGoal ||
        monthlyCapitalGainsGoalDraft_ != activeMonthlyGoal;
    const bool hasChanges = draftValid &&
        (std::fabs(*weeklyGoal - state.weeklyRealizedCapitalGainsGoal) >= 0.005 ||
            std::fabs(*monthlyGoal - state.monthlyRealizedCapitalGainsGoal) >= 0.005);

    if (!draftValid) {
        ImGui::TextColored(UiTheme::Loss, "Enter non-negative dollar amounts.");
    } else {
        ImGui::TextColored(UiTheme::MutedText, "Empty values save as 0.00.");
    }

    if (!capitalGainsGoalsMessage_.empty()) {
        ImGui::TextColored(capitalGainsGoalsMessageIsError_ ? UiTheme::Loss : UiTheme::Gain, "%s", capitalGainsGoalsMessage_.c_str());
    }

    ImGui::Spacing();
    if (!draftValid || !hasChanges) {
        ImGui::BeginDisabled();
    }
    if (ImGui::Button("Save Goals", ImVec2(118.0f, 0.0f))) {
        const std::string weeklyValue = formatCurrencyDraft(*weeklyGoal);
        const std::string monthlyValue = formatCurrencyDraft(*monthlyGoal);
        std::string error;
        if (settingsRepository.setString(AppSettingKeys::WeeklyRealizedCapitalGainsGoal, weeklyValue, error) &&
            settingsRepository.setString(AppSettingKeys::MonthlyRealizedCapitalGainsGoal, monthlyValue, error)) {
            state.weeklyRealizedCapitalGainsGoal = parseNonNegativeCurrencyDraft(weeklyValue).value_or(0.0);
            state.monthlyRealizedCapitalGainsGoal = parseNonNegativeCurrencyDraft(monthlyValue).value_or(0.0);
            weeklyCapitalGainsGoalDraft_ = weeklyValue;
            monthlyCapitalGainsGoalDraft_ = monthlyValue;
            capitalGainsGoalsMessage_ = "Capital gains goals saved.";
            capitalGainsGoalsMessageIsError_ = false;
            state.setStatus("Capital gains goals saved locally.");
        } else {
            capitalGainsGoalsMessage_ = "Could not save capital gains goals: " + error;
            capitalGainsGoalsMessageIsError_ = true;
        }
    }
    if (!draftValid || !hasChanges) {
        ImGui::EndDisabled();
    }

    ImGui::SameLine();
    if (!hasDraftTextChanges) {
        ImGui::BeginDisabled();
    }
    if (ImGui::Button("Cancel", ImVec2(94.0f, 0.0f))) {
        weeklyCapitalGainsGoalDraft_ = activeWeeklyGoal;
        monthlyCapitalGainsGoalDraft_ = activeMonthlyGoal;
        capitalGainsGoalsMessage_ = "Changes discarded.";
        capitalGainsGoalsMessageIsError_ = false;
    }
    if (!hasDraftTextChanges) {
        ImGui::EndDisabled();
    }

    ImGui::SameLine();
    if (ImGui::Button("Close", ImVec2(94.0f, 0.0f))) {
        ImGui::CloseCurrentPopup();
    }

    ImGui::EndPopup();
}

void DashboardView::renderCustomizePanel(AppState& state, DashboardLayoutRepository& layoutRepository, const std::function<void()>& reloadData)
{
    beginDashboardPanel("DashboardCustomizePanel", "Customize Dashboard", ImVec2(0.0f, 458.0f), UiTheme::NeonMagenta);
    ImGui::TextWrapped("Rearrange your dashboard so it matches the way you review your portfolio.");
    ImGui::TextColored(UiTheme::MutedText, "Dashboard layout is stored locally.");
    ImGui::Spacing();

    UiTheme::pushButtonStyle(UiTheme::ElectricCyan);
    if (ImGui::Button("Reset Dashboard Layout")) {
        std::string error;
        if (layoutRepository.resetToDefaults(error)) {
            reloadData();
            state.setStatus("Dashboard layout reset.");
        } else {
            state.setStatus("Could not reset dashboard layout: " + error, true);
        }
    }
    UiTheme::popButtonStyle();

    ImGui::Spacing();
    std::vector<DashboardWidget> widgets = DashboardLayoutService::orderedWidgets(state.dashboardWidgets);
    UiTheme::pushTableStyle();
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
    UiTheme::popTableStyle();

    endDashboardPanel();
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
