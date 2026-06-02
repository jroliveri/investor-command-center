// SPDX-License-Identifier: MIT
#include "ui/WatchlistView.hpp"

#include "app/AppState.hpp"
#include "repositories/WatchlistRepository.hpp"
#include "services/MarketDataService.hpp"
#include "services/PortfolioCalculator.hpp"
#include "services/WatchlistSignalService.hpp"
#include "ui/UiTheme.hpp"
#include "util/Money.hpp"

#include <imgui.h>
#include <imgui_stdlib.h>

#include <algorithm>
#include <array>
#include <cctype>
#include <string>
#include <vector>

namespace {

constexpr const char* WatchlistGroupEditorPopup = "Watchlist Manager";
constexpr const char* DeactivateWatchlistPopup = "Deactivate Watchlist";
constexpr const char* WatchlistEditorPopup = "Watchlist Editor";
constexpr const char* DeleteWatchlistPopup = "Delete Watchlist Item";
constexpr const char* SignalNoticePopup = "Watchlist Price Signal";
constexpr const char* NewWatchlistGroupEditorPopup = "Watchlist Manager###watchlist_group_edit_popup_new";
constexpr const char* NewWatchlistEditorPopup = "Watchlist Editor###watchlist_edit_popup_new";

std::string watchlistGroupEditorPopupId(int watchlistId)
{
    return "Watchlist Manager###watchlist_group_edit_popup_" + std::to_string(watchlistId);
}

std::string watchlistDeactivatePopupId(int watchlistId)
{
    return "Deactivate Watchlist###watchlist_group_deactivate_popup_" + std::to_string(watchlistId);
}

std::string watchlistEditorPopupId(int itemId)
{
    return "Watchlist Editor###watchlist_edit_popup_" + std::to_string(itemId);
}

std::string watchlistDeletePopupId(int itemId)
{
    return "Delete Watchlist Item###watchlist_delete_popup_" + std::to_string(itemId);
}

std::string lowerCopy(std::string value)
{
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char character) {
        return static_cast<char>(std::tolower(character));
    });
    return value;
}

bool containsCaseInsensitive(const std::string& value, const std::string& query)
{
    return lowerCopy(value).find(lowerCopy(query)) != std::string::npos;
}

bool matchesFilter(const WatchlistItem& item, const std::string& query)
{
    if (query.empty()) {
        return true;
    }

    return containsCaseInsensitive(item.ticker, query) ||
        containsCaseInsensitive(item.assetName, query) ||
        containsCaseInsensitive(item.assetType, query) ||
        containsCaseInsensitive(item.priority, query) ||
        containsCaseInsensitive(item.signalStatus, query) ||
        containsCaseInsensitive(item.priceSource, query) ||
        containsCaseInsensitive(item.reasonWatching, query) ||
        containsCaseInsensitive(item.riskNotes, query);
}

template <std::size_t Size>
void drawStringCombo(const char* label, std::string& value, const std::array<const char*, Size>& options)
{
    const char* preview = value.empty() ? options[0] : value.c_str();
    if (ImGui::BeginCombo(label, preview)) {
        for (const char* option : options) {
            const bool selected = value == option;
            if (ImGui::Selectable(option, selected)) {
                value = option;
            }
            if (selected) {
                ImGui::SetItemDefaultFocus();
            }
        }
        ImGui::EndCombo();
    }
}

const char* emptyIfBlank(const std::string& value)
{
    return value.empty() ? "N/A" : value.c_str();
}

std::string priceOrNotSet(double value)
{
    return value > 0.0 ? Money::format(value) : "Not set";
}

std::string currentPriceText(double value)
{
    return value > 0.0 ? Money::format(value) : "N/A";
}

ImVec4 signalColor(const std::string& status)
{
    if (status == "Buy Signal") {
        return UiTheme::Gain;
    }
    if (status == "Sell Signal") {
        return UiTheme::Loss;
    }
    if (status == "Check Signals") {
        return UiTheme::Amber;
    }
    if (status == "No Price") {
        return UiTheme::TextMuted;
    }
    return UiTheme::TextSecondary;
}

bool isActionableSignal(const std::string& status)
{
    return status == "Buy Signal" || status == "Sell Signal" || status == "Check Signals";
}

std::vector<Watchlist> activeWatchlists(const AppState& state)
{
    std::vector<Watchlist> active;
    for (const Watchlist& watchlist : state.watchlists) {
        if (watchlist.isActive) {
            active.push_back(watchlist);
        }
    }
    return active;
}

std::vector<Watchlist> visibleWatchlists(const AppState& state, bool showInactive)
{
    std::vector<Watchlist> visible;
    for (const Watchlist& watchlist : state.watchlists) {
        if (showInactive || watchlist.isActive) {
            visible.push_back(watchlist);
        }
    }
    return visible;
}

