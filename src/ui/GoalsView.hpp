// SPDX-License-Identifier: MIT
#pragma once

#include "models/Goal.hpp"

#include <functional>
#include <string>

class AppState;
class GoalRepository;

class GoalsView {
public:
    void render(AppState& state, GoalRepository& repository, const std::function<void()>& reloadData);

private:
    void openCreate();
    void openEdit(const Goal& goal);
    void drawEditor(AppState& state, GoalRepository& repository, const std::function<void()>& reloadData);
    void drawDeleteConfirmation(AppState& state, GoalRepository& repository, const std::function<void()>& reloadData);

    Goal draft_;
    bool editing_ = false;
    bool openEditorPopup_ = false;
    bool openDeletePopup_ = false;
    int deleteId_ = 0;
    std::string deleteName_;
    std::string editorPopupId_;
    std::string deletePopupId_;
    std::string formError_;
    std::string searchText_;
};
