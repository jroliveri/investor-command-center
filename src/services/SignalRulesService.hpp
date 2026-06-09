// SPDX-License-Identifier: MIT
#pragma once

#include "models/SignalRules.hpp"

#include <string>

class AppSettingsRepository;

namespace SignalRulesService {
SignalRules defaults();
bool validate(const SignalRules& rules, std::string& error);
SignalRules load(const AppSettingsRepository& settingsRepository, std::string& error);
bool save(const AppSettingsRepository& settingsRepository, const SignalRules& rules, std::string& error);
bool resetToDefaults(const AppSettingsRepository& settingsRepository, std::string& error);
}
