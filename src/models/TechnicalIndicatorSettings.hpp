// SPDX-License-Identifier: MIT
#pragma once

namespace TechnicalIndicatorSettingsDefaults {
inline constexpr int RsiPeriod = 14;
inline constexpr double RsiOversoldThreshold = 30.0;
inline constexpr double RsiOverboughtThreshold = 70.0;
inline constexpr int MacdFastPeriod = 12;
inline constexpr int MacdSlowPeriod = 26;
inline constexpr int MacdSignalPeriod = 9;
inline constexpr int MomentumLookbackDays = 7;
inline constexpr double MomentumPositiveThresholdPercent = 0.5;
inline constexpr double MomentumNegativeThresholdPercent = -0.5;
inline constexpr bool ShowExtraTechnicals = true;
}

struct TechnicalIndicatorSettings {
    int rsiPeriod = TechnicalIndicatorSettingsDefaults::RsiPeriod;
    double rsiOversoldThreshold = TechnicalIndicatorSettingsDefaults::RsiOversoldThreshold;
    double rsiOverboughtThreshold = TechnicalIndicatorSettingsDefaults::RsiOverboughtThreshold;
    int macdFastPeriod = TechnicalIndicatorSettingsDefaults::MacdFastPeriod;
    int macdSlowPeriod = TechnicalIndicatorSettingsDefaults::MacdSlowPeriod;
    int macdSignalPeriod = TechnicalIndicatorSettingsDefaults::MacdSignalPeriod;
    int momentumLookbackDays = TechnicalIndicatorSettingsDefaults::MomentumLookbackDays;
    double momentumPositiveThresholdPercent = TechnicalIndicatorSettingsDefaults::MomentumPositiveThresholdPercent;
    double momentumNegativeThresholdPercent = TechnicalIndicatorSettingsDefaults::MomentumNegativeThresholdPercent;
    bool showExtraTechnicals = TechnicalIndicatorSettingsDefaults::ShowExtraTechnicals;

    bool operator==(const TechnicalIndicatorSettings&) const = default;
};
