// SPDX-License-Identifier: MIT
#pragma once

#include "models/CapitalGainAllocationRule.hpp"
#include "models/Holding.hpp"
#include "models/Transaction.hpp"

#include <string>
#include <vector>

struct CapitalGainAllocationLine {
    std::string category;
    double percentage = 0.0;
    double amount = 0.0;
};

struct CapitalGainAllocationResult {
    double saleProceeds = 0.0;
    double costBasisUsed = 0.0;
    double realizedGain = 0.0;
    double totalPercentage = 0.0;
    double allocatedAmount = 0.0;
    double unallocatedAmount = 0.0;
    std::vector<CapitalGainAllocationLine> lines;
};

namespace CapitalGainAllocationService {
CapitalGainAllocationResult calculate(const Transaction& transaction,
    const std::vector<CapitalGainAllocationRule>& rules,
    const std::vector<Holding>& holdings = {});
}
