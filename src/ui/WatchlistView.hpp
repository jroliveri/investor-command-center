// SPDX-License-Identifier: MIT
#pragma once

#include "models/WatchlistItem.hpp"

#include <functional>
#include <string>

class AppState;
class MarketDataService;
<<<<<<< Updated upstream
=======
struct SignalRules;
class TechnicalIndicatorService;
struct WatchlistSignalResult;
>>>>>>> Stashed changes
class WatchlistRepository;

class WatchlistView {
public:
    void render(AppState& state, WatchlistRepository& repository, MarketDataService& marketDataService, const std::function<void()>& reloadData);

private:
    void openCreate();
    void openEdit(const WatchlistItem& item);
    void drawEditor(AppState& state, WatchlistRepository& repository, const std::function<void()>& reloadData);
    void drawDeleteConfirmation(AppState& state, WatchlistRepository& repository, const std::function<void()>& reloadData);
    void refreshPrices(AppState& state, WatchlistRepository& repository, MarketDataService& marketDataService, const std::function<void()>& reloadData);
    void drawPriorityBadge(const std::string& priority) const;
<<<<<<< Updated upstream
    void drawSignalBadge(const WatchlistItem& item);
=======
    void drawSignalBadge(const WatchlistItem& item, const WatchlistSignalResult& signal, const std::string& detailText);
>>>>>>> Stashed changes
    void drawSignalNoticePopup();
    bool hasSignalLevelError(const WatchlistItem& item) const;

    WatchlistItem draft_;
    bool editing_ = false;
    bool openEditorPopup_ = false;
    bool openDeletePopup_ = false;
    bool openSignalNoticePopup_ = false;
<<<<<<< Updated upstream
=======
    bool showExtraTechnicals_ = false;
    int selectedWatchlistId_ = 0;
    int deactivateWatchlistId_ = 0;
    int deactivateWatchlistItemCount_ = 0;
>>>>>>> Stashed changes
    int deleteId_ = 0;
    std::string deleteName_;
    std::string editorPopupId_;
    std::string deletePopupId_;
    std::string signalNoticeTicker_;
    std::string signalNoticeStatus_;
    std::string formError_;
    std::string searchText_;
};
