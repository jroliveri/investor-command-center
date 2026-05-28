// SPDX-License-Identifier: MIT
#pragma once

#include "models/Transaction.hpp"

#include <string>
#include <vector>

class Database;

class TransactionRepository {
public:
    explicit TransactionRepository(Database& database);

    std::vector<Transaction> listAll(std::string& error) const;
    bool create(Transaction& transaction, std::string& error) const;
    bool update(const Transaction& transaction, std::string& error) const;
    bool remove(int id, std::string& error) const;

    static bool validate(const Transaction& transaction, std::string& error);

private:
    Database& database_;
};
