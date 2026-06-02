// SPDX-License-Identifier: MIT
#include "ui/StockResearchView.hpp"

#include "app/AppState.hpp"
#include "models/WatchlistItem.hpp"
#include "repositories/WatchlistRepository.hpp"
#include "services/MarketDataService.hpp"
#include "ui/UiTheme.hpp"
#include "ui/widgets/TerminalPanel.hpp"
#include "util/Money.hpp"

#include <imgui.h>
#include <imgui_stdlib.h>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <optional>
#include <string>

namespace {

std::string trim(std::string value)
{
    value.erase(value.begin(), std::find_if(value.begin(), value.end(), [](unsigned char character) {
        return std::isspace(character) == 0;
    }));
    value.erase(std::find_if(value.rbegin(), value.rend(), [](unsigned char character) {
        return std::isspace(character) == 0;
    }).base(), value.end());
    return value;
}

std::string uppercase(std::string value)
{
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char character) {
        return static_cast<char>(std::toupper(character));
    });
    return value;
}

std::string naIfEmpty(const std::string& value)
{
    return value.empty() ? "N/A" : value;
}

std::string optionalMoney(const std::optional<double>& value)
{
    return value.has_value() ? Money::format(*value) : "N/A";
}

std::string optionalNumber(const std::optional<double>& value, int decimals = 2)
{
    return value.has_value() ? Money::formatNumber(*value, decimals) : "N/A";
}

std::string optionalPercent(const std::optional<double>& value, bool includeSign = false)
{
    return value.has_value() ? Money::formatPercent(*value, includeSign) : "N/A";
}

std::string formatLargeNumber(const std::optional<double>& value)
{
    if (!value.has_value()) {
        return "N/A";
    }

    const double absoluteValue = std::fabs(*value);
    const char* suffix = "";
    double scaled = *value;

    if (absoluteValue >= 1'000'000'000'000.0) {
        scaled = *value / 1'000'000'000'000.0;
        suffix = "T";
    } else if (absoluteValue >= 1'000'000'000.0) {
        scaled = *value / 1'000'000'000.0;
        suffix = "B";
    } else if (absoluteValue >= 1'000'000.0) {
        scaled = *value / 1'000'000.0;
        suffix = "M";
    } else if (absoluteValue >= 1'000.0) {
        scaled = *value / 1'000.0;
        suffix = "K";
    }

    return Money::formatNumber(scaled, absoluteValue >= 1'000.0 ? 2 : 0) + suffix;
}

bool watchlistContains(const AppState& state, const std::string& symbol)
{
    const std::string normalizedSymbol = uppercase(trim(symbol));
    return std::any_of(state.watchlist.begin(), state.watchlist.end(), [&normalizedSymbol](const WatchlistItem& item) {
        return uppercase(trim(item.ticker)) == normalizedSymbol;
    });
}

ImVec4 movementColor(const std::optional<double>& value)
{
    if (!value.has_value() || std::fabs(*value) < 0.005) {
        return UiTheme::TextMuted;
    }
    return UiTheme::moneyColor(*value);
}

std::string statusLabel(const MarketQuoteResult& result, bool hasResult)
{
    if (!hasResult && !result.error.empty()) {
        return "Error";
    }
    if (!hasResult) {
        return "Idle";
    }
    if (result.fromCache) {
        return "Cached";
    }
    if (result.quote.rawStatus.find("fallback") != std::string::npos || result.quote.rawStatus.find("chart metadata") != std::string::npos) {
        return "Fallback";
    }
    return "Live";
}

ImVec4 statusColor(const std::string& status)
{
    if (status == "Live") {
        return UiTheme::TextSuccess;
    }
    if (status == "Cached" || status == "Fallback") {
        return UiTheme::TextWarning;
    }
    if (status == "Error") {
        return UiTheme::TextDanger;
    }
    return UiTheme::TextMuted;
}

void renderStatusBadge(const char* label, ImVec4 color)
{
    ImGui::PushStyleColor(ImGuiCol_Button, color);
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, color);
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, color);
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.05f, 0.06f, 0.07f, 1.0f));
    ImGui::Button(label, ImVec2(92.0f, 0.0f));
    ImGui::PopStyleColor(4);
}

void renderResearchMetric(const char* label, const std::string& value, ImVec4 color)
{
    ImGui::TextColored(UiTheme::TextSecondary, "%s", label);
    ImGui::SameLine(175.0f);
    if (value == "N/A") {
        color = UiTheme::TextMuted;
    }
    ImGui::TextColored(color, "%s", value.c_str());
}

