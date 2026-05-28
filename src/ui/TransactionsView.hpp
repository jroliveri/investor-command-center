// SPDX-License-Identifier: MIT
#pragma once

#include "models/Transaction.hpp"

#include <functional>
#include <string>

class AppState;
class TransactionRepository;

class TransactionsView {
public:
    void render(AppState& state, TransactionRepository& repository, const std::function<void()>& reloadData);

private:
    void openCreate();
    void openEdit(const Transaction& transaction);
    void drawEditor(AppState& state, TransactionRepository& repository, const std::function<void()>& reloadData);
    void drawDeleteConfirmation(AppState& state, TransactionRepository& repository, const std::function<void()>& reloadData);
    const char* accountNameFor(const AppState& state, int accountId) const;

    Transaction draft_;
    bool editing_ = false;
    int deleteId_ = 0;
    std::string deleteName_;
    std::string formError_;
    std::string searchText_;
};