const Watchlist* findWatchlist(const std::vector<Watchlist>& watchlists, int watchlistId)
{
    const auto result = std::find_if(watchlists.begin(), watchlists.end(), [watchlistId](const Watchlist& watchlist) {
        return watchlist.id == watchlistId;
    });
    return result == watchlists.end() ? nullptr : &(*result);
}

std::vector<WatchlistItem> itemsForWatchlist(const AppState& state, int watchlistId)
{
    std::vector<WatchlistItem> items;
    for (const WatchlistItem& item : state.watchlist) {
        if (item.watchlistId == watchlistId) {
            items.push_back(item);
        }
    }
    return items;
}

int nextSortOrder(const AppState& state)
{
    int sortOrder = 0;
    for (const Watchlist& watchlist : state.watchlists) {
        sortOrder = std::max(sortOrder, watchlist.sortOrder + 1);
    }
    return sortOrder;
}

std::string watchlistDisplayName(const std::vector<Watchlist>& watchlists, int watchlistId)
{
    const Watchlist* watchlist = findWatchlist(watchlists, watchlistId);
    return watchlist == nullptr ? "Choose Watchlist" : watchlist->name;
}

const char* sidebarSlotLabel(const Watchlist& watchlist)
{
    if (!watchlist.isActive || !watchlist.showInSidebar || watchlist.sidebarSlot <= 0) {
        return "Not shown";
    }
    if (watchlist.sidebarSlot == 1) {
        return "Sidebar Watchlist 1";
    }
    if (watchlist.sidebarSlot == 2) {
        return "Sidebar Watchlist 2";
    }
    return "Not shown";
}

bool updateSidebarSlot(WatchlistRepository& repository, const Watchlist& watchlist, int slot, std::string& error)
{
    Watchlist updated = watchlist;
    updated.showInSidebar = slot > 0 && watchlist.isActive;
    updated.sidebarSlot = updated.showInSidebar ? slot : 0;
    return repository.updateWatchlist(updated, error);
}

}

void WatchlistView::render(AppState& state,
    WatchlistRepository& repository,
    MarketDataService& marketDataService,
    const std::function<void()>& reloadData)
{
    ensureSelectedWatchlist(state);

    UiTheme::sectionHeading("Watchlist", "Named local watchlists for manual review, without recommendations.");
    ImGui::TextWrapped("Watchlist price signals are user-defined tracking levels only. Yahoo Finance data is informational and may be delayed, unavailable, or incomplete.");

    drawWatchlistManager(state, repository, reloadData);
    ImGui::Spacing();
    drawWatchlistItems(state, repository, marketDataService, reloadData);

    if (openWatchlistEditorPopup_) {
        ImGui::OpenPopup(watchlistEditorPopupId_.empty() ? WatchlistGroupEditorPopup : watchlistEditorPopupId_.c_str());
        openWatchlistEditorPopup_ = false;
    }
    if (openDeactivateWatchlistPopup_) {
        ImGui::OpenPopup(deactivateWatchlistPopupId_.empty() ? DeactivateWatchlistPopup : deactivateWatchlistPopupId_.c_str());
        openDeactivateWatchlistPopup_ = false;
    }
    if (openEditorPopup_) {
        ImGui::OpenPopup(editorPopupId_.empty() ? WatchlistEditorPopup : editorPopupId_.c_str());
        openEditorPopup_ = false;
    }
    if (openDeletePopup_) {
        ImGui::OpenPopup(deletePopupId_.empty() ? DeleteWatchlistPopup : deletePopupId_.c_str());
        openDeletePopup_ = false;
    }
    if (openSignalNoticePopup_) {
        ImGui::OpenPopup(SignalNoticePopup);
        openSignalNoticePopup_ = false;
    }

    drawWatchlistEditor(state, repository, reloadData);
    drawWatchlistDeactivateConfirmation(state, repository, reloadData);
    drawEditor(state, repository, reloadData);
    drawDeleteConfirmation(state, repository, reloadData);
    drawSignalNoticePopup();
}

