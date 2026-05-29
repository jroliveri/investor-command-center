// SPDX-License-Identifier: MIT
#pragma once

#include "models/DashboardWidget.hpp"

#include <string>
#include <vector>

struct DashboardWidgetDefinition {
    const char* widgetKey;
    const char* displayName;
    bool isVisibleByDefault;
};

namespace DashboardLayoutService {
const std::vector<DashboardWidgetDefinition>& defaultDefinitions();
std::vector<DashboardWidget> defaultWidgets();
std::vector<DashboardWidget> orderedWidgets(std::vector<DashboardWidget> widgets);
std::vector<DashboardWidget> visibleWidgets(const std::vector<DashboardWidget>& widgets);
void normalizeSortOrder(std::vector<DashboardWidget>& widgets);
bool moveWidget(std::vector<DashboardWidget>& widgets, const std::string& widgetKey, int direction);
bool isKnownWidgetKey(const std::string& widgetKey);
}
