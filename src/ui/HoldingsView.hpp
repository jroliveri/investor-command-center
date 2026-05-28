// SPDX-License-Identifier: MIT
#pragma once

#include "models/Holding.hpp"

#include <functional>
#include <string>

class AppState;
class HoldingRepository;

class HoldingsView {
public:
    void render(AppState& state, HoldingRepository& repository, const std::function<void()>& reloadData);

private:
    void openCreate(const AppState& state);
    void openEdit(const Holding& holding);
    void drawEditor(AppState& state, HoldingRepository& repository, const std::function<void()>& reloadData);
    void drawDeleteConfirmation(AppState& state, HoldingRepository& repository, const std::function<void()>& reloadData);
    const char* accountNameFor(const AppState& state, int accountId) const;

    Holding draft_;
    bool editing_ = false;
    int deleteId_ = 0;
    std::string deleteName_;
    std::string formError_;
    std::string searchText_;
};