void WatchlistView::drawWatchlistManager(AppState& state, WatchlistRepository& repository, const std::function<void()>& reloadData)
{
    ImGui::BeginChild("WatchlistManagerPanel", ImVec2(0.0f, 250.0f), true);
    ImGui::TextColored(UiTheme::Accent, "Watchlist Manager");
    ImGui::TextWrapped("Create and organize named local watchlists. Deactivated watchlists are hidden from normal watchlist and sidebar views.");
    ImGui::Separator();

    if (ImGui::Button("Create Watchlist", ImVec2(150.0f, 0.0f))) {
        openWatchlistCreate(state);
        openWatchlistEditorPopup_ = true;
    }
    ImGui::SameLine();
    ImGui::Checkbox("Show inactive", &showInactiveWatchlists_);

    std::vector<Watchlist> visible = visibleWatchlists(state, showInactiveWatchlists_);
    if (visible.empty()) {
        UiTheme::emptyState("No watchlists yet", "Create a watchlist to start grouping local ticker ideas.");
        ImGui::EndChild();
        return;
    }

    ImGui::Spacing();
    if (ImGui::BeginTable("WatchlistManagerTable", 8, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable | ImGuiTableFlags_SizingStretchProp)) {
        ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthFixed, 180.0f);
        ImGui::TableSetupColumn("Description", ImGuiTableColumnFlags_WidthStretch, 1.0f);
        ImGui::TableSetupColumn("Status", ImGuiTableColumnFlags_WidthFixed, 76.0f);
        ImGui::TableSetupColumn("Sidebar", ImGuiTableColumnFlags_WidthFixed, 178.0f);
        ImGui::TableSetupColumn("Items", ImGuiTableColumnFlags_WidthFixed, 58.0f);
        ImGui::TableSetupColumn("Order", ImGuiTableColumnFlags_WidthFixed, 98.0f);
        ImGui::TableSetupColumn("Active", ImGuiTableColumnFlags_WidthFixed, 104.0f);
        ImGui::TableSetupColumn("Actions", ImGuiTableColumnFlags_WidthFixed, 130.0f);
        ImGui::TableHeadersRow();

        for (std::size_t index = 0; index < visible.size(); ++index) {
            const Watchlist& watchlist = visible[index];
            std::string countError;
            const int itemCount = repository.itemCountForWatchlist(watchlist.id, countError);

            ImGui::TableNextRow();
            ImGui::PushID(watchlist.id);
            ImGui::TableNextColumn();
            ImGui::Text("%s", watchlist.name.c_str());
            if (watchlist.id == selectedWatchlistId_) {
                ImGui::TextColored(UiTheme::TextMuted, "Selected");
            }
            ImGui::TableNextColumn();
            ImGui::TextWrapped("%s", watchlist.description.empty() ? "N/A" : watchlist.description.c_str());
            ImGui::TableNextColumn();
            ImGui::TextColored(watchlist.isActive ? UiTheme::TextSuccess : UiTheme::TextMuted, "%s", watchlist.isActive ? "Active" : "Inactive");
            ImGui::TableNextColumn();
            if (!watchlist.isActive) {
                ImGui::TextColored(UiTheme::TextMuted, "Not shown");
            } else if (ImGui::BeginCombo("##SidebarSlot", sidebarSlotLabel(watchlist))) {
                struct SidebarOption {
                    int slot;
                    const char* label;
                };
                constexpr std::array<SidebarOption, 3> options {{
                    { 0, "Not shown" },
                    { 1, "Sidebar Watchlist 1" },
                    { 2, "Sidebar Watchlist 2" },
                }};

                const int currentSlot = watchlist.showInSidebar ? watchlist.sidebarSlot : 0;
                for (const SidebarOption& option : options) {
                    const bool selected = currentSlot == option.slot;
                    if (ImGui::Selectable(option.label, selected)) {
                        std::string error;
                        const std::string statusMessage = option.slot == 0
                            ? watchlist.name + " removed from sidebar."
                            : std::string(option.label) + " now shows " + watchlist.name + ". Any previous assignment for that slot was cleared.";
                        if (updateSidebarSlot(repository, watchlist, option.slot, error)) {
                            reloadData();
                            state.setStatus(statusMessage);
                        } else {
                            state.setStatus("Could not update sidebar assignment: " + error, true);
                        }
                    }
                    if (selected) {
                        ImGui::SetItemDefaultFocus();
                    }
                }
                ImGui::EndCombo();
            }
            ImGui::TableNextColumn();
            if (countError.empty()) {
                ImGui::Text("%d", itemCount);
            } else {
                ImGui::TextColored(UiTheme::TextWarning, "N/A");
            }
            ImGui::TableNextColumn();
            if (index == 0) {
                ImGui::BeginDisabled();
            }
            if (ImGui::SmallButton("Up")) {
                std::vector<Watchlist> reordered = visible;
                std::swap(reordered[index], reordered[index - 1]);
                std::string error;
                if (repository.saveWatchlistOrder(reordered, error)) {
                    reloadData();
                    state.setStatus("Watchlist order updated.");
                } else {
                    state.setStatus("Could not update watchlist order: " + error, true);
                }
            }
            if (index == 0) {
                ImGui::EndDisabled();
            }
            ImGui::SameLine();
            if (index + 1 >= visible.size()) {
                ImGui::BeginDisabled();
            }
            if (ImGui::SmallButton("Down")) {
                std::vector<Watchlist> reordered = visible;
                std::swap(reordered[index], reordered[index + 1]);
                std::string error;
                if (repository.saveWatchlistOrder(reordered, error)) {
                    reloadData();
                    state.setStatus("Watchlist order updated.");
                } else {
                    state.setStatus("Could not update watchlist order: " + error, true);
                }
            }
            if (index + 1 >= visible.size()) {
                ImGui::EndDisabled();
            }
            ImGui::TableNextColumn();
            if (watchlist.isActive) {
                if (ImGui::SmallButton("Deactivate")) {
                    deactivateWatchlistId_ = watchlist.id;
                    deactivateWatchlistName_ = watchlist.name;
                    deactivateWatchlistItemCount_ = itemCount;
                    deactivateWatchlistPopupId_ = watchlistDeactivatePopupId(watchlist.id);
                    openDeactivateWatchlistPopup_ = true;
                }
            } else if (ImGui::SmallButton("Activate")) {
                Watchlist updated = watchlist;
                updated.isActive = true;
                std::string error;
                if (repository.updateWatchlist(updated, error)) {
                    selectedWatchlistId_ = updated.id;
                    reloadData();
                    state.setStatus("Watchlist activated.");
                } else {
                    state.setStatus("Could not activate watchlist: " + error, true);
                }
            }
            ImGui::TableNextColumn();
            const std::string editButtonId = "Edit##watchlist_group_edit_button_" + std::to_string(watchlist.id);
            if (ImGui::SmallButton(editButtonId.c_str())) {
                openWatchlistEdit(watchlist);
                openWatchlistEditorPopup_ = true;
            }
            ImGui::PopID();
        }

        ImGui::EndTable();
    }

    ImGui::EndChild();
}

