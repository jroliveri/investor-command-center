// SPDX-License-Identifier: MIT
#include "services/SignalRulesService.hpp"

#include "repositories/AppSettingsRepository.hpp"

#include <cmath>
#include <iomanip>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>

namespace {

constexpr const char* RsiBuyMinSettingKey = "signal_rules.rsi_buy_min";
constexpr const char* RsiBuyMaxSettingKey = "signal_rules.rsi_buy_max";
constexpr const char* MacdHistogramMinSettingKey = "signal_rules.macd_histogram_min";
constexpr const char* MomentumLookbackSessionsSettingKey = "signal_rules.momentum_lookback_sessions";

std::string serializeDouble(double value, int decimals)
{
    std::ostringstream stream;
    stream << std::fixed << std::setprecision(decimals) << value;
    return stream.str();
}

std::string serializeInt(int value)
{
    return std::to_string(value);
}

std::optional<double> parseDouble(const std::string& raw)
{
    try {
        std::size_t consumed = 0;
        const double value = std::stod(raw, &consumed);
        if (consumed != raw.size() || !std::isfinite(value)) {
            return std::nullopt;
        }
        return value;
    } catch (const std::invalid_argument&) {
        return std::nullopt;
    } catch (const std::out_of_range&) {
        return std::nullopt;
    }
}

std::optional<int> parseInt(const std::string& raw)
{
    try {
        std::size_t consumed = 0;
        const int value = std::stoi(raw, &consumed);
        if (consumed != raw.size()) {
            return std::nullopt;
        }
        return value;
    } catch (const std::invalid_argument&) {
        return std::nullopt;
    } catch (const std::out_of_range&) {
        return std::nullopt;
    }
}

bool readDoubleSetting(
    const AppSettingsRepository& settingsRepository,
    const char* key,
    double fallback,
    int decimals,
    double& value,
    std::string& error)
{
    std::string repositoryError;
    const std::string raw = settingsRepository.getString(key, serializeDouble(fallback, decimals), repositoryError);
    if (!repositoryError.empty()) {
        error = repositoryError;
        return false;
    }

    const std::optional<double> parsed = parseDouble(raw);
    if (!parsed.has_value()) {
        error = std::string("Signal rules setting ") + key + " is invalid. Defaults are active.";
        return false;
    }

    value = *parsed;
    return true;
}

bool readIntSetting(
    const AppSettingsRepository& settingsRepository,
    const char* key,
    int fallback,
    int& value,
    std::string& error)
{
    std::string repositoryError;
    const std::string raw = settingsRepository.getString(key, serializeInt(fallback), repositoryError);
    if (!repositoryError.empty()) {
        error = repositoryError;
        return false;
    }

    const std::optional<int> parsed = parseInt(raw);
    if (!parsed.has_value()) {
        error = std::string("Signal rules setting ") + key + " is invalid. Defaults are active.";
        return false;
    }

    value = *parsed;
    return true;
}

} // namespace

SignalRules SignalRulesService::defaults()
{
    return SignalRules {};
}

bool SignalRulesService::validate(const SignalRules& rules, std::string& error)
{
    error.clear();

    if (!std::isfinite(rules.rsiBuyMin) || !std::isfinite(rules.rsiBuyMax)) {
        error = "RSI thresholds must be numeric values.";
        return false;
    }
    if (rules.rsiBuyMin < 0.0 || rules.rsiBuyMin > 100.0 || rules.rsiBuyMax < 0.0 || rules.rsiBuyMax > 100.0) {
        error = "RSI thresholds must be between 0 and 100.";
        return false;
    }
    if (rules.rsiBuyMax <= rules.rsiBuyMin) {
        error = "RSI upper threshold must be greater than the lower threshold.";
        return false;
    }
    if (!std::isfinite(rules.macdHistogramMin)) {
        error = "MACD histogram threshold must be numeric.";
        return false;
    }
    if (rules.momentumLookbackSessions <= 0) {
        error = "Momentum lookback must be a positive whole number of trading sessions.";
        return false;
    }

    return true;
}

SignalRules SignalRulesService::load(const AppSettingsRepository& settingsRepository, std::string& error)
{
    error.clear();
    SignalRules rules = defaults();

    if (!readDoubleSetting(settingsRepository, RsiBuyMinSettingKey, SignalRulesDefaults::RsiBuyMin, 1, rules.rsiBuyMin, error) ||
        !readDoubleSetting(settingsRepository, RsiBuyMaxSettingKey, SignalRulesDefaults::RsiBuyMax, 1, rules.rsiBuyMax, error) ||
        !readDoubleSetting(settingsRepository, MacdHistogramMinSettingKey, SignalRulesDefaults::MacdHistogramMin, 4, rules.macdHistogramMin, error) ||
        !readIntSetting(settingsRepository, MomentumLookbackSessionsSettingKey, SignalRulesDefaults::MomentumLookbackSessions, rules.momentumLookbackSessions, error)) {
        return defaults();
    }

    std::string validationError;
    if (!validate(rules, validationError)) {
        error = validationError + " Defaults are active.";
        return defaults();
    }

    return rules;
}

bool SignalRulesService::save(const AppSettingsRepository& settingsRepository, const SignalRules& rules, std::string& error)
{
    error.clear();

    if (!validate(rules, error)) {
        return false;
    }

    if (!settingsRepository.setString(RsiBuyMinSettingKey, serializeDouble(rules.rsiBuyMin, 1), error)) {
        return false;
    }
    if (!settingsRepository.setString(RsiBuyMaxSettingKey, serializeDouble(rules.rsiBuyMax, 1), error)) {
        return false;
    }
    if (!settingsRepository.setString(MacdHistogramMinSettingKey, serializeDouble(rules.macdHistogramMin, 4), error)) {
        return false;
    }
    if (!settingsRepository.setString(MomentumLookbackSessionsSettingKey, serializeInt(rules.momentumLookbackSessions), error)) {
        return false;
    }

    return true;
}

bool SignalRulesService::resetToDefaults(const AppSettingsRepository& settingsRepository, std::string& error)
{
    return save(settingsRepository, defaults(), error);
}