void renderResearchMetric(const char* label, const std::string& value)
{
    renderResearchMetric(label, value, UiTheme::TextPrimary);
}

void renderCurrentPriceHero(const MarketQuote& quote)
{
    ImGui::TextColored(UiTheme::TextSecondary, "Current Price");
    ImGui::SetWindowFontScale(1.55f);
    ImGui::TextColored(quote.currentPrice.has_value() ? UiTheme::TextPrimary : UiTheme::TextMuted, "%s", optionalMoney(quote.currentPrice).c_str());
    ImGui::SetWindowFontScale(1.0f);

    const std::string change = optionalMoney(quote.priceChangeDollar);
    const std::string changePercent = optionalPercent(quote.priceChangePercent, true);
    ImGui::TextColored(movementColor(quote.priceChangeDollar), "%s   %s", change.c_str(), changePercent.c_str());
}

} // namespace

void StockResearchView::render(AppState& state,
    MarketDataService& marketDataService,
    WatchlistRepository& watchlistRepository,
    const std::function<void()>& reloadData)
{
    ImGui::PushStyleColor(ImGuiCol_Text, UiTheme::TextPrimary);
    ImGui::PushStyleColor(ImGuiCol_TextDisabled, UiTheme::TextMuted);

    UiTheme::sectionHeading("Stock Research", "Research data is informational and may be delayed. CSV import remains the portfolio update workflow.");

    renderToolbar(state, marketDataService, watchlistRepository, reloadData);
    renderStatusStrip(marketDataService);

    ImGui::Spacing();

    const float availableWidth = ImGui::GetContentRegionAvail().x;
    const float gap = ImGui::GetStyle().ItemSpacing.x;
    const float panelWidth = (availableWidth - gap) / 2.0f;

    if (TerminalPanel::begin("Quote Summary", ImVec2(panelWidth, 315.0f))) {
        renderQuoteSummary();
    }
    TerminalPanel::end();
    ImGui::SameLine();
    if (TerminalPanel::begin("Key Metrics", ImVec2(panelWidth, 315.0f))) {
        renderMetrics();
    }
    TerminalPanel::end();

    ImGui::Spacing();

    if (TerminalPanel::begin("Price / History", ImVec2(panelWidth, 220.0f))) {
        renderHistoryPlaceholder();
    }
    TerminalPanel::end();
    ImGui::SameLine();
    if (TerminalPanel::begin("Notes / Watchlist", ImVec2(panelWidth, 220.0f))) {
        renderWatchlistAction(state, watchlistRepository, reloadData);
    }
    TerminalPanel::end();

    ImGui::PopStyleColor(2);
}

void StockResearchView::refreshCurrent(MarketDataService& marketDataService, AppState& state)
{
    if (trim(searchSymbol_).empty()) {
        state.setStatus("Open Stock Research and enter a ticker before refreshing.", true);
        return;
    }

    fetchSymbol(marketDataService, state);
}

void StockResearchView::fetchSymbol(MarketDataService& marketDataService, AppState& state)
{
    searchSymbol_ = uppercase(trim(searchSymbol_));
    lastResult_ = marketDataService.fetchQuote(searchSymbol_);
    hasResult_ = lastResult_.success;

    if (lastResult_.success) {
        const std::string status = statusLabel(lastResult_, hasResult_);
        state.setStatus(status == "Live" ? "Research quote fetched for " + lastResult_.quote.symbol + "." : "Research quote loaded with " + status + " data for " + lastResult_.quote.symbol + ".");
    } else {
        state.setStatus("Could not fetch research quote: " + lastResult_.error, true);
    }
}

void StockResearchView::clearResult(AppState& state)
{
    searchSymbol_.clear();
    lastResult_ = MarketQuoteResult {};
    hasResult_ = false;
    state.clearStatus();
}

bool StockResearchView::addCurrentQuoteToWatchlist(AppState& state, WatchlistRepository& watchlistRepository, const std::function<void()>& reloadData)
{
    if (!hasResult_ || lastResult_.quote.symbol.empty()) {
        return false;
    }

    const MarketQuote& quote = lastResult_.quote;
    if (watchlistContains(state, quote.symbol)) {
        state.setStatus(quote.symbol + " is already on the local watchlist.");
        return false;
    }

    WatchlistItem item;
    item.ticker = quote.symbol;
    item.assetName = quote.companyName.empty() ? quote.symbol : quote.companyName;
    item.assetType = "Stock";
    item.currentPrice = quote.currentPrice.value_or(0.0);
    item.priority = "Medium";
    item.reasonWatching = "Added from Stock Research.";

    std::string error;
    if (watchlistRepository.create(item, error)) {
        reloadData();
        state.setStatus("Added " + quote.symbol + " to the local watchlist.");
        return true;
    }

    state.setStatus("Could not add watchlist item: " + error, true);
    return false;
}