void WatchlistView::drawWatchlistItems(AppState& state,
    WatchlistRepository& repository,
    MarketDataService& marketDataService,
    const std::function<void()>& reloadData)
{
    ImGui::BeginChild("WatchlistItemsPanel", ImVec2(0.0f, 0.0f), true);
    ImGui::TextColored(UiTheme::Accent, "Watchlist Items");
    ImGui::TextWrapped("Select an active watchlist, then add or edit the local symbols assigned to it.");
    ImGui::Separator();

    const std::vector<Watchlist> active = activeWatchlists(state);
    if (active.empty()) {
        UiTheme::emptyState("No active watchlists", "Create or reactivate a watchlist before adding items.");
        ImGui::EndChild();
        return;
    }

    const std::string selectedName = watchlistDisplayName(active, selectedWatchlistId_);
    ImGui::AlignTextToFramePadding();
    ImGui::TextColored(UiTheme::TextSecondary, "Selected Watchlist");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(260.0f);
    if (ImGui::BeginCombo("##SelectedWatchlist", selectedName.c_str())) {
        for (const Watchlist& watchlist : active) {
            const bool selected = watchlist.id == selectedWatchlistId_;
            if (ImGui::Selectable(watchlist.name.c_str(), selected)) {
                selectedWatchlistId_ = watchlist.id;
                searchText_.clear();
            }
            if (selected) {
                ImGui::SetItemDefaultFocus();
            }
        }
        ImGui::EndCombo();
    }

    std::vector<WatchlistItem> selectedItems = itemsForWatchlist(state, selectedWatchlistId_);
    const WatchlistSummary summary = PortfolioCalculator::calculateWatchlist(selectedItems);
    ImGui::SameLine();
    ImGui::TextColored(UiTheme::Loss, "High: %d", summary.highPriority);
    ImGui::SameLine();
    ImGui::TextColored(UiTheme::Amber, "Medium: %d", summary.mediumPriority);
    ImGui::SameLine();
    ImGui::TextColored(UiTheme::MutedText, "Low: %d", summary.lowPriority);

    ImGui::Spacing();
    if (ImGui::Button("Add Watchlist Item", ImVec2(150.0f, 0.0f))) {
        openCreate(selectedWatchlistId_);
        openEditorPopup_ = true;
    }
    ImGui::SameLine();
    if (ImGui::Button("Refresh Selected Watchlist", ImVec2(194.0f, 0.0f))) {
        refreshPrices(state, repository, marketDataService, selectedItems, selectedName, reloadData);
    }
    ImGui::SameLine();
    if (ImGui::Button("Refresh All Watchlists", ImVec2(180.0f, 0.0f))) {
        refreshPrices(state, repository, marketDataService, state.watchlist, "All Watchlists", reloadData);
    }
    ImGui::SameLine();
    ImGui::SetNextItemWidth(300.0f);
    ImGui::InputTextWithHint("##WatchlistSearch", "Search selected watchlist", &searchText_);

    const WatchlistPriceRefreshStatus& refreshStatus = state.watchlistPriceRefreshStatus;
    if (refreshStatus.hasRun) {
        ImGui::Spacing();
        ImGui::TextColored(UiTheme::TextSecondary, "Last refresh: %s  Source: %s  Refreshed: %d  Cached: %d  Failed: %d",
            emptyIfBlank(refreshStatus.lastRefreshedAt),
            emptyIfBlank(refreshStatus.provider),
            refreshStatus.refreshedSymbols,
            refreshStatus.cachedSymbols,
            refreshStatus.failedSymbols);
        if (!refreshStatus.warning.empty()) {
            ImGui::TextColored(refreshStatus.failedSymbols > 0 ? UiTheme::TextWarning : UiTheme::TextMuted, "%s", refreshStatus.warning.c_str());
        }
    }

    ImGui::Spacing();
    int visibleRows = 0;
    if (selectedItems.empty()) {
        UiTheme::emptyState("No items in this watchlist", "Add symbols to this named watchlist when you want to monitor them manually.");
    } else if (ImGui::BeginTable("WatchlistTable", 10, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable | ImGuiTableFlags_Reorderable | ImGuiTableFlags_Hideable | ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_ScrollX)) {
        ImGui::TableSetupColumn("Ticker", ImGuiTableColumnFlags_WidthFixed, 76.0f);
        ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthStretch, 1.3f);
        ImGui::TableSetupColumn("Priority", ImGuiTableColumnFlags_WidthFixed, 76.0f);
        ImGui::TableSetupColumn("Current Price", ImGuiTableColumnFlags_WidthFixed, 112.0f);
        ImGui::TableSetupColumn("Buy Signal", ImGuiTableColumnFlags_WidthFixed, 112.0f);
        ImGui::TableSetupColumn("Sell Signal", ImGuiTableColumnFlags_WidthFixed, 112.0f);
        ImGui::TableSetupColumn("Signal", ImGuiTableColumnFlags_WidthFixed, 128.0f);
        ImGui::TableSetupColumn("Last Refresh", ImGuiTableColumnFlags_WidthFixed, 166.0f);
        ImGui::TableSetupColumn("Source", ImGuiTableColumnFlags_WidthFixed, 144.0f);
        ImGui::TableSetupColumn("Actions", ImGuiTableColumnFlags_WidthFixed, 132.0f);
        ImGui::TableHeadersRow();

        for (const WatchlistItem& item : selectedItems) {
            if (!matchesFilter(item, searchText_)) {
                continue;
            }

            ++visibleRows;
            ImGui::TableNextRow();
            ImGui::PushID(item.id);
            ImGui::TableNextColumn();
            ImGui::Text("%s", item.ticker.c_str());
            ImGui::TableNextColumn();
            ImGui::Text("%s", item.assetName.c_str());
            ImGui::TextColored(UiTheme::TextMuted, "%s", item.assetType.c_str());
            ImGui::TableNextColumn();
            drawPriorityBadge(item.priority);
            ImGui::TableNextColumn();
            ImGui::Text("%s", currentPriceText(item.currentPrice).c_str());
            ImGui::TableNextColumn();
            ImGui::TextColored(UiTheme::Gain, "%s", priceOrNotSet(item.buySignalPrice).c_str());
            ImGui::TableNextColumn();
            ImGui::TextColored(UiTheme::Loss, "%s", priceOrNotSet(item.sellSignalPrice).c_str());
            ImGui::TableNextColumn();
            drawSignalBadge(item);
            ImGui::TableNextColumn();
            ImGui::TextColored(UiTheme::TextMuted, "%s", emptyIfBlank(item.lastPriceRefreshAt));
            ImGui::TableNextColumn();
            ImGui::TextColored(UiTheme::TextMuted, "%s", emptyIfBlank(item.priceSource));
            ImGui::TableNextColumn();
            const std::string editButtonId = "Edit##edit_button_" + std::to_string(item.id);
            if (ImGui::SmallButton(editButtonId.c_str())) {
                openEdit(item);
                openEditorPopup_ = true;
            }
            ImGui::SameLine();
            const std::string deleteButtonId = "Delete##delete_button_" + std::to_string(item.id);
            if (ImGui::SmallButton(deleteButtonId.c_str())) {
                deleteId_ = item.id;
                deleteName_ = item.ticker + " - " + item.assetName;
                deletePopupId_ = watchlistDeletePopupId(item.id);
                openDeletePopup_ = true;
            }
            ImGui::PopID();
        }

        ImGui::EndTable();
        if (visibleRows == 0) {
            ImGui::TextColored(UiTheme::MutedText, "No watchlist items match the current search.");
        }
    }

    ImGui::EndChild();
}

