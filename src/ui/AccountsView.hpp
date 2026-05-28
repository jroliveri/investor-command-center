// SPDX-License-Identifier: MIT
#pragma once

#include "models/Account.hpp"

#include <functional>
#include <string>

class AccountRepository;
class AppState;

class AccountsView {
public:
    void render(AppState& state, AccountRepository& repository, const std::function<void()>& reloadData);

private:
    void openCreate();
    void openEdit(const Account& account);
    void drawEditor(AppState& state, AccountRepository& repository, const std::function<void()>& reloadData);
    void drawDeleteConfirmation(AppState& state, AccountRepository& repository, const std::function<void()>& reloadData);

    Account draft_;
    bool editing_ = false;
    int deleteId_ = 0;
    std::string deleteName_;
    std::string formError_;
    std::string searchText_;
};
