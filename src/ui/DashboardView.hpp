// SPDX-License-Identifier: MIT
#pragma once

#include <functional>

class AppState;
class DashboardChartSettingsRepository;
class DashboardLayoutRepository;
class PortfolioSnapshotRepository;

class DashboardView {
public:
    void render(AppState& state,
        PortfolioSnapshotRepository& snapshotRepository,
        DashboardLayoutRepository& layoutRepository,
        DashboardChartSettingsRepository& chartSettingsRepository,
        const std::function<void()>& reloadData);
    void setCustomizeMode(bool enabled) { customizeMode_ = enabled; }

private:
    void renderCustomizePanel(AppState& state, DashboardLayoutRepository& layoutRepository, const std::function<void()>& reloadData);
    void createTodaySnapshot(AppState& state, PortfolioSnapshotRepository& snapshotRepository, const std::function<void()>& reloadData, bool replaceExisting);

    bool customizeMode_ = false;
};
