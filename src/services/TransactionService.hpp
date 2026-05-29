// SPDX-License-Identifier: MIT
#pragma once

#include "models/Holding.hpp"
#include "models/Transaction.hpp"

#include <string>
#include <vector>

class Database;
class HoldingRepository;
class TransactionRepository;

class TransactionService {
public:
    TransactionService(Database& database, TransactionRepository& transactionRepository, HoldingRepository& holdingRepository);

    Transaction preview(const Transaction& transaction, const std::vector<Holding>& holdings) const;
    bool create(Transaction& transaction, const std::vector<Holding>& holdings, std::string& error) const;
    bool update(Transaction& transaction, const std::vector<Holding>& holdings, std::string& error) const;
    bool remove(int id, std::string& error) const;

private:
    Transaction prepare(const Transaction& transaction, const std::vector<Holding>& holdings) const;
    bool applyCreateHoldingImpact(const Transaction& transaction, const std::vector<Holding>& holdings, std::string& error) const;
    const Holding* findHolding(const Transaction& transaction, const std::vector<Holding>& holdings) const;

    Database& database_;
    TransactionRepository& transactionRepository_;
    HoldingRepository& holdingRepository_;
};
