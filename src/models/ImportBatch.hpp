// SPDX-License-Identifier: MIT
#pragma once

#include <string>

struct ImportBatch {
    int id = 0;
    int accountId = 0;
    std::string importDate;
    std::string sourceType = "CSV";
    std::string sourceName;
    int totalRows = 0;
    int importedRows = 0;
    int updatedHoldings = 0;
    int addedHoldings = 0;
    int skippedRows = 0;
    int warningCount = 0;
    int errorCount = 0;
    int missingHoldings = 0;
    std::string notes;
    std::string createdAt;
    std::string updatedAt;
};
