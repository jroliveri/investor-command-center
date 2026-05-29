// SPDX-License-Identifier: MIT
#include "services/PerformanceCalculator.hpp"

#include <algorithm>
#include <cctype>

namespace {

std::string lowerCopy(std::string value)
{
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char character) {
        return static_cast<char>(std::tolower(character));
    });
    return value;
}

bool isSell(const Transaction& transaction)
{
    return lowerCopy(transaction.transactionType) == "sell";
}

bool startsWith(const std::string& value, const std::string& prefix)
{
    return value.rfind(prefix, 0) == 0;
}

}

namespace PerformanceCalculator {

RealizedGainSummary calculateRealizedGains(
    const std::vector<Transaction>& transactions,
    const std::string& today,
    const std::string& monthPrefix,
    const std::string& yearPrefix)
{
    RealizedGainSummary summary;

    for (const Transaction& transaction : transactions) {
        if (!isSell(transaction)) {
            continue;
        }

        ++summary.sellCount;
        summary.lifetime += transaction.realizedGainDollar;

        if (transaction.transactionDate == today) {
            summary.today += transaction.realizedGainDollar;
        }

        if (startsWith(transaction.transactionDate, monthPrefix)) {
            summary.thisMonth += transaction.realizedGainDollar;
        }

        if (startsWith(transaction.transactionDate, yearPrefix)) {
            summary.thisYear += transaction.realizedGainDollar;
        }
    }

    return summary;
}

PerformanceComparison calculateSnapshotPerformance(
    const std::vector<PortfolioSnapshot>& snapshots,
    double currentPortfolioValue,
    const std::string& today,
    const std::string& monthPrefix,
    const std::string& yearPrefix)
{
    PerformanceComparison comparison;

    const PortfolioSnapshot* previousSnapshot = nullptr;
    const PortfolioSnapshot* firstMonthSnapshot = nullptr;
    const PortfolioSnapshot* firstYearSnapshot = nullptr;

    for (const PortfolioSnapshot& snapshot : snapshots) {
        if (snapshot.snapshotDate >= today) {
            continue;
        }

        if (previousSnapshot == nullptr || snapshot.snapshotDate > previousSnapshot->snapshotDate) {
            previousSnapshot = &snapshot;
        }

        if (startsWith(snapshot.snapshotDate, monthPrefix) &&
            (firstMonthSnapshot == nullptr || snapshot.snapshotDate < firstMonthSnapshot->snapshotDate)) {
            firstMonthSnapshot = &snapshot;
        }

        if (startsWith(snapshot.snapshotDate, yearPrefix) &&
            (firstYearSnapshot == nullptr || snapshot.snapshotDate < firstYearSnapshot->snapshotDate)) {
            firstYearSnapshot = &snapshot;
        }
    }

    if (previousSnapshot != nullptr) {
        comparison.hasDaily = true;
        comparison.daily = currentPortfolioValue - previousSnapshot->portfolioValue;
    }

    if (firstMonthSnapshot != nullptr) {
        comparison.hasMonthly = true;
        comparison.monthly = currentPortfolioValue - firstMonthSnapshot->portfolioValue;
    }

    if (firstYearSnapshot != nullptr) {
        comparison.hasYearly = true;
        comparison.yearly = currentPortfolioValue - firstYearSnapshot->portfolioValue;
    }

    return comparison;
}

}
