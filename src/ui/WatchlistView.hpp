// SPDX-License-Identifier: MIT
#pragma once

#include "models/WatchlistItem.hpp"

#include <functional>
#include <string>

class AppState;
class WatchlistRepository;

class WatchlistView {
public:
    void render(AppState& state, WatchlistRepository& repository, const std::function<void()>& reloadData);

private:
    void openCreate();
    void openEdit(const WatchlistItem& item);
    void drawEditor(AppState& state, WatchlistRepository& repository, const std::function<void()>& reloadData);
    void drawDeleteConfirmation(AppState& state, WatchlistRepository& repository, const std::function<void()>& reloadData);
    void drawPriorityBadge(const std::string& priority) const;

    WatchlistItem draft_;
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
