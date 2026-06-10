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

namespace {

constexpr const char* WatchlistEditorPopup = "Watchlist Editor";
constexpr const char* DeleteWatchlistPopup = "Delete Watchlist Item";
constexpr const char* SignalNoticePopup = "Watchlist Price Signal";
constexpr const char* NewWatchlistEditorPopup = "Watchlist Editor###watchlist_edit_popup_new";

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

}

void WatchlistView::render(AppState& state,
    WatchlistRepository& repository,
    MarketDataService& marketDataService,
    const std::function<void()>& reloadData)
{
    UiTheme::sectionHeading("Watchlist", "Manual ideas under observation, without recommendations.");

    ImGui::TextWrapped("Signals are based on your saved prices and refreshed market data. They are not recommendations, trading advice, or brokerage actions.");

    const WatchlistSummary summary = PortfolioCalculator::calculateWatchlist(state.watchlist);
    ImGui::TextColored(UiTheme::Loss, "High: %d", summary.highPriority);
    ImGui::SameLine();
    ImGui::TextColored(UiTheme::Amber, "Medium: %d", summary.mediumPriority);
    ImGui::SameLine();
    ImGui::TextColored(UiTheme::MutedText, "Low: %d", summary.lowPriority);

    if (ImGui::Button("Add Watchlist Item")) {
        openCreate();
        openEditorPopup_ = true;
    }
    ImGui::SameLine();
    if (ImGui::Button("Refresh Watchlist Prices")) {
        refreshPrices(state, repository, marketDataService, reloadData);
    }
    ImGui::SameLine();
    ImGui::SetNextItemWidth(300.0f);
    ImGui::InputTextWithHint("##WatchlistSearch", "Search watchlist", &searchText_);

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
    if (state.watchlist.empty()) {
        UiTheme::emptyState("No watchlist items yet", "Add assets you want to monitor manually.");
    } else if (ImGui::BeginTable("WatchlistTable", 9, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable | ImGuiTableFlags_Reorderable | ImGuiTableFlags_Hideable | ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_ScrollX)) {
        ImGui::TableSetupColumn("Ticker", ImGuiTableColumnFlags_WidthFixed, 76.0f);
        ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthStretch, 1.3f);
        ImGui::TableSetupColumn("Current Price", ImGuiTableColumnFlags_WidthFixed, 112.0f);
        ImGui::TableSetupColumn("Buy Signal", ImGuiTableColumnFlags_WidthFixed, 112.0f);
        ImGui::TableSetupColumn("Sell Signal", ImGuiTableColumnFlags_WidthFixed, 112.0f);
        ImGui::TableSetupColumn("Signal", ImGuiTableColumnFlags_WidthFixed, 128.0f);
        ImGui::TableSetupColumn("Last Refresh", ImGuiTableColumnFlags_WidthFixed, 166.0f);
        ImGui::TableSetupColumn("Source", ImGuiTableColumnFlags_WidthFixed, 144.0f);
        ImGui::TableSetupColumn("Actions", ImGuiTableColumnFlags_WidthFixed, 132.0f);
        ImGui::TableHeadersRow();

        for (const WatchlistItem& item : state.watchlist) {
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

    drawEditor(state, repository, reloadData);
    drawDeleteConfirmation(state, repository, reloadData);
    drawSignalNoticePopup();
}

void WatchlistView::openCreate()
{
    draft_ = WatchlistItem {};
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

    ImGui::Text("%s", editing_ ? "Edit watchlist item" : "Add watchlist item");
    ImGui::TextColored(UiTheme::TextMuted, "Price signals are personal tracking levels only.");
    ImGui::Separator();

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
        if (hasSignalLevelError(draft_)) {
            formError_ = "Buy signal price must be lower than sell signal price, or leave one side blank.";
        } else {
            draft_.targetBuyPrice = draft_.buySignalPrice;
            draft_.signalStatus = WatchlistSignalService::calculateSignalStatus(draft_.currentPrice, draft_.buySignalPrice, draft_.sellSignalPrice);

            std::string error;
            const bool saved = editing_ ? repository.update(draft_, error) : repository.create(draft_, error);
            if (saved) {
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

void WatchlistView::refreshPrices(AppState& state, WatchlistRepository& repository, MarketDataService& marketDataService, const std::function<void()>& reloadData)
{
    std::string error;
    WatchlistPriceRefreshStatus refreshStatus = WatchlistSignalService::refreshPrices(state.watchlist, marketDataService, repository, error);
    reloadData();
    state.watchlistPriceRefreshStatus = refreshStatus;

    if (refreshStatus.refreshedSymbols > 0) {
        state.setStatus("Watchlist prices refreshed: " + std::to_string(refreshStatus.refreshedSymbols) + " updated, " +
            std::to_string(refreshStatus.failedSymbols) + " failed.");
    } else {
        state.setStatus(error.empty() ? "Watchlist price refresh did not update any symbols." : error, true);
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

bool WatchlistView::hasSignalLevelError(const WatchlistItem& item) const
{
    return item.buySignalPrice > 0.0 && item.sellSignalPrice > 0.0 && item.buySignalPrice >= item.sellSignalPrice;
}
