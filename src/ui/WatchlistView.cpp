// SPDX-License-Identifier: MIT
#include "ui/WatchlistView.hpp"

#include "app/AppState.hpp"
#include "models/SignalRules.hpp"
#include "repositories/WatchlistRepository.hpp"
#include "services/MarketDataService.hpp"
#include "services/PortfolioCalculator.hpp"
<<<<<<< Updated upstream
=======
#include "services/TechnicalIndicatorService.hpp"
#include "services/WatchlistIndicatorDisplay.hpp"
>>>>>>> Stashed changes
#include "services/WatchlistSignalService.hpp"
#include "ui/UiTheme.hpp"
#include "util/Money.hpp"

#include <imgui.h>
#include <imgui_stdlib.h>

#include <algorithm>
#include <array>
#include <cctype>
<<<<<<< Updated upstream
=======
#include <cmath>
#include <map>
#include <optional>
#include <set>
#include <sstream>
>>>>>>> Stashed changes
#include <string>

namespace {

constexpr const char* WatchlistEditorPopup = "Watchlist Editor";
constexpr const char* DeleteWatchlistPopup = "Delete Watchlist Item";
constexpr const char* SignalNoticePopup = "Watchlist Price Signal";
constexpr const char* NewWatchlistEditorPopup = "Watchlist Editor###watchlist_edit_popup_new";

<<<<<<< Updated upstream
=======
void showGlassTooltip(const std::string& text);
std::string momentumColumnLabel(int lookbackSessions);

std::string watchlistGroupEditorPopupId(int watchlistId)
{
    return "Watchlist Manager###watchlist_group_edit_popup_" + std::to_string(watchlistId);
}

std::string watchlistDeactivatePopupId(int watchlistId)
{
    return "Deactivate Watchlist###watchlist_group_deactivate_popup_" + std::to_string(watchlistId);
}

>>>>>>> Stashed changes
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

<<<<<<< Updated upstream
=======
std::string signedMoney(double value)
{
    return std::string(value > 0.0 ? "+" : "") + Money::format(value);
}

std::string quoteChangeText(const std::optional<MarketQuote>& quote)
{
    if (!quote.has_value()) {
        return "N/A";
    }

    if (quote->priceChangeDollar.has_value() && quote->priceChangePercent.has_value()) {
        return signedMoney(*quote->priceChangeDollar) + " (" + Money::formatPercent(*quote->priceChangePercent, true) + ")";
    }

    if (quote->priceChangeDollar.has_value()) {
        return signedMoney(*quote->priceChangeDollar);
    }

    if (quote->priceChangePercent.has_value()) {
        return Money::formatPercent(*quote->priceChangePercent, true);
    }

    return "N/A";
}

ImVec4 quoteChangeColor(const std::optional<MarketQuote>& quote)
{
    if (!quote.has_value()) {
        return UiTheme::TextMuted;
    }
    if (quote->priceChangeDollar.has_value()) {
        return UiTheme::moneyColor(*quote->priceChangeDollar);
    }
    if (quote->priceChangePercent.has_value()) {
        return UiTheme::moneyColor(*quote->priceChangePercent);
    }
    return UiTheme::TextMuted;
}

std::string compactLargeNumber(const std::optional<double>& value)
{
    if (!value.has_value()) {
        return "N/A";
    }

    const double absoluteValue = std::fabs(*value);
    double scaled = *value;
    const char* suffix = "";
    if (absoluteValue >= 1'000'000'000.0) {
        scaled = *value / 1'000'000'000.0;
        suffix = "B";
    } else if (absoluteValue >= 1'000'000.0) {
        scaled = *value / 1'000'000.0;
        suffix = "M";
    } else if (absoluteValue >= 1'000.0) {
        scaled = *value / 1'000.0;
        suffix = "K";
    }

    return Money::formatNumber(scaled, absoluteValue >= 1'000.0 ? 1 : 0) + suffix;
}

std::string volumeText(const std::optional<TechnicalIndicatorSnapshot>& snapshot)
{
    if (!snapshot.has_value()) {
        return "N/A";
    }

    const std::string relative = snapshot->volumeVsAvg20Percent.has_value()
        ? Money::formatPercent(*snapshot->volumeVsAvg20Percent, true) + " vs 20D Avg"
        : "N/A vs 20D Avg";
    return compactLargeNumber(snapshot->latestVolume) + " / " + relative;
}

>>>>>>> Stashed changes
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
<<<<<<< Updated upstream
    return status == "Buy Signal" || status == "Sell Signal" || status == "Check Signals";
=======
    std::string error;
    return technicalIndicatorService.cachedSnapshot(item.ticker, "Yahoo Finance", error);
}

std::optional<WatchlistMomentum7D> momentumFor(TechnicalIndicatorService& technicalIndicatorService, const WatchlistItem& item, int lookbackSessions)
{
    std::string error;
    return technicalIndicatorService.watchlistMomentum(item.ticker, "Yahoo Finance", lookbackSessions, error);
}

std::optional<MarketQuote> cachedQuoteFor(MarketDataService& marketDataService, const WatchlistItem& item)
{
    std::string error;
    return marketDataService.cachedQuote(item.ticker, error);
}

ImVec4 macdSignalColor(const std::optional<TechnicalIndicatorSnapshot>& snapshot)
{
    return snapshot.has_value() && snapshot->macdSignal.has_value() ? UiTheme::moneyColor(*snapshot->macdSignal) : UiTheme::TextMuted;
}

ImVec4 momentumColor(const std::optional<WatchlistMomentum7D>& momentum)
{
    if (!momentum.has_value()) {
        return UiTheme::TextMuted;
    }
    if (momentum->rising) {
        return UiTheme::Gain;
    }
    if (momentum->flat) {
        return UiTheme::ElectricCyan;
    }
    return UiTheme::Loss;
}

std::string rulesRsiRangeText(const SignalRules& signalRules)
{
    return Money::formatNumber(signalRules.rsiBuyMin, 1) + "-" + Money::formatNumber(signalRules.rsiBuyMax, 1);
}

std::string rulesMacdHistogramText(const SignalRules& signalRules)
{
    return "MACD histogram > " + Money::formatNumber(signalRules.macdHistogramMin, 4);
}

std::string rsiStatusText(const std::optional<TechnicalIndicatorSnapshot>& snapshot, const SignalRules& signalRules)
{
    if (!snapshot.has_value() || !snapshot->rsi14.has_value()) {
        return "No RSI data";
    }
    if (*snapshot->rsi14 < signalRules.rsiBuyMin) {
        return "Below configured RSI range";
    }
    if (*snapshot->rsi14 > signalRules.rsiBuyMax) {
        return "Above configured RSI range";
    }
    return "Inside configured RSI range";
}

ImVec4 rsiColor(const std::optional<TechnicalIndicatorSnapshot>& snapshot, const SignalRules& signalRules)
{
    if (!snapshot.has_value() || !snapshot->rsi14.has_value()) {
        return UiTheme::TextMuted;
    }
    if (*snapshot->rsi14 < signalRules.rsiBuyMin) {
        return UiTheme::Amber;
    }
    if (*snapshot->rsi14 > signalRules.rsiBuyMax) {
        return UiTheme::Loss;
    }
    return UiTheme::ElectricCyan;
}

std::string rsiTooltip(const std::optional<TechnicalIndicatorSnapshot>& snapshot, const SignalRules& signalRules)
{
    if (!snapshot.has_value() || !snapshot->rsi14.has_value()) {
        return "RSI: no cached RSI value. Refresh history to calculate technical indicators.";
    }

    return "RSI: " + Money::formatNumber(*snapshot->rsi14, 1) + "\n" +
        rsiStatusText(snapshot, signalRules) + "\n" +
        "Buy confirmation RSI range: " + rulesRsiRangeText(signalRules) + ".";
}

std::string macdSignalTooltip(const std::optional<TechnicalIndicatorSnapshot>& snapshot, const SignalRules& signalRules)
{
    if (!snapshot.has_value() || !snapshot->macdSignal.has_value()) {
        return "MACD Signal: no cached signal-line value. Refresh history to calculate technical indicators.";
    }

    std::string detail = "MACD Signal line: " + Money::formatNumber(*snapshot->macdSignal, 4) + "\n";
    if (snapshot->macdHistogram.has_value()) {
        detail += "MACD histogram: " + Money::formatNumber(*snapshot->macdHistogram, 4) + "\n";
    }
    detail += "Buy confirmation uses " + rulesMacdHistogramText(signalRules) + ", not the signal-line value.";
    return detail;
}

std::string momentumTooltip(const std::optional<WatchlistMomentum7D>& momentum, int lookbackSessions)
{
    const std::string label = momentumColumnLabel(lookbackSessions);
    if (!momentum.has_value()) {
        return label + ": insufficient historical close data.\nMomentum is display-only and does not change Buy/Sell/Hold.";
    }

    const std::string status = momentum->flat ? "Flat" : (momentum->rising ? "Rising" : "Falling");
    return label + ": " + status + " " + Money::formatPercent(momentum->percent, true) + "\n" +
        "Latest close: " + Money::format(momentum->latestClose) + "\n" +
        "Comparison close: " + Money::format(momentum->comparisonClose) + "\n" +
        "Momentum is display-only and does not change Buy/Sell/Hold.";
}

std::string signalBadgeLabel(const std::string& signal)
{
    if (signal == "Buy") {
        return "BUY";
    }
    if (signal == "Sell") {
        return "SELL";
    }
    return "HOLD";
}

std::string signalDetailText(
    const WatchlistItem& item,
    const std::optional<TechnicalIndicatorSnapshot>& indicators,
    const std::optional<WatchlistMomentum7D>& momentum,
    const WatchlistSignalResult& signal,
    const SignalRules& signalRules)
{
    std::ostringstream stream;
    stream << "Final signal: " << signal.signal << "\n";
    if (!signal.reasonText.empty()) {
        stream << signal.reasonText << "\n";
    }
    stream << "\nSymbol: " << item.ticker;
    if (!item.assetName.empty()) {
        stream << " - " << item.assetName;
    }
    stream << "\nPrice: " << currentPriceText(item.currentPrice);
    stream << "\nBuy Level: " << priceOrNotSet(item.buySignalPrice);
    stream << "\nSell Level: " << priceOrNotSet(item.sellSignalPrice);
    stream << "\nRSI: " << WatchlistIndicatorDisplay::rsiDisplayText(indicators) << " (" << rsiStatusText(indicators, signalRules) << ")";
    stream << "\nMACD Signal: " << WatchlistIndicatorDisplay::macdSignalDisplayText(indicators);
    if (indicators.has_value() && indicators->macdHistogram.has_value()) {
        stream << "\nMACD Histogram: " << Money::formatNumber(*indicators->macdHistogram, 4);
    }
    stream << "\n" << momentumColumnLabel(signalRules.momentumLookbackSessions) << ": " << WatchlistIndicatorDisplay::momentumDisplayText(momentum);
    stream << "\n\nRules: Buy when price is at or below Buy Level, RSI is within " << rulesRsiRangeText(signalRules) <<
        ", and " << rulesMacdHistogramText(signalRules) << ". Sell when price is at or above Sell Level. Otherwise Hold.";
    stream << "\nMomentum is display-only.";
    return stream.str();
}

void drawRightAlignedTechnical(const std::string& value, ImVec4 color, const std::string& tooltip)
{
    UiTheme::textRightAligned(value.c_str(), color);
    if (ImGui::IsItemHovered() && !tooltip.empty()) {
        showGlassTooltip(tooltip);
    }
}

void showGlassTooltip(const std::string& text)
{
    constexpr float TooltipMinWidth = 240.0f;
    constexpr float TooltipMaxWidth = 360.0f;

    ImGui::PushStyleColor(ImGuiCol_PopupBg, UiTheme::withAlpha(UiTheme::GlassPanel, 0.96f));
    ImGui::PushStyleColor(ImGuiCol_Border, UiTheme::withAlpha(UiTheme::ElectricCyan, 0.42f));
    ImGui::PushStyleVar(ImGuiStyleVar_PopupBorderSize, 1.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(10.0f, 8.0f));
    ImGui::SetNextWindowSizeConstraints(
        ImVec2(TooltipMinWidth, 0.0f),
        ImVec2(TooltipMaxWidth, ImGui::GetIO().DisplaySize.y));
    ImGui::BeginTooltip();
    const float wrapWidth = TooltipMaxWidth - ImGui::GetStyle().WindowPadding.x * 2.0f;
    ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + wrapWidth);
    ImGui::TextUnformatted(text.c_str());
    ImGui::PopTextWrapPos();
    ImGui::EndTooltip();
    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor(2);
}

