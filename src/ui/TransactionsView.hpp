// SPDX-License-Identifier: MIT
#pragma once

#include "models/Transaction.hpp"

#include <functional>
#include <string>

class AppState;
class TransactionService;

class TransactionsView {
public:
    void render(AppState& state, TransactionService& service, const std::function<void()>& reloadData);

private:
    void openCreate();
    void openEdit(const Transaction& transaction);
    void drawEditor(AppState& state, TransactionService& service, const std::function<void()>& reloadData);
    void drawDeleteConfirmation(AppState& state, TransactionService& service, const std::function<void()>& reloadData);
    void drawAllocationPopup(const AppState& state);
    const char* accountNameFor(const AppState& state, int accountId) const;

    Transaction draft_;
    Transaction allocationTransaction_;
    bool editing_ = false;
    bool openEditorPopup_ = false;
    bool openDeletePopup_ = false;
    bool openAllocationPopup_ = false;
    bool hasAllocationTransaction_ = false;
    int deleteId_ = 0;
    std::string deleteName_;
    std::string formError_;
    std::string searchText_;
};
