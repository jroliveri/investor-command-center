// SPDX-License-Identifier: MIT
#pragma once

#include "models/ImportBatch.hpp"

#include <string>
#include <vector>

class Database;

class ImportBatchRepository {
public:
    explicit ImportBatchRepository(Database& database);

    std::vector<ImportBatch> listAll(std::string& error) const;
    bool create(ImportBatch& batch, std::string& error) const;

    static bool validate(const ImportBatch& batch, std::string& error);

private:
    Database& database_;
};