std::string momentumColumnLabel(int lookbackSessions)
{
    return "Momentum " + std::to_string(lookbackSessions) + "D";
}

void sortWatchlistItemsBySignal(std::vector<WatchlistItem>& items, TechnicalIndicatorService& technicalIndicatorService, const SignalRules& signalRules)
{
    std::map<int, WatchlistSignalResult> signalResults;
    for (const WatchlistItem& item : items) {
        signalResults[item.id] = WatchlistSignalService::calculateSignal(item, cachedIndicatorsFor(technicalIndicatorService, item), signalRules);
    }

    std::stable_sort(items.begin(), items.end(), [&signalResults](const WatchlistItem& left, const WatchlistItem& right) {
        const int leftSignalRank = WatchlistSignalService::signalSortRank(signalResults[left.id].signal);
        const int rightSignalRank = WatchlistSignalService::signalSortRank(signalResults[right.id].signal);
        if (leftSignalRank != rightSignalRank) {
            return leftSignalRank < rightSignalRank;
        }

        const int leftPriorityRank = prioritySortRank(left.priority);
        const int rightPriorityRank = prioritySortRank(right.priority);
        if (leftPriorityRank != rightPriorityRank) {
            return leftPriorityRank < rightPriorityRank;
        }

        return left.ticker < right.ticker;
    });
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

std::vector<WatchlistItem> itemsForWatchlist(const AppState& state, int watchlistId, TechnicalIndicatorService& technicalIndicatorService)
{
    std::vector<WatchlistItem> items;
    for (const WatchlistItem& item : state.watchlist) {
        if (item.watchlistId == watchlistId) {
            items.push_back(item);
        }
    }
    sortWatchlistItemsBySignal(items, technicalIndicatorService, state.signalRules);
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
>>>>>>> Stashed changes
}

}

