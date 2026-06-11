// SPDX-License-Identifier: MIT
#include "services/TechnicalIndicatorSettingsService.hpp"

#include "repositories/AppSettingsRepository.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <iomanip>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>

namespace {

constexpr const char* RsiPeriodSettingKey = "watchlist.technical.rsi_period";
constexpr const char* RsiOversoldThresholdSettingKey = "watchlist.technical.rsi_oversold_threshold";
constexpr const char* RsiOverboughtThresholdSettingKey = "watchlist.technical.rsi_overbought_threshold";
constexpr const char* MacdFastPeriodSettingKey = "watchlist.technical.macd_fast_period";
constexpr const char* MacdSlowPeriodSettingKey = "watchlist.technical.macd_slow_period";
constexpr const char* MacdSignalPeriodSettingKey = "watchlist.technical.macd_signal_period";
constexpr const char* MomentumLookbackDaysSettingKey = "watchlist.technical.momentum_lookback_days";
constexpr const char* MomentumPositiveThresholdSettingKey = "watchlist.technical.momentum_positive_threshold_percent";
constexpr const char* MomentumNegativeThresholdSettingKey = "watchlist.technical.momentum_negative_threshold_percent";
constexpr const char* ShowExtraTechnicalsSettingKey = "watchlist.technical.show_extra_technicals";

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

std::string serializeBool(bool value)
{
    return value ? "1" : "0";
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

std::string lowerCopy(std::string value)
{
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char character) {
        return static_cast<char>(std::tolower(character));
    });
    return value;
}

std::optional<bool> parseBool(const std::string& raw)
{
    const std::string value = lowerCopy(raw);
    if (value == "1" || value == "true" || value == "yes" || value == "on") {
        return true;
    }
    if (value == "0" || value == "false" || value == "no" || value == "off") {
        return false;
    }
    return std::nullopt;
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
        error = std::string("Technical indicator setting ") + key + " is invalid. Defaults are active.";
        return false;
    }

    value = *parsed;
    return true;
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
        error = std::string("Technical indicator setting ") + key + " is invalid. Defaults are active.";
        return false;
    }

    value = *parsed;
    return true;
}

bool readBoolSetting(
    const AppSettingsRepository& settingsRepository,
    const char* key,
    bool fallback,
    bool& value,
    std::string& error)
{
    std::string repositoryError;
    const std::string raw = settingsRepository.getString(key, serializeBool(fallback), repositoryError);
    if (!repositoryError.empty()) {
        error = repositoryError;
        return false;
    }

    const std::optional<bool> parsed = parseBool(raw);
    if (!parsed.has_value()) {
        error = std::string("Technical indicator setting ") + key + " is invalid. Defaults are active.";
        return false;
    }

    value = *parsed;
    return true;
}

} // namespace

TechnicalIndicatorSettings TechnicalIndicatorSettingsService::defaults()
{
    return TechnicalIndicatorSettings {};
}

bool TechnicalIndicatorSettingsService::validate(const TechnicalIndicatorSettings& settings, std::string& error)
{
    error.clear();

    if (settings.rsiPeriod <= 1) {
        error = "RSI period must be greater than 1.";
        return false;
    }
    if (!std::isfinite(settings.rsiOversoldThreshold) || !std::isfinite(settings.rsiOverboughtThreshold)) {
        error = "RSI thresholds must be numeric values.";
        return false;
    }
    if (settings.rsiOversoldThreshold < 0.0 || settings.rsiOversoldThreshold > 100.0 ||
        settings.rsiOverboughtThreshold < 0.0 || settings.rsiOverboughtThreshold > 100.0) {
        error = "RSI thresholds must be between 0 and 100.";
        return false;
    }
    if (settings.rsiOversoldThreshold >= settings.rsiOverboughtThreshold) {
        error = "RSI oversold threshold must be less than the overbought threshold.";
        return false;
    }
    if (settings.macdFastPeriod <= 1) {
        error = "MACD fast EMA period must be greater than 1.";
        return false;
    }
    if (settings.macdSlowPeriod <= settings.macdFastPeriod) {
        error = "MACD slow EMA period must be greater than the fast EMA period.";
        return false;
    }
    if (settings.macdSignalPeriod <= 1) {
        error = "MACD signal EMA period must be greater than 1.";
        return false;
    }
    if (settings.momentumLookbackDays <= 0) {
        error = "Momentum lookback days must be greater than 0.";
        return false;
    }
    if (!std::isfinite(settings.momentumPositiveThresholdPercent) || !std::isfinite(settings.momentumNegativeThresholdPercent)) {
        error = "Momentum thresholds must be numeric percentage values.";
        return false;
    }

    return true;
}

