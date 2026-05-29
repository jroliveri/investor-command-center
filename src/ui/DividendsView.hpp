// SPDX-License-Identifier: MIT
#pragma once

#include "models/Dividend.hpp"

#include <functional>
#include <string>

class AppState;
class DividendRepository;

class DividendsView {
public:
    void render(AppState& state, DividendRepository& repository, const std::function<void()>& reloadData);

private:
    void openCreate();
    void openEdit(const Dividend& dividend);
    void drawEditor(AppState& state, DividendRepository& repository, const std::function<void()>& reloadData);
    void drawDeleteConfirmation(AppState& state, DividendRepository& repository, const std::function<void()>& reloadData);
    const char* accountNameFor(const AppState& state, int accountId) const;

    Dividend draft_;
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
