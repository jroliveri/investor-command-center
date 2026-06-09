// SPDX-License-Identifier: MIT
#pragma once

namespace SignalRulesDefaults {
inline constexpr double RsiBuyMin = 40.0;
inline constexpr double RsiBuyMax = 60.0;
inline constexpr double MacdHistogramMin = 0.0;
inline constexpr int MomentumLookbackSessions = 7;
}

struct SignalRules {
    double rsiBuyMin = SignalRulesDefaults::RsiBuyMin;
    double rsiBuyMax = SignalRulesDefaults::RsiBuyMax;
    double macdHistogramMin = SignalRulesDefaults::MacdHistogramMin;
    int momentumLookbackSessions = SignalRulesDefaults::MomentumLookbackSessions;

    bool operator==(const SignalRules&) const = default;
};
