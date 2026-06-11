// SPDX-License-Identifier: MIT
#pragma once

#include "models/TechnicalIndicatorSettings.hpp"

#include <string>

class AppSettingsRepository;

namespace TechnicalIndicatorSettingsService {
TechnicalIndicatorSettings defaults();
bool validate(const TechnicalIndicatorSettings& settings, std::string& error);
TechnicalIndicatorSettings load(const AppSettingsRepository& settingsRepository, std::string& error);
bool save(const AppSettingsRepository& settingsRepository, const TechnicalIndicatorSettings& settings, std::string& error);
bool resetToDefaults(const AppSettingsRepository& settingsRepository, std::string& error);
}
