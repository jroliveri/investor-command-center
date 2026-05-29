// SPDX-License-Identifier: MIT
#include "services/TransactionService.hpp"

#include "db/Database.hpp"
#include "repositories/HoldingRepository.hpp"
#include "repositories/TransactionRepository.hpp"

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

std::string upperCopy(std::string value)
{
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char character) {
        return static_cast<char>(std::toupper(character));
    });
    return value;
}

bool sameTicker(const std::string& left, const std::string& right)
{
    return upperCopy(left) == upperCopy(right);
}

bool isBuy(const Transaction& transaction)
{
    return lowerCopy(transaction.transactionType) == "buy";
}

bool isSell(const Transaction& transaction)
{
    return lowerCopy(transaction.transactionType) == "sell";
}

double realizedPercent(double gain, double costBasis)
{
    return costBasis == 0.0 ? 0.0 : gain / costBasis * 100.0;
}

}

TransactionService::TransactionService(Database& database, TransactionRepository& transactionRepository, HoldingRepository& holdingRepository)
    : database_(database)
    , transactionRepository_(transactionRepository)
    , holdingRepository_(holdingRepository)
{
}

Transaction TransactionService::preview(const Transaction& transaction, const std::vector<Holding>& holdings) const
{
    return prepare(transaction, holdings);
}

bool TransactionService::create(Transaction& transaction, const std::vector<Holding>& holdings, std::string& error) const
{
    error.clear();
    transaction = prepare(transaction, holdings);

    if (!database_.execute("BEGIN TRANSACTION;")) {
        error = database_.lastError();
        return false;
    }

    if (!applyCreateHoldingImpact(transaction, holdings, error)) {
        database_.execute("ROLLBACK;");
        return false;
    }

    if (!transactionRepository_.create(transaction, error)) {
        database_.execute("ROLLBACK;");
        return false;
    }

    if (!database_.execute("COMMIT;")) {
        error = database_.lastError();
        return false;
    }

    return true;
}

bool TransactionService::update(Transaction& transaction, const std::vector<Holding>& holdings, std::string& error) const
{
    error.clear();
    transaction = prepare(transaction, holdings);
    return transactionRepository_.update(transaction, error);
}

bool TransactionService::remove(int id, std::string& error) const
{
    return transactionRepository_.remove(id, error);
}

Transaction TransactionService::prepare(const Transaction& transaction, const std::vector<Holding>& holdings) const
{
    Transaction prepared = transaction;

    if (isBuy(prepared)) {
        prepared.soldQuantity = 0.0;
        prepared.salePrice = 0.0;
        prepared.saleProceeds = 0.0;
        prepared.costBasisUsed = 0.0;
        prepared.realizedGainDollar = 0.0;
        prepared.realizedGainPercent = 0.0;
        if (prepared.totalAmount == 0.0 && prepared.quantity > 0.0 && prepared.price > 0.0) {
            prepared.totalAmount = prepared.quantity * prepared.price + prepared.fees;
        }
        return prepared;
    }

    if (!isSell(prepared)) {
        prepared.soldQuantity = 0.0;
        prepared.salePrice = 0.0;
        prepared.saleProceeds = 0.0;
        prepared.costBasisUsed = 0.0;
        prepared.realizedGainDollar = 0.0;
        prepared.realizedGainPercent = 0.0;
        return prepared;
    }

    if (prepared.soldQuantity <= 0.0) {
        prepared.soldQuantity = prepared.quantity;
    }
    if (prepared.salePrice <= 0.0) {
        prepared.salePrice = prepared.price;
    }
    prepared.quantity = prepared.soldQuantity;
    prepared.price = prepared.salePrice;

    prepared.saleProceeds = prepared.soldQuantity * prepared.salePrice - prepared.fees;

    const Holding* holding = findHolding(prepared, holdings);
    if (holding != nullptr) {
        prepared.costBasisUsed = prepared.soldQuantity * holding->averageCost;
    }

    prepared.totalAmount = prepared.saleProceeds;
    prepared.realizedGainDollar = prepared.saleProceeds - prepared.costBasisUsed;
    prepared.realizedGainPercent = realizedPercent(prepared.realizedGainDollar, prepared.costBasisUsed);
    return prepared;
}

bool TransactionService::applyCreateHoldingImpact(const Transaction& transaction, const std::vector<Holding>& holdings, std::string& error) const
{
    error.clear();

    if (!isBuy(transaction) && !isSell(transaction)) {
        return true;
    }

    if (transaction.accountId <= 0 || transaction.ticker.empty()) {
        return true;
    }

    const Holding* existing = findHolding(transaction, holdings);

    if (isSell(transaction)) {
        if (existing == nullptr) {
            if (transaction.isAdjustment) {
                return true;
            }

            error = "Cannot sell shares because no matching holding exists. Mark as adjustment to save without changing holdings.";
            return false;
        }

        if (transaction.soldQuantity > existing->shares && !transaction.isAdjustment) {
            error = "Cannot sell more shares than currently owned. Mark as adjustment to allow this ledger entry.";
            return false;
        }

        Holding updated = *existing;
        updated.shares = std::max(0.0, updated.shares - transaction.soldQuantity);
        return holdingRepository_.update(updated, error);
    }

    if (transaction.quantity <= 0.0) {
        return true;
    }

    const double buyCost = transaction.quantity * transaction.price + transaction.fees;
    if (existing == nullptr) {
        Holding holding;
        holding.accountId = transaction.accountId;
        holding.ticker = transaction.ticker;
        holding.assetName = transaction.assetName.empty() ? transaction.ticker : transaction.assetName;
        holding.assetType = "Stock";
        holding.shares = transaction.quantity;
        holding.averageCost = transaction.quantity == 0.0 ? 0.0 : buyCost / transaction.quantity;
        holding.currentPrice = transaction.price;
        holding.notes = "Created from buy transaction.";
        return holdingRepository_.create(holding, error);
    }

    Holding updated = *existing;
    updated.status = "Active";
    const double oldCostBasis = updated.shares * updated.averageCost;
    const double newShares = updated.shares + transaction.quantity;
    updated.shares = newShares;
    updated.averageCost = newShares == 0.0 ? 0.0 : (oldCostBasis + buyCost) / newShares;
    if (transaction.price > 0.0) {
        updated.currentPrice = transaction.price;
    }
    if (!transaction.assetName.empty()) {
        updated.assetName = transaction.assetName;
    }
    return holdingRepository_.update(updated, error);
}

const Holding* TransactionService::findHolding(const Transaction& transaction, const std::vector<Holding>& holdings) const
{
    for (const Holding& holding : holdings) {
        if (holding.accountId == transaction.accountId && sameTicker(holding.ticker, transaction.ticker)) {
            return &holding;
        }
    }

    return nullptr;
}