void WatchlistView::render(AppState& state,
    WatchlistRepository& repository,
    MarketDataService& marketDataService,
    const std::function<void()>& reloadData)
{
    UiTheme::sectionHeading("Watchlist", "Manual ideas under observation, without recommendations.");

    ImGui::TextWrapped("Signals are based on your saved prices and refreshed market data. They are not recommendations, trading advice, or brokerage actions.");

<<<<<<< Updated upstream
    const WatchlistSummary summary = PortfolioCalculator::calculateWatchlist(state.watchlist);
=======
    drawWatchlistManager(state, repository, reloadData);
    ImGui::Spacing();
    drawWatchlistItems(state, repository, marketDataService, technicalIndicatorService, reloadData);

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
    UiTheme::pushPanelStyle();
    ImGui::BeginChild("WatchlistManagerPanel", ImVec2(0.0f, 250.0f), true);
    ImGui::TextColored(UiTheme::TextPrimary, "Watchlist Manager");
    ImGui::TextWrapped("Create and organize named local watchlists. Deactivated watchlists are hidden from normal watchlist and sidebar views.");
    ImGui::Separator();

    UiTheme::pushButtonStyle(UiTheme::NeonMagenta);
    if (ImGui::Button("Create Watchlist", ImVec2(150.0f, 0.0f))) {
        openWatchlistCreate(state);
        openWatchlistEditorPopup_ = true;
    }
    UiTheme::popButtonStyle();
    ImGui::SameLine();
    ImGui::Checkbox("Show inactive", &showInactiveWatchlists_);

    std::vector<Watchlist> visible = visibleWatchlists(state, showInactiveWatchlists_);
    if (visible.empty()) {
        UiTheme::emptyState("No watchlists yet", "Create a watchlist to start grouping local ticker ideas.");
        ImGui::EndChild();
        UiTheme::popPanelStyle();
        return;
    }

    ImGui::Spacing();
    UiTheme::pushTableStyle();
    if (ImGui::BeginTable("WatchlistManagerTable", 8, ImGuiTableFlags_BordersInnerH | ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable | ImGuiTableFlags_SizingStretchProp)) {
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
                UiTheme::badge("Selected", UiTheme::NeonMagenta);
            }
            ImGui::TableNextColumn();
            ImGui::TextWrapped("%s", watchlist.description.empty() ? "N/A" : watchlist.description.c_str());
            ImGui::TableNextColumn();
            UiTheme::badge(watchlist.isActive ? "Active" : "Inactive", watchlist.isActive ? UiTheme::Gain : UiTheme::TextMuted);
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
                UiTheme::textRightAligned(std::to_string(itemCount).c_str());
            } else {
                UiTheme::textRightAligned("N/A", UiTheme::TextWarning);
            }
            ImGui::TableNextColumn();
            if (index == 0) {
                ImGui::BeginDisabled();
            }
            UiTheme::pushButtonStyle(UiTheme::ElectricCyan);
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
            UiTheme::popButtonStyle();
            if (index == 0) {
                ImGui::EndDisabled();
            }
            ImGui::SameLine();
            if (index + 1 >= visible.size()) {
                ImGui::BeginDisabled();
            }
            UiTheme::pushButtonStyle(UiTheme::ElectricCyan);
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
            UiTheme::popButtonStyle();
            if (index + 1 >= visible.size()) {
                ImGui::EndDisabled();
            }
            ImGui::TableNextColumn();
            if (watchlist.isActive) {
                UiTheme::pushButtonStyle(UiTheme::Loss);
                if (ImGui::SmallButton("Deactivate")) {
                    deactivateWatchlistId_ = watchlist.id;
                    deactivateWatchlistName_ = watchlist.name;
                    deactivateWatchlistItemCount_ = itemCount;
                    deactivateWatchlistPopupId_ = watchlistDeactivatePopupId(watchlist.id);
                    openDeactivateWatchlistPopup_ = true;
                }
                UiTheme::popButtonStyle();
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
            UiTheme::pushButtonStyle(UiTheme::ElectricCyan);
            if (ImGui::SmallButton(editButtonId.c_str())) {
                openWatchlistEdit(watchlist);
                openWatchlistEditorPopup_ = true;
            }
            UiTheme::popButtonStyle();
            ImGui::PopID();
        }

        ImGui::EndTable();
    }
    UiTheme::popTableStyle();

    ImGui::EndChild();
    UiTheme::popPanelStyle();
}