void StockResearchView::renderToolbar(AppState& state, MarketDataService& marketDataService, WatchlistRepository& watchlistRepository, const std::function<void()>& reloadData)
{
    ImGui::BeginChild("ResearchToolbar", ImVec2(0.0f, 58.0f), true, ImGuiWindowFlags_NoScrollbar);
    ImGui::AlignTextToFramePadding();
    ImGui::TextColored(UiTheme::TextSecondary, "Ticker");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(150.0f);
    const bool enterPressed = ImGui::InputText("##ResearchTicker", &searchSymbol_, ImGuiInputTextFlags_EnterReturnsTrue);
    searchSymbol_ = uppercase(searchSymbol_);

    ImGui::SameLine();
    if (ImGui::Button("Search / Fetch", ImVec2(120.0f, 0.0f)) || enterPressed) {
        fetchSymbol(marketDataService, state);
    }

    ImGui::SameLine();
    const bool canAdd = hasResult_ && !lastResult_.quote.symbol.empty() && !watchlistContains(state, lastResult_.quote.symbol);
    if (!canAdd) {
        ImGui::BeginDisabled();
    }
    if (ImGui::Button("Add to Watchlist", ImVec2(135.0f, 0.0f))) {
        addCurrentQuoteToWatchlist(state, watchlistRepository, reloadData);
    }
    if (!canAdd) {
        ImGui::EndDisabled();
    }

    ImGui::SameLine();
    if (!hasResult_) {
        ImGui::BeginDisabled();
    }
    if (ImGui::Button("Refresh", ImVec2(82.0f, 0.0f))) {
        fetchSymbol(marketDataService, state);
    }
    if (!hasResult_) {
        ImGui::EndDisabled();
    }

    ImGui::SameLine();
    if (ImGui::Button("Clear", ImVec2(70.0f, 0.0f))) {
        clearResult(state);
    }

    ImGui::SameLine();
    ImGui::TextColored(UiTheme::TextMuted, "Data source: %s", marketDataService.providerName());
    ImGui::EndChild();
}

void StockResearchView::renderStatusStrip(const MarketDataService& marketDataService)
{
    const std::string status = statusLabel(lastResult_, hasResult_);
    ImGui::BeginChild("ResearchStatusStrip", ImVec2(0.0f, 82.0f), true, ImGuiWindowFlags_NoScrollbar);
    renderStatusBadge(status.c_str(), statusColor(status));
    ImGui::SameLine();
    ImGui::TextColored(UiTheme::TextPrimary, "Provider: %s", marketDataService.providerName());
    ImGui::SameLine(250.0f);
    ImGui::TextColored(UiTheme::TextPrimary, "Last fetched: %s", hasResult_ && !lastResult_.quote.fetchedAt.empty() ? lastResult_.quote.fetchedAt.c_str() : "N/A");
    ImGui::SameLine(520.0f);
    ImGui::TextColored(UiTheme::TextPrimary, "Quote time: %s", hasResult_ && !lastResult_.quote.quoteTime.empty() ? lastResult_.quote.quoteTime.c_str() : "N/A");

    if ((status == "Cached" || status == "Fallback") && hasResult_) {
        ImGui::TextColored(UiTheme::TextWarning, "Showing cached/fallback data because the live quote request failed or returned limited data.");
    } else if (status == "Error") {
        ImGui::TextColored(UiTheme::TextDanger, "%s", lastResult_.error.empty() ? "No quote data is available for this ticker." : lastResult_.error.c_str());
    } else {
        ImGui::TextColored(UiTheme::TextMuted, "Research data is informational only. It does not create trades, alerts, recommendations, or brokerage actions.");
    }
    ImGui::EndChild();
}