void WatchlistView::openWatchlistCreate(const AppState& state)
{
    watchlistDraft_ = Watchlist {};
    watchlistDraft_.name = "New Watchlist";
    watchlistDraft_.sortOrder = nextSortOrder(state);
    watchlistDraft_.isActive = true;
    editingWatchlist_ = false;
    watchlistEditorPopupId_ = NewWatchlistGroupEditorPopup;
    watchlistFormError_.clear();
}

void WatchlistView::openWatchlistEdit(const Watchlist& watchlist)
{
    watchlistDraft_ = watchlist;
    editingWatchlist_ = true;
    watchlistEditorPopupId_ = watchlistGroupEditorPopupId(watchlist.id);
    watchlistFormError_.clear();
}

void WatchlistView::drawWatchlistEditor(AppState& state, WatchlistRepository& repository, const std::function<void()>& reloadData)
{
    const char* popupId = watchlistEditorPopupId_.empty() ? WatchlistGroupEditorPopup : watchlistEditorPopupId_.c_str();
    if (!ImGui::BeginPopupModal(popupId, nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        return;
    }

    ImGui::Text("%s", editingWatchlist_ ? "Edit watchlist" : "Create watchlist");
    ImGui::TextColored(UiTheme::TextMuted, "Watchlist groups are local organization only.");
    ImGui::Separator();
    ImGui::SetNextItemWidth(420.0f);
    ImGui::InputText("Name", &watchlistDraft_.name);
    ImGui::InputTextMultiline("Description", &watchlistDraft_.description, ImVec2(420.0f, 84.0f));
    ImGui::TextColored(UiTheme::TextMuted, "Use the manager table to activate, deactivate, or reorder watchlists.");

    UiTheme::formError(watchlistFormError_);

    if (ImGui::Button(editingWatchlist_ ? "Save Changes" : "Create Watchlist", ImVec2(150.0f, 0.0f))) {
        std::string error;
        const bool saved = editingWatchlist_
            ? repository.updateWatchlist(watchlistDraft_, error)
            : repository.createWatchlist(watchlistDraft_, error);
        if (saved) {
            if (!editingWatchlist_) {
                selectedWatchlistId_ = watchlistDraft_.id;
            }
            reloadData();
            state.setStatus(editingWatchlist_ ? "Watchlist updated." : "Watchlist created.");
            ImGui::CloseCurrentPopup();
        } else {
            watchlistFormError_ = error;
        }
    }

    ImGui::SameLine();
    if (ImGui::Button("Cancel", ImVec2(100.0f, 0.0f))) {
        ImGui::CloseCurrentPopup();
    }

    ImGui::EndPopup();
}

void WatchlistView::drawWatchlistDeactivateConfirmation(AppState& state, WatchlistRepository& repository, const std::function<void()>& reloadData)
{
    const char* popupId = deactivateWatchlistPopupId_.empty() ? DeactivateWatchlistPopup : deactivateWatchlistPopupId_.c_str();
    if (!ImGui::BeginPopupModal(popupId, nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        return;
    }

    ImGui::Text("Deactivate watchlist?");
    ImGui::TextColored(UiTheme::MutedText, "%s", deactivateWatchlistName_.c_str());
    if (deactivateWatchlistItemCount_ > 0) {
        ImGui::TextWrapped("This watchlist contains items. Deactivate it and hide its items?");
    } else {
        ImGui::TextWrapped("This will hide the watchlist from normal lists. Its items are not deleted.");
    }

    if (ImGui::Button("Deactivate", ImVec2(110.0f, 0.0f))) {
        std::string error;
        if (repository.deactivateWatchlist(deactivateWatchlistId_, error)) {
            if (selectedWatchlistId_ == deactivateWatchlistId_) {
                selectedWatchlistId_ = 0;
            }
            reloadData();
            ensureSelectedWatchlist(state);
            state.setStatus("Watchlist deactivated. Items were hidden, not deleted.");
            ImGui::CloseCurrentPopup();
        } else {
            state.setStatus("Could not deactivate watchlist: " + error, true);
            ImGui::CloseCurrentPopup();
        }
    }

    ImGui::SameLine();
    if (ImGui::Button("Cancel", ImVec2(100.0f, 0.0f))) {
        ImGui::CloseCurrentPopup();
    }

    ImGui::EndPopup();
}

void WatchlistView::openCreate(int watchlistId)
{
    draft_ = WatchlistItem {};
    draft_.watchlistId = watchlistId;
    draft_.assetType = "Stock";
    draft_.priority = "Medium";
    draft_.signalStatus = "None";
    editing_ = false;
    editorPopupId_ = NewWatchlistEditorPopup;
    formError_.clear();
}

void WatchlistView::openEdit(const WatchlistItem& item)
{
    draft_ = item;
    editing_ = true;
    editorPopupId_ = watchlistEditorPopupId(item.id);
    formError_.clear();
}

void WatchlistView::drawEditor(AppState& state, WatchlistRepository& repository, const std::function<void()>& reloadData)
{
    const char* popupId = editorPopupId_.empty() ? WatchlistEditorPopup : editorPopupId_.c_str();
    if (!ImGui::BeginPopupModal(popupId, nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        return;
    }

    const std::vector<Watchlist> active = activeWatchlists(state);

    ImGui::Text("%s", editing_ ? "Edit watchlist item" : "Add watchlist item");
    ImGui::TextColored(UiTheme::TextMuted, "Price signals are personal tracking levels only.");
    ImGui::Separator();

    if (active.empty()) {
        ImGui::TextColored(UiTheme::TextWarning, "Create an active watchlist before saving items.");
    } else {
        const std::string selectedName = watchlistDisplayName(active, draft_.watchlistId);
        ImGui::SetNextItemWidth(260.0f);
        if (ImGui::BeginCombo("Watchlist", selectedName.c_str())) {
            for (const Watchlist& watchlist : active) {
                const bool selected = draft_.watchlistId == watchlist.id;
                if (ImGui::Selectable(watchlist.name.c_str(), selected)) {
                    draft_.watchlistId = watchlist.id;
                }
                if (selected) {
                    ImGui::SetItemDefaultFocus();
                }
            }
            ImGui::EndCombo();
        }
    }

    ImGui::InputText("Ticker", &draft_.ticker, ImGuiInputTextFlags_CharsUppercase | ImGuiInputTextFlags_CharsNoBlank);
    ImGui::InputText("Asset name", &draft_.assetName);
    drawStringCombo("Asset type", draft_.assetType, std::array {
        "Stock",
        "ETF",
        "Mutual Fund",
        "Bond",
        "Crypto",
        "Other",
    });
    drawStringCombo("Priority", draft_.priority, std::array {
        "High",
        "Medium",
        "Low",
    });
    ImGui::InputDouble("Buy Signal Price", &draft_.buySignalPrice, 0.0, 0.0, "%.4f");
    ImGui::InputDouble("Sell Signal Price", &draft_.sellSignalPrice, 0.0, 0.0, "%.4f");
    ImGui::InputDouble("Current price", &draft_.currentPrice, 0.0, 0.0, "%.4f");
    ImGui::InputTextMultiline("Reason watching", &draft_.reasonWatching, ImVec2(440.0f, 70.0f));
    ImGui::InputTextMultiline("Risk notes", &draft_.riskNotes, ImVec2(440.0f, 70.0f));

    draft_.signalStatus = WatchlistSignalService::calculateSignalStatus(draft_.currentPrice, draft_.buySignalPrice, draft_.sellSignalPrice);

    ImGui::Separator();
    ImGui::TextColored(signalColor(draft_.signalStatus), "Current signal: %s", draft_.signalStatus.c_str());
    if (draft_.lastPriceRefreshAt.empty()) {
        ImGui::TextColored(UiTheme::TextMuted, "Last refresh: N/A");
    } else {
        ImGui::TextColored(UiTheme::TextMuted, "Last refresh: %s from %s", draft_.lastPriceRefreshAt.c_str(), emptyIfBlank(draft_.priceSource));
    }
    if (hasSignalLevelError(draft_)) {
        ImGui::TextColored(UiTheme::TextWarning, "Buy signal price should be lower than sell signal price when both are entered.");
    }

    UiTheme::formError(formError_);

    if (ImGui::Button(editing_ ? "Save Changes" : "Create Item", ImVec2(140.0f, 0.0f))) {
        if (active.empty()) {
            formError_ = "Create an active watchlist before saving items.";
        } else if (hasSignalLevelError(draft_)) {
            formError_ = "Buy signal price must be lower than sell signal price, or leave one side blank.";
        } else {
            draft_.targetBuyPrice = draft_.buySignalPrice;
            draft_.signalStatus = WatchlistSignalService::calculateSignalStatus(draft_.currentPrice, draft_.buySignalPrice, draft_.sellSignalPrice);

            std::string error;
            const bool saved = editing_ ? repository.update(draft_, error) : repository.create(draft_, error);
            if (saved) {
                selectedWatchlistId_ = draft_.watchlistId;
                reloadData();
                state.setStatus(editing_ ? "Watchlist item updated." : "Watchlist item created.");
                ImGui::CloseCurrentPopup();
            } else {
                formError_ = error;
            }
        }
    }

    ImGui::SameLine();
    if (ImGui::Button("Cancel", ImVec2(100.0f, 0.0f))) {
        ImGui::CloseCurrentPopup();
    }

    ImGui::EndPopup();
}

void WatchlistView::drawDeleteConfirmation(AppState& state, WatchlistRepository& repository, const std::function<void()>& reloadData)
{
    const char* popupId = deletePopupId_.empty() ? DeleteWatchlistPopup : deletePopupId_.c_str();
    if (!ImGui::BeginPopupModal(popupId, nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        return;
    }

    ImGui::Text("Delete watchlist item?");
    ImGui::TextColored(UiTheme::MutedText, "%s", deleteName_.c_str());
    ImGui::TextWrapped("This removes the local watchlist item. It does not affect holdings, accounts, brokerage records, or money movement.");

    if (ImGui::Button("Delete", ImVec2(100.0f, 0.0f))) {
        std::string error;
        if (repository.remove(deleteId_, error)) {
            reloadData();
            state.setStatus("Watchlist item deleted.");
            ImGui::CloseCurrentPopup();
        } else {
            state.setStatus("Could not delete watchlist item: " + error, true);
            ImGui::CloseCurrentPopup();
        }
    }

    ImGui::SameLine();
    if (ImGui::Button("Cancel", ImVec2(100.0f, 0.0f))) {
        ImGui::CloseCurrentPopup();
    }

    ImGui::EndPopup();
}

void WatchlistView::refreshPrices(AppState& state,
    WatchlistRepository& repository,
    MarketDataService& marketDataService,
    const std::vector<WatchlistItem>& items,
    const std::string& watchlistName,
    const std::function<void()>& reloadData)
{
    std::string error;
    WatchlistPriceRefreshStatus refreshStatus = WatchlistSignalService::refreshPrices(items, marketDataService, repository, error);
    reloadData();
    state.watchlistPriceRefreshStatus = refreshStatus;

    if (refreshStatus.refreshedSymbols > 0) {
        state.setStatus(watchlistName + " refreshed: " + std::to_string(refreshStatus.refreshedSymbols) + " updated, " +
            std::to_string(refreshStatus.failedSymbols) + " failed. Last refresh: " + refreshStatus.lastRefreshedAt + ". Source: " +
            (refreshStatus.cachedSymbols > 0 ? "Cached " : "") + refreshStatus.provider + ".");
    } else {
        state.setStatus(error.empty() ? watchlistName + " refresh did not update any symbols." : error, true);
    }
}

void WatchlistView::drawPriorityBadge(const std::string& priority) const
{
    ImVec4 color = UiTheme::Amber;
    if (priority == "High") {
        color = UiTheme::Loss;
    } else if (priority == "Low") {
        color = UiTheme::MutedText;
    }

    ImGui::TextColored(color, "%s", priority.c_str());
}

void WatchlistView::drawSignalBadge(const WatchlistItem& item)
{
    const std::string status = WatchlistSignalService::calculateSignalStatus(item.currentPrice, item.buySignalPrice, item.sellSignalPrice);
    const ImVec4 color = signalColor(status);

    if (isActionableSignal(status)) {
        ImGui::PushStyleColor(ImGuiCol_Text, color);
        if (ImGui::SmallButton((status + "##signal_badge_" + std::to_string(item.id)).c_str())) {
            signalNoticeTicker_ = item.ticker;
            signalNoticeStatus_ = status;
            openSignalNoticePopup_ = true;
        }
        ImGui::PopStyleColor();
    } else {
        ImGui::TextColored(color, "%s", status.c_str());
    }
}

void WatchlistView::drawSignalNoticePopup()
{
    if (!ImGui::BeginPopupModal(SignalNoticePopup, nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        return;
    }

    ImGui::Text("Watchlist Price Signal");
    ImGui::Separator();
    ImGui::TextColored(signalColor(signalNoticeStatus_), "%s", signalNoticeStatus_.c_str());
    ImGui::TextColored(UiTheme::TextMuted, "%s", signalNoticeTicker_.empty() ? "Selected watchlist item" : signalNoticeTicker_.c_str());
    ImGui::Spacing();
    ImGui::TextWrapped("This is your saved price signal, not financial advice or a trade action.");
    ImGui::TextWrapped("Investor Command Center does not place trades, move money, connect to brokerage accounts, or recommend buy/sell decisions.");
    ImGui::Spacing();
    if (ImGui::Button("Close", ImVec2(100.0f, 0.0f))) {
        ImGui::CloseCurrentPopup();
    }

    ImGui::EndPopup();
}

void WatchlistView::ensureSelectedWatchlist(const AppState& state)
{
    const std::vector<Watchlist> active = activeWatchlists(state);
    if (active.empty()) {
        selectedWatchlistId_ = 0;
        return;
    }

    const Watchlist* selected = findWatchlist(active, selectedWatchlistId_);
    if (selected == nullptr) {
        selectedWatchlistId_ = active.front().id;
    }
}

bool WatchlistView::hasSignalLevelError(const WatchlistItem& item) const
{
    return item.buySignalPrice > 0.0 && item.sellSignalPrice > 0.0 && item.buySignalPrice >= item.sellSignalPrice;
}
