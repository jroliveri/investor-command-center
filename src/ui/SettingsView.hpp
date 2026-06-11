// SPDX-License-Identifier: MIT
#pragma once

#include "models/CapitalGainAllocationRule.hpp"
#include "models/TechnicalIndicatorSettings.hpp"
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
        const std::string& activeDatabasePath,
        const char* appVersion,
        const std::function<bool(const std::string&, std::string&)>& saveDatabasePath,
        const std::function<void()>& reloadData);

private:
    void renderDatabaseLocation(const std::string& activeDatabasePath, const std::function<bool(const std::string&, std::string&)>& saveDatabasePath);
    void renderTechnicalIndicatorSettings(AppState& state, AppSettingsRepository& settingsRepository);
    void renderCapitalGainAllocation(AppState& state, CapitalGainAllocationRepository& allocationRepository, const std::function<void()>& reloadData);

    TechnicalIndicatorSettings technicalSettingsDraft_;
    bool technicalSettingsDraftInitialized_ = false;
    bool technicalSettingsDraftDirty_ = false;
    std::string technicalSettingsMessage_;
    bool technicalSettingsMessageIsError_ = false;
    std::string allocationMessage_;
    bool allocationMessageIsError_ = false;
    std::string databasePathDraft_;
    bool databasePathDraftInitialized_ = false;
    std::string databaseLocationMessage_;
    bool databaseLocationMessageIsError_ = false;
};
