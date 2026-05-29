// SPDX-License-Identifier: MIT
#include "services/DashboardLayoutService.hpp"

#include <algorithm>

namespace {

const std::vector<DashboardWidgetDefinition> Defaults = {
    { "portfolio_summary", "Portfolio Summary", true },
    { "performance_movement", "Daily/Monthly/Yearly Movement", true },
    { "holdings_table", "Holdings Table", true },
    { "realized_gains", "Realized Gains", true },
    { "dividend_summary", "Dividend Summary", true },
    { "recent_transactions", "Recent Transactions", true },
    { "snapshot_status", "Snapshot Status", true },
    { "allocation_panel", "Allocation", true },
    { "performance_panel", "Performance Chart", true },
    { "income_gains_panel", "Income / Gains Chart", true },
    { "top_gainers_losers", "Top Gainers/Losers", true },
    { "holdings_value", "Holdings Value", false },
    { "cash_balance", "Cash Balance", false },
    { "cost_basis", "Cost Basis", false },
    { "unrealized_gain_loss", "Unrealized Gain/Loss", false },
};

bool compareWidgets(const DashboardWidget& left, const DashboardWidget& right)
{
    if (left.sortOrder == right.sortOrder) {
        return left.widgetKey < right.widgetKey;
    }
    return left.sortOrder < right.sortOrder;
}

}

namespace DashboardLayoutService {

const std::vector<DashboardWidgetDefinition>& defaultDefinitions()
{
    return Defaults;
}

std::vector<DashboardWidget> defaultWidgets()
{
    std::vector<DashboardWidget> widgets;
    widgets.reserve(Defaults.size());

    int sortOrder = 1;
    for (const DashboardWidgetDefinition& definition : Defaults) {
        DashboardWidget widget;
        widget.widgetKey = definition.widgetKey;
        widget.displayName = definition.displayName;
        widget.sortOrder = sortOrder++;
        widget.isVisible = definition.isVisibleByDefault;
        widgets.push_back(widget);
    }

    return widgets;
}

std::vector<DashboardWidget> orderedWidgets(std::vector<DashboardWidget> widgets)
{
    if (widgets.empty()) {
        widgets = defaultWidgets();
    }

    widgets.erase(
        std::remove_if(widgets.begin(), widgets.end(), [](const DashboardWidget& widget) {
            return !isKnownWidgetKey(widget.widgetKey);
        }),
        widgets.end());
    std::sort(widgets.begin(), widgets.end(), compareWidgets);
    return widgets;
}

std::vector<DashboardWidget> visibleWidgets(const std::vector<DashboardWidget>& widgets)
{
    std::vector<DashboardWidget> ordered = orderedWidgets(widgets);
    ordered.erase(
        std::remove_if(ordered.begin(), ordered.end(), [](const DashboardWidget& widget) {
            return !widget.isVisible;
        }),
        ordered.end());
    return ordered;
}

void normalizeSortOrder(std::vector<DashboardWidget>& widgets)
{
    std::sort(widgets.begin(), widgets.end(), compareWidgets);

    int sortOrder = 1;
    for (DashboardWidget& widget : widgets) {
        widget.sortOrder = sortOrder++;
    }
}

bool moveWidget(std::vector<DashboardWidget>& widgets, const std::string& widgetKey, int direction)
{
    if (direction == 0) {
        return false;
    }

    normalizeSortOrder(widgets);

    const auto iterator = std::find_if(widgets.begin(), widgets.end(), [&widgetKey](const DashboardWidget& widget) {
        return widget.widgetKey == widgetKey;
    });
    if (iterator == widgets.end()) {
        return false;
    }

    const int index = static_cast<int>(std::distance(widgets.begin(), iterator));
    const int targetIndex = direction < 0 ? index - 1 : index + 1;
    if (targetIndex < 0 || targetIndex >= static_cast<int>(widgets.size())) {
        return false;
    }

    std::swap(widgets[static_cast<std::size_t>(index)], widgets[static_cast<std::size_t>(targetIndex)]);
    int sortOrder = 1;
    for (DashboardWidget& widget : widgets) {
        widget.sortOrder = sortOrder++;
    }
    return true;
}

bool isKnownWidgetKey(const std::string& widgetKey)
{
    return std::any_of(Defaults.begin(), Defaults.end(), [&widgetKey](const DashboardWidgetDefinition& definition) {
        return widgetKey == definition.widgetKey;
    });
}

}
