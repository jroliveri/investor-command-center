// SPDX-License-Identifier: MIT
#pragma once

#include "models/CapitalGainAllocationRule.hpp"
#include "models/SignalRules.hpp"

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
        const std::function<void()>& reloadData);

private:
<<<<<<< Updated upstream
=======
    void renderDatabaseLocation(const std::string& activeDatabasePath, const std::function<bool(const std::string&, std::string&)>& moveDatabaseToFolder);
    void renderDatabaseBackup(AppState& state, AppSettingsRepository& settingsRepository, const std::function<void()>& backupNow);
    void renderSignalRules(AppState& state, AppSettingsRepository& settingsRepository);
>>>>>>> Stashed changes
    void renderCapitalGainAllocation(AppState& state, CapitalGainAllocationRepository& allocationRepository, const std::function<void()>& reloadData);

    SignalRules signalRulesDraft_;
    bool signalRulesDraftInitialized_ = false;
    bool signalRulesDraftDirty_ = false;
    std::string signalRulesMessage_;
    bool signalRulesMessageIsError_ = false;
    std::string allocationMessage_;
    bool allocationMessageIsError_ = false;
};