void StockResearchView::renderQuoteSummary()
{
    if (!hasResult_) {
        ImGui::TextColored(UiTheme::TextMuted, lastResult_.error.empty()
                ? "Search for a ticker to view a quote summary."
                : "No quote data is available. Check the ticker or try again later.");
        return;
    }

    const MarketQuote& quote = lastResult_.quote;
    renderCurrentPriceHero(quote);
    ImGui::Separator();
    renderResearchMetric("Symbol", naIfEmpty(quote.symbol), UiTheme::Accent);
    renderResearchMetric("Company", naIfEmpty(quote.companyName));
    renderResearchMetric("Price Change $", optionalMoney(quote.priceChangeDollar), movementColor(quote.priceChangeDollar));
    renderResearchMetric("Price Change %", optionalPercent(quote.priceChangePercent, true), movementColor(quote.priceChangePercent));
    renderResearchMetric("Previous Close", optionalMoney(quote.previousClose));
    renderResearchMetric("Open", optionalMoney(quote.openPrice));
    renderResearchMetric("Day High", optionalMoney(quote.dayHigh));
    renderResearchMetric("Day Low", optionalMoney(quote.dayLow));
    renderResearchMetric("Currency", naIfEmpty(quote.currency));
    renderResearchMetric("Exchange", naIfEmpty(quote.exchangeName));
}

void StockResearchView::renderMetrics()
{
    if (!hasResult_) {
        ImGui::TextColored(UiTheme::TextMuted, "Metrics display as N/A when Yahoo Finance does not return them.");
        return;
    }

    const MarketQuote& quote = lastResult_.quote;
    renderResearchMetric("52-Week High", optionalMoney(quote.fiftyTwoWeekHigh));
    renderResearchMetric("52-Week Low", optionalMoney(quote.fiftyTwoWeekLow));
    renderResearchMetric("Market Cap", formatLargeNumber(quote.marketCap));
    renderResearchMetric("Volume", formatLargeNumber(quote.volume));
    renderResearchMetric("Average Volume", formatLargeNumber(quote.averageVolume));
    renderResearchMetric("PE Ratio", optionalNumber(quote.peRatio));
    renderResearchMetric("EPS", optionalNumber(quote.eps));
    renderResearchMetric("Dividend Yield", optionalPercent(quote.dividendYield));
    renderResearchMetric("Beta", optionalNumber(quote.beta));
}

void StockResearchView::renderHistoryPlaceholder()
{
    ImGui::TextColored(UiTheme::TextWarning, "Historical chart support is planned.");
    ImGui::TextWrapped("Historical chart support is planned. Current provider abstraction already includes a placeholder for range and interval support.");
    ImGui::Spacing();
    ImGui::BeginDisabled();
    ImGui::Button("1M", ImVec2(54.0f, 0.0f));
    ImGui::SameLine();
    ImGui::Button("3M", ImVec2(54.0f, 0.0f));
    ImGui::SameLine();
    ImGui::Button("1Y", ImVec2(54.0f, 0.0f));
    ImGui::SameLine();
    ImGui::Button("All", ImVec2(54.0f, 0.0f));
    ImGui::EndDisabled();
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
        ImGui::SetTooltip("Historical price fetching is not implemented yet.");
    }
}

void StockResearchView::renderWatchlistAction(AppState& state, WatchlistRepository& watchlistRepository, const std::function<void()>& reloadData)
{
    if (!hasResult_) {
        ImGui::TextColored(UiTheme::TextMuted, "Fetch a quote before adding a symbol to the watchlist.");
        ImGui::TextWrapped("This panel is for local notes and watchlist context only.");
        return;
    }

    const MarketQuote& quote = lastResult_.quote;
    const bool alreadyWatching = watchlistContains(state, quote.symbol);
    ImGui::TextColored(UiTheme::TextPrimary, "Symbol: %s", quote.symbol.c_str());
    ImGui::TextColored(UiTheme::TextSecondary, "Watchlist: ");
    ImGui::SameLine();
    UiTheme::badge(alreadyWatching ? "Already watching" : "Not watching", alreadyWatching ? UiTheme::TextSuccess : UiTheme::TextMuted);
    ImGui::Spacing();

    if (alreadyWatching) {
        ImGui::TextColored(UiTheme::TextMuted, "%s is already on the local watchlist.", quote.symbol.c_str());
    } else if (ImGui::Button("Add to Watchlist", ImVec2(150.0f, 0.0f))) {
        addCurrentQuoteToWatchlist(state, watchlistRepository, reloadData);
    }

    ImGui::Spacing();
    ImGui::TextWrapped("This does not create trades, alerts, recommendations, transfers, or brokerage actions.");
}
