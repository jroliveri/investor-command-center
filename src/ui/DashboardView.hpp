// SPDX-License-Identifier: MIT
#pragma once

#include <functional>
#include <string>

class AppState;
class AppSettingsRepository;
class DashboardChartSettingsRepository;
class DashboardLayoutRepository;
class PortfolioSnapshotRepository;

class DashboardView {
public:
    void render(AppState& state,
        PortfolioSnapshotRepository& snapshotRepository,
        AppSettingsRepository& settingsRepository,
        DashboardLayoutRepository& layoutRepository,
        DashboardChartSettingsRepository& chartSettingsRepository,
        const std::function<void()>& refreshCurrentPrices,
        const std::function<void()>& reloadData);
    void setCustomizeMode(bool enabled) { customizeMode_ = enabled; }

private:
    void openCapitalGainsGoalsEditor(const AppState& state);
    void renderCapitalGainsGoalsEditor(AppState& state, AppSettingsRepository& settingsRepository);
    void renderCustomizePanel(AppState& state, DashboardLayoutRepository& layoutRepository, const std::function<void()>& reloadData);
    void createTodaySnapshot(AppState& state, PortfolioSnapshotRepository& snapshotRepository, const std::function<void()>& reloadData, bool replaceExisting);

    bool customizeMode_ = false;
    bool showSettingsSnapshotReplacePopup_ = false;
    bool showCapitalGainsGoalsPopup_ = false;
    std::string weeklyCapitalGainsGoalDraft_;
    std::string monthlyCapitalGainsGoalDraft_;
    std::string capitalGainsGoalsMessage_;
    bool capitalGainsGoalsMessageIsError_ = false;
};