TechnicalIndicatorSettings TechnicalIndicatorSettingsService::load(const AppSettingsRepository& settingsRepository, std::string& error)
{
    error.clear();
    TechnicalIndicatorSettings settings = defaults();

    if (!readIntSetting(settingsRepository, RsiPeriodSettingKey, TechnicalIndicatorSettingsDefaults::RsiPeriod, settings.rsiPeriod, error) ||
        !readDoubleSetting(settingsRepository, RsiOversoldThresholdSettingKey, TechnicalIndicatorSettingsDefaults::RsiOversoldThreshold, 2, settings.rsiOversoldThreshold, error) ||
        !readDoubleSetting(settingsRepository, RsiOverboughtThresholdSettingKey, TechnicalIndicatorSettingsDefaults::RsiOverboughtThreshold, 2, settings.rsiOverboughtThreshold, error) ||
        !readIntSetting(settingsRepository, MacdFastPeriodSettingKey, TechnicalIndicatorSettingsDefaults::MacdFastPeriod, settings.macdFastPeriod, error) ||
        !readIntSetting(settingsRepository, MacdSlowPeriodSettingKey, TechnicalIndicatorSettingsDefaults::MacdSlowPeriod, settings.macdSlowPeriod, error) ||
        !readIntSetting(settingsRepository, MacdSignalPeriodSettingKey, TechnicalIndicatorSettingsDefaults::MacdSignalPeriod, settings.macdSignalPeriod, error) ||
        !readIntSetting(settingsRepository, MomentumLookbackDaysSettingKey, TechnicalIndicatorSettingsDefaults::MomentumLookbackDays, settings.momentumLookbackDays, error) ||
        !readDoubleSetting(settingsRepository, MomentumPositiveThresholdSettingKey, TechnicalIndicatorSettingsDefaults::MomentumPositiveThresholdPercent, 2, settings.momentumPositiveThresholdPercent, error) ||
        !readDoubleSetting(settingsRepository, MomentumNegativeThresholdSettingKey, TechnicalIndicatorSettingsDefaults::MomentumNegativeThresholdPercent, 2, settings.momentumNegativeThresholdPercent, error) ||
        !readBoolSetting(settingsRepository, ShowExtraTechnicalsSettingKey, TechnicalIndicatorSettingsDefaults::ShowExtraTechnicals, settings.showExtraTechnicals, error)) {
        return defaults();
    }

    std::string validationError;
    if (!validate(settings, validationError)) {
        error = validationError + " Defaults are active.";
        return defaults();
    }

    return settings;
}

bool TechnicalIndicatorSettingsService::save(const AppSettingsRepository& settingsRepository, const TechnicalIndicatorSettings& settings, std::string& error)
{
    error.clear();

    if (!validate(settings, error)) {
        return false;
    }

    if (!settingsRepository.setString(RsiPeriodSettingKey, serializeInt(settings.rsiPeriod), error)) {
        return false;
    }
    if (!settingsRepository.setString(RsiOversoldThresholdSettingKey, serializeDouble(settings.rsiOversoldThreshold, 2), error)) {
        return false;
    }
    if (!settingsRepository.setString(RsiOverboughtThresholdSettingKey, serializeDouble(settings.rsiOverboughtThreshold, 2), error)) {
        return false;
    }
    if (!settingsRepository.setString(MacdFastPeriodSettingKey, serializeInt(settings.macdFastPeriod), error)) {
        return false;
    }
    if (!settingsRepository.setString(MacdSlowPeriodSettingKey, serializeInt(settings.macdSlowPeriod), error)) {
        return false;
    }
    if (!settingsRepository.setString(MacdSignalPeriodSettingKey, serializeInt(settings.macdSignalPeriod), error)) {
        return false;
    }
    if (!settingsRepository.setString(MomentumLookbackDaysSettingKey, serializeInt(settings.momentumLookbackDays), error)) {
        return false;
    }
    if (!settingsRepository.setString(MomentumPositiveThresholdSettingKey, serializeDouble(settings.momentumPositiveThresholdPercent, 2), error)) {
        return false;
    }
    if (!settingsRepository.setString(MomentumNegativeThresholdSettingKey, serializeDouble(settings.momentumNegativeThresholdPercent, 2), error)) {
        return false;
    }
    if (!settingsRepository.setString(ShowExtraTechnicalsSettingKey, serializeBool(settings.showExtraTechnicals), error)) {
        return false;
    }

    return true;
}

bool TechnicalIndicatorSettingsService::resetToDefaults(const AppSettingsRepository& settingsRepository, std::string& error)
{
    return save(settingsRepository, defaults(), error);
}