void WatchlistView::drawWatchlistItems(AppState& state,
    WatchlistRepository& repository,
    MarketDataService& marketDataService,
    TechnicalIndicatorService& technicalIndicatorService,
    const std::function<void()>& reloadData)
{
    UiTheme::pushPanelStyle();
    ImGui::BeginChild("WatchlistItemsPanel", ImVec2(0.0f, 0.0f), true);
    ImGui::TextColored(UiTheme::TextPrimary, "Signal Command Surface");
    ImGui::TextWrapped("Scan symbols, technical context, and the existing Buy/Sell/Hold output from one table.");
    ImGui::Separator();

    const std::vector<Watchlist> active = activeWatchlists(state);
    if (active.empty()) {
        UiTheme::emptyState("No active watchlists", "Create or reactivate a watchlist before adding items.");
        ImGui::EndChild();
        UiTheme::popPanelStyle();
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

    std::vector<WatchlistItem> selectedItems = itemsForWatchlist(state, selectedWatchlistId_, technicalIndicatorService);
    const WatchlistSummary summary = PortfolioCalculator::calculateWatchlist(selectedItems);
    ImGui::SameLine();
>>>>>>> Stashed changes
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
<<<<<<< Updated upstream
    ImGui::InputTextWithHint("##WatchlistSearch", "Search watchlist", &searchText_);
=======
    ImGui::InputTextWithHint("##WatchlistSearch", "Search selected watchlist", &searchText_);
    UiTheme::pushButtonStyle(UiTheme::SoftBlue);
    if (ImGui::Button("Refresh History for Selected Watchlist", ImVec2(250.0f, 0.0f))) {
        refreshHistory(state, marketDataService, technicalIndicatorService, selectedItems, selectedName);
    }
    UiTheme::popButtonStyle();
    ImGui::SameLine();
    UiTheme::pushButtonStyle(UiTheme::SoftBlue);
    if (ImGui::Button("Refresh History for All Watchlists", ImVec2(226.0f, 0.0f))) {
        refreshHistory(state, marketDataService, technicalIndicatorService, state.watchlist, "All Watchlists");
    }
    UiTheme::popButtonStyle();
    ImGui::SameLine();
    ImGui::Checkbox("Show Extra Technicals", &showExtraTechnicals_);
>>>>>>> Stashed changes

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
<<<<<<< Updated upstream
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
=======
    if (selectedItems.empty()) {
        UiTheme::emptyState("No items in this watchlist", "Add symbols to this named watchlist when you want to monitor them manually.");
    } else {
        UiTheme::pushTableStyle();
        const int tableColumnCount = showExtraTechnicals_ ? 16 : 14;
        const ImGuiTableFlags tableFlags =
            ImGuiTableFlags_BordersInnerH |
            ImGuiTableFlags_RowBg |
            ImGuiTableFlags_Resizable |
            ImGuiTableFlags_Reorderable |
            ImGuiTableFlags_Hideable |
            ImGuiTableFlags_SizingFixedFit |
            ImGuiTableFlags_ScrollX |
            ImGuiTableFlags_ScrollY |
            ImGuiTableFlags_HighlightHoveredColumn;
        const float tableHeight = std::max(280.0f, ImGui::GetContentRegionAvail().y - 6.0f);
        if (ImGui::BeginTable("WatchlistTable", tableColumnCount, tableFlags, ImVec2(0.0f, tableHeight))) {
        ImGui::TableSetupColumn("Symbol", ImGuiTableColumnFlags_WidthFixed, 92.0f);
        ImGui::TableSetupColumn("Price", ImGuiTableColumnFlags_WidthFixed, 112.0f);
        ImGui::TableSetupColumn("Change", ImGuiTableColumnFlags_WidthFixed, 150.0f);
        ImGui::TableSetupColumn("RSI", ImGuiTableColumnFlags_WidthFixed, 82.0f);
        ImGui::TableSetupColumn("MACD Signal", ImGuiTableColumnFlags_WidthFixed, 128.0f);
        const std::string momentumHeader = momentumColumnLabel(state.signalRules.momentumLookbackSessions);
        ImGui::TableSetupColumn(momentumHeader.c_str(), ImGuiTableColumnFlags_WidthFixed, 150.0f);
        ImGui::TableSetupColumn("Signal", ImGuiTableColumnFlags_WidthFixed, 124.0f);
        ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthFixed, 210.0f);
        ImGui::TableSetupColumn("Priority", ImGuiTableColumnFlags_WidthFixed, 80.0f);
        ImGui::TableSetupColumn("Buy Level", ImGuiTableColumnFlags_WidthFixed, 110.0f);
        ImGui::TableSetupColumn("Sell Level", ImGuiTableColumnFlags_WidthFixed, 110.0f);
        if (showExtraTechnicals_) {
            ImGui::TableSetupColumn("MACD Line/Hist", ImGuiTableColumnFlags_WidthFixed, 182.0f);
            ImGui::TableSetupColumn("Volume", ImGuiTableColumnFlags_WidthFixed, 178.0f);
        }
        ImGui::TableSetupColumn("Last Refresh", ImGuiTableColumnFlags_WidthFixed, 150.0f);
        ImGui::TableSetupColumn("Source", ImGuiTableColumnFlags_WidthFixed, 132.0f);
>>>>>>> Stashed changes
        ImGui::TableSetupColumn("Actions", ImGuiTableColumnFlags_WidthFixed, 132.0f);
        ImGui::TableSetupScrollFreeze(1, 1);
        ImGui::TableHeadersRow();

        for (const WatchlistItem& item : state.watchlist) {
            if (!matchesFilter(item, searchText_)) {
                continue;
            }

            ++visibleRows;
            const std::optional<MarketQuote> quote = cachedQuoteFor(marketDataService, item);
            const std::optional<TechnicalIndicatorSnapshot> indicators = cachedIndicatorsFor(technicalIndicatorService, item);
            const std::optional<WatchlistMomentum7D> momentum = momentumFor(technicalIndicatorService, item, state.signalRules.momentumLookbackSessions);
            const WatchlistSignalResult signal = WatchlistSignalService::calculateSignal(item, indicators, state.signalRules);
            const std::string detailText = signalDetailText(item, indicators, momentum, signal, state.signalRules);

            constexpr float WatchlistRowHeight = 42.0f;
            ImGui::TableNextRow(ImGuiTableRowFlags_None, WatchlistRowHeight);
            const ImVec2 rowStart = ImGui::GetCursorScreenPos();
            const ImVec2 mouse = ImGui::GetIO().MousePos;
            const bool rowHovered = ImGui::IsWindowHovered(ImGuiHoveredFlags_AllowWhenBlockedByActiveItem) &&
                mouse.x >= ImGui::GetWindowPos().x &&
                mouse.x <= ImGui::GetWindowPos().x + ImGui::GetWindowWidth() &&
                mouse.y >= rowStart.y &&
                mouse.y <= rowStart.y + WatchlistRowHeight;
            ImGui::TableSetBgColor(ImGuiTableBgTarget_RowBg1,
                ImGui::GetColorU32(rowHovered ? UiTheme::withAlpha(UiTheme::ElectricCyan, 0.055f) : UiTheme::withAlpha(signalColor(signal), 0.025f)));
            ImGui::PushID(item.id);
            ImGui::TableNextColumn();
            ImGui::TextColored(UiTheme::TextPrimary, "%s", item.ticker.c_str());
            if (ImGui::IsItemHovered()) {
                showGlassTooltip(detailText);
            }
            ImGui::TableNextColumn();
            UiTheme::textRightAligned(currentPriceText(item.currentPrice).c_str());
            ImGui::TableNextColumn();
            drawRightAlignedTechnical(quoteChangeText(quote), quoteChangeColor(quote), "Quote change from cached market data. Missing values display as N/A.");
            ImGui::TableNextColumn();
            drawRightAlignedTechnical(WatchlistIndicatorDisplay::rsiDisplayText(indicators), rsiColor(indicators, state.signalRules), rsiTooltip(indicators, state.signalRules));
            ImGui::TableNextColumn();
            drawRightAlignedTechnical(WatchlistIndicatorDisplay::macdSignalDisplayText(indicators), macdSignalColor(indicators), macdSignalTooltip(indicators, state.signalRules));
            ImGui::TableNextColumn();
            drawRightAlignedTechnical(WatchlistIndicatorDisplay::momentumDisplayText(momentum), momentumColor(momentum), momentumTooltip(momentum, state.signalRules.momentumLookbackSessions));
            ImGui::TableNextColumn();
            drawSignalBadge(item, signal, detailText);
            ImGui::TableNextColumn();
            ImGui::Text("%s", item.assetName.c_str());
            ImGui::TextColored(UiTheme::TextMuted, "%s", item.assetType.c_str());
            ImGui::TableNextColumn();
            ImGui::Text("%s", currentPriceText(item.currentPrice).c_str());
            ImGui::TableNextColumn();
<<<<<<< Updated upstream
            ImGui::TextColored(UiTheme::Gain, "%s", priceOrNotSet(item.buySignalPrice).c_str());
            ImGui::TableNextColumn();
            ImGui::TextColored(UiTheme::Loss, "%s", priceOrNotSet(item.sellSignalPrice).c_str());
            ImGui::TableNextColumn();
            drawSignalBadge(item);
=======
            UiTheme::textRightAligned(priceOrNotSet(item.buySignalPrice).c_str(), UiTheme::Gain);
            ImGui::TableNextColumn();
            UiTheme::textRightAligned(priceOrNotSet(item.sellSignalPrice).c_str(), UiTheme::Loss);
            if (showExtraTechnicals_) {
                ImGui::TableNextColumn();
                ImGui::TextColored(indicators.has_value() && indicators->macdHistogram.has_value() ? UiTheme::moneyColor(*indicators->macdHistogram) : UiTheme::TextMuted,
                    "%s",
                    WatchlistIndicatorDisplay::macdLineHistogramDisplayText(indicators).c_str());
                ImGui::TableNextColumn();
                ImGui::TextColored(indicators.has_value() && indicators->volumeVsAvg20Percent.has_value() ? UiTheme::moneyColor(*indicators->volumeVsAvg20Percent) : UiTheme::TextMuted,
                    "%s",
                    volumeText(indicators).c_str());
            }
>>>>>>> Stashed changes
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

    draft_.signalStatus = WatchlistSignalService::calculateSignalStatus(draft_.currentPrice, draft_.buySignalPrice, draft_.sellSignalPrice, state.signalRules);

    ImGui::Separator();
<<<<<<< Updated upstream
    ImGui::TextColored(signalColor(draft_.signalStatus), "Current signal: %s", draft_.signalStatus.c_str());
=======
    const WatchlistSignalResult draftSignal = WatchlistSignalService::calculateSignal(draft_, std::nullopt, state.signalRules);
    ImGui::TextColored(signalColor(draftSignal), "Current signal: %s", draftSignal.signal.c_str());
    ImGui::TextColored(UiTheme::TextMuted, "%s", draftSignal.reasonText.c_str());
>>>>>>> Stashed changes
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
            draft_.signalStatus = WatchlistSignalService::calculateSignalStatus(draft_.currentPrice, draft_.buySignalPrice, draft_.sellSignalPrice, state.signalRules);

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
<<<<<<< Updated upstream
    WatchlistPriceRefreshStatus refreshStatus = WatchlistSignalService::refreshPrices(state.watchlist, marketDataService, repository, error);
=======
    WatchlistPriceRefreshStatus refreshStatus = WatchlistSignalService::refreshPrices(items, marketDataService, technicalIndicatorService, repository, error, state.signalRules);
>>>>>>> Stashed changes
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

<<<<<<< Updated upstream
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
=======
void WatchlistView::drawSignalBadge(const WatchlistItem& item, const WatchlistSignalResult& signal, const std::string& detailText)
{
    const ImVec4 color = signalColor(signal);
    const float width = std::max(92.0f, ImGui::GetContentRegionAvail().x);
    const std::string label = signalBadgeLabel(signal.signal) + "##signal_badge_" + std::to_string(item.id);

    UiTheme::pushBadgeStyle(color);
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(10.0f, 5.0f));
    if (ImGui::Button(label.c_str(), ImVec2(width, 0.0f))) {
        signalNoticeTicker_ = item.ticker;
        signalNoticeStatus_ = signal.signal;
        signalNoticeDetail_ = detailText;
        openSignalNoticePopup_ = true;
    }
    ImGui::PopStyleVar();
    if (ImGui::IsItemHovered() && !detailText.empty()) {
        showGlassTooltip(detailText);
    }
    UiTheme::popBadgeStyle();
>>>>>>> Stashed changes
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
<<<<<<< Updated upstream
    ImGui::TextWrapped("This is your saved price signal, not financial advice or a trade action.");
=======
    ImGui::TextWrapped("This is your saved price signal based on your own watchlist thresholds and local technical filters. It is not financial advice or a trade action.");
    if (!signalNoticeDetail_.empty()) {
        ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + 520.0f);
        ImGui::TextColored(UiTheme::Amber, "%s", signalNoticeDetail_.c_str());
        ImGui::PopTextWrapPos();
    }
>>>>>>> Stashed changes
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
