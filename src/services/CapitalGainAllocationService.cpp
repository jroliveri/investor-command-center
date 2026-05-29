// SPDX-License-Identifier: MIT
#include "services/CapitalGainAllocationService.hpp"

#include <cmath>

namespace {

double saleProceedsFor(const Transaction& transaction)
{
    if (std::fabs(transaction.saleProceeds) > 0.005) {
        return transaction.saleProceeds;
    }

    return transaction.soldQuantity * transaction.salePrice - transaction.fees;
}

double costBasisFor(const Transaction& transaction, const std::vector<Holding>& holdings)
{
    if (std::fabs(transaction.costBasisUsed) > 0.005) {
        return transaction.costBasisUsed;
    }

    if (transaction.soldQuantity <= 0.0 || transaction.ticker.empty()) {
        return 0.0;
    }

    for (const Holding& holding : holdings) {
        const bool accountMatches = transaction.accountId <= 0 || holding.accountId == transaction.accountId;
        if (accountMatches && holding.ticker == transaction.ticker && holding.averageCost > 0.0) {
            return transaction.soldQuantity * holding.averageCost;
        }
    }

    return 0.0;
}

double realizedGainFor(const Transaction& transaction, double saleProceeds, double costBasisUsed)
{
    if (std::fabs(transaction.realizedGainDollar) > 0.005) {
        return transaction.realizedGainDollar;
    }

    if (std::fabs(saleProceeds) > 0.005 || std::fabs(costBasisUsed) > 0.005) {
        return saleProceeds - costBasisUsed;
    }

    return 0.0;
}

}

namespace CapitalGainAllocationService {

CapitalGainAllocationResult calculate(const Transaction& transaction,
    const std::vector<CapitalGainAllocationRule>& rules,
    const std::vector<Holding>& holdings)
{
    CapitalGainAllocationResult result;
    result.saleProceeds = saleProceedsFor(transaction);
    result.costBasisUsed = costBasisFor(transaction, holdings);
    result.realizedGain = realizedGainFor(transaction, result.saleProceeds, result.costBasisUsed);

    for (const CapitalGainAllocationRule& rule : rules) {
        if (!rule.isActive) {
            continue;
        }

        result.totalPercentage += rule.percentage;
        if (result.realizedGain > 0.0) {
            const double amount = result.realizedGain * (rule.percentage / 100.0);
            result.allocatedAmount += amount;
            result.lines.push_back(CapitalGainAllocationLine {
                rule.name,
                rule.percentage,
                amount,
            });
        }
    }

    if (result.realizedGain > 0.0) {
        result.unallocatedAmount = result.realizedGain - result.allocatedAmount;
    }

    return result;
}

}
