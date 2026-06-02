// SPDX-License-Identifier: MIT
#pragma once

#include "models/Watchlist.hpp"
#include "models/WatchlistItem.hpp"

#include <functional>
#include <string>
#include <vector>

class AppState;
class MarketDataService;
class WatchlistRepository;

class WatchlistView {
public:
    void render(AppState& state, WatchlistRepository& repository, MarketDataService& marketDataService, const std::function<void()>& reloadData);

private:
    void drawWatchlistManager(AppState& state, WatchlistRepository& repository, const std::function<void()>& reloadData);
    void drawWatchlistItems(AppState& state, WatchlistRepository& repository, MarketDataService& marketDataService, const std::function<void()>& reloadData);
    void openWatchlistCreate(const AppState& state);
    void openWatchlistEdit(const Watchlist& watchlist);
    void drawWatchlistEditor(AppState& state, WatchlistRepository& repository, const std::function<void()>& reloadData);
    void drawWatchlistDeactivateConfirmation(AppState& state, WatchlistRepository& repository, const std::function<void()>& reloadData);
    void openCreate(int watchlistId);
    void openEdit(const WatchlistItem& item);
    void drawEditor(AppState& state, WatchlistRepository& repository, const std::function<void()>& reloadData);
    void drawDeleteConfirmation(AppState& state, WatchlistRepository& repository, const std::function<void()>& reloadData);
    void refreshPrices(AppState& state, WatchlistRepository& repository, MarketDataService& marketDataService, const std::vector<WatchlistItem>& items, const std::string& watchlistName, const std::function<void()>& reloadData);
    void drawPriorityBadge(const std::string& priority) const;
    void drawSignalBadge(const WatchlistItem& item);
    void drawSignalNoticePopup();
    void ensureSelectedWatchlist(const AppState& state);
    bool hasSignalLevelError(const WatchlistItem& item) const;

    Watchlist watchlistDraft_;
    WatchlistItem draft_;
    bool editingWatchlist_ = false;
    bool editing_ = false;
    bool showInactiveWatchlists_ = false;
    bool openWatchlistEditorPopup_ = false;
    bool openDeactivateWatchlistPopup_ = false;
    bool openEditorPopup_ = false;
    bool openDeletePopup_ = false;
    bool openSignalNoticePopup_ = false;
    int selectedWatchlistId_ = 0;
    int deactivateWatchlistId_ = 0;
    int deactivateWatchlistItemCount_ = 0;
    int deleteId_ = 0;
    std::string deactivateWatchlistName_;
    std::string deleteName_;
    std::string watchlistEditorPopupId_;
    std::string deactivateWatchlistPopupId_;
    std::string editorPopupId_;
    std::string deletePopupId_;
    std::string signalNoticeTicker_;
    std::string signalNoticeStatus_;
    std::string watchlistFormError_;
    std::string formError_;
    std::string searchText_;
};
