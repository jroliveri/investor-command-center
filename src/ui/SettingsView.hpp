// SPDX-License-Identifier: MIT
#pragma once

#include "models/CapitalGainAllocationRule.hpp"

#include <functional>
#include <string>

class AppSettingsRepository;
class CapitalGainAllocationRepository;
class AppState;

class SettingsView {
public:
    void render(AppState& state,
        AppSettingsRepository& settingsRepository,
        CapitalGainAllocationRepository& allocationRepository,
        const std::string& databasePath,
        const char* appVersion,
        const std::function<void()>& backupNow,
        const std::function<void()>& reloadData);

private:
    void renderDatabaseBackup(AppState& state, AppSettingsRepository& settingsRepository, const std::function<void()>& backupNow);
    void renderCapitalGainAllocation(AppState& state, CapitalGainAllocationRepository& allocationRepository, const std::function<void()>& reloadData);

    std::string allocationMessage_;
    bool allocationMessageIsError_ = false;
    std::string backupMessage_;
    bool backupMessageIsError_ = false;
};
