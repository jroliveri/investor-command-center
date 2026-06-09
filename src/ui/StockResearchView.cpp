// SPDX-License-Identifier: MIT
#include "ui/StockResearchView.hpp"

#include "app/AppState.hpp"
#include "models/WatchlistItem.hpp"
#include "repositories/WatchlistRepository.hpp"
#include "services/MarketDataService.hpp"
#include "services/TechnicalIndicatorService.hpp"
#include "ui/UiTheme.hpp"
#include "ui/widgets/TerminalPanel.hpp"
#include "util/Money.hpp"

#include <imgui.h>
#include <imgui_stdlib.h>

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace {

class ScopedImGuiStyleColors {
public:
    ScopedImGuiStyleColors() = default;
    ScopedImGuiStyleColors(const ScopedImGuiStyleColors&) = delete;
    ScopedImGuiStyleColors& operator=(const ScopedImGuiStyleColors&) = delete;

    ~ScopedImGuiStyleColors()
    {
        if (count_ > 0) {
            ImGui::PopStyleColor(count_);
        }
    }

    void push(ImGuiCol colorIndex, ImVec4 color)
    {
        ImGui::PushStyleColor(colorIndex, color);
        ++count_;
    }

private:
    int count_ = 0;
};

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
    UiTheme::badge(label, color);
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

std::string optionalTechnicalNumber(const std::optional<double>& value, int decimals = 2)
{
    return value.has_value() ? Money::formatNumber(*value, decimals) : "N/A";
}

void renderTechnicalMetricCard(const char* label, const std::string& value, ImVec4 accent, ImVec2 size)
{
    UiTheme::pushPanelStyle();
    ImGui::PushStyleColor(ImGuiCol_Border, UiTheme::withAlpha(accent, 0.30f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(10.0f, 9.0f));
    ImGui::BeginChild(label, size, true, ImGuiWindowFlags_NoScrollbar);
    ImGui::PopStyleVar();

    const ImVec2 min = ImGui::GetWindowPos();
    const ImVec2 max(min.x + ImGui::GetWindowSize().x, min.y + ImGui::GetWindowSize().y);
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    drawList->AddRectFilled(min, max, ImGui::GetColorU32(UiTheme::withAlpha(UiTheme::SurfaceMain, 0.44f)), 14.0f);
    drawList->AddRectFilled(min, ImVec2(max.x, min.y + 2.0f), ImGui::GetColorU32(UiTheme::withAlpha(accent, 0.68f)), 14.0f, ImDrawFlags_RoundCornersTop);
    drawList->AddRect(min, max, ImGui::GetColorU32(UiTheme::withAlpha(accent, 0.26f)), 14.0f);

    ImGui::TextColored(UiTheme::TextMuted, "%s", label);
    ImGui::SetWindowFontScale(1.08f);
    ImGui::TextColored(value == "N/A" ? UiTheme::TextMuted : accent, "%s", value.c_str());
    ImGui::SetWindowFontScale(1.0f);

    ImGui::EndChild();
    ImGui::PopStyleColor();
    UiTheme::popPanelStyle();
}

void renderTechnicalMetricGrid(const std::vector<std::pair<const char*, std::pair<std::string, ImVec4>>>& metrics)
{
    const float availableWidth = ImGui::GetContentRegionAvail().x;
    const int columns = availableWidth > 680.0f ? 4 : (availableWidth > 460.0f ? 3 : 2);
    const float gap = ImGui::GetStyle().ItemSpacing.x;
    const float cardWidth = (availableWidth - gap * static_cast<float>(columns - 1)) / static_cast<float>(columns);
    const ImVec2 cardSize(cardWidth, 66.0f);

    for (std::size_t index = 0; index < metrics.size(); ++index) {
        renderTechnicalMetricCard(metrics[index].first, metrics[index].second.first, metrics[index].second.second, cardSize);
        if (static_cast<int>(index % static_cast<std::size_t>(columns)) != columns - 1) {
            ImGui::SameLine();
        }
    }
}

void renderCurrentPriceHero(const MarketQuote& quote)
{
    ImGui::TextColored(UiTheme::TextSecondary, "Current Price");
    ImGui::SetWindowFontScale(1.55f);
    UiTheme::pushNumericFont();
    ImGui::TextColored(quote.currentPrice.has_value() ? UiTheme::TextPrimary : UiTheme::TextMuted, "%s", optionalMoney(quote.currentPrice).c_str());
    UiTheme::popNumericFont();
    ImGui::SetWindowFontScale(1.0f);

    const std::string change = optionalMoney(quote.priceChangeDollar);
    const std::string changePercent = optionalPercent(quote.priceChangePercent, true);
    UiTheme::pushNumericFont();
    ImGui::TextColored(movementColor(quote.priceChangeDollar), "%s   %s", change.c_str(), changePercent.c_str());
    UiTheme::popNumericFont();
}

} // namespace

void StockResearchView::render(AppState& state,
    MarketDataService& marketDataService,
    TechnicalIndicatorService& technicalIndicatorService,
    WatchlistRepository& watchlistRepository,
    const std::function<void()>& reloadData)
{
    ScopedImGuiStyleColors pageColors;
    pageColors.push(ImGuiCol_Text, UiTheme::TextPrimary);
    pageColors.push(ImGuiCol_TextDisabled, UiTheme::TextMuted);

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

    if (TerminalPanel::begin("Price / History", ImVec2(panelWidth, 250.0f))) {
        renderHistoryPanel(marketDataService, technicalIndicatorService, state);
    }
    TerminalPanel::end();
    ImGui::SameLine();
    if (TerminalPanel::begin("Technicals", ImVec2(panelWidth, 250.0f))) {
        renderTechnicalsPanel(technicalIndicatorService);
    }
    TerminalPanel::end();

    ImGui::Spacing();

    if (TerminalPanel::begin("Notes / Watchlist", ImVec2(0.0f, 165.0f))) {
        renderWatchlistAction(state, watchlistRepository, reloadData);
    }
    TerminalPanel::end();

}

void StockResearchView::refreshCurrent(MarketDataService& marketDataService, AppState& state)
{
    if (trim(searchSymbol_).empty()) {
        state.setStatus("Open Stock Research and enter a ticker before refreshing.", true);
        return;
    }

    fetchSymbol(marketDataService, state);
}

void StockResearchView::refreshCurrentHistory(MarketDataService& marketDataService, TechnicalIndicatorService& technicalIndicatorService, AppState& state)
{
    const std::string symbol = hasResult_ && !lastResult_.quote.symbol.empty() ? lastResult_.quote.symbol : searchSymbol_;
    if (trim(symbol).empty()) {
        state.setStatus("Open Stock Research and enter a ticker before refreshing historical prices.", true);
        return;
    }

    fetchHistory(marketDataService, technicalIndicatorService, state, historyRange_);
}

void StockResearchView::fetchSymbol(MarketDataService& marketDataService, AppState& state)
{
    searchSymbol_ = uppercase(trim(searchSymbol_));
    lastResult_ = marketDataService.fetchQuote(searchSymbol_);
    hasResult_ = lastResult_.success;
    lastHistoryResult_ = HistoricalPriceResult {};
    lastTechnicalResult_ = TechnicalIndicatorResult {};
    historyRequestHasRun_ = false;
    technicalRequestHasRun_ = false;

    if (lastResult_.success) {
        const std::string status = statusLabel(lastResult_, hasResult_);
        state.setStatus(status == "Live" ? "Research quote fetched for " + lastResult_.quote.symbol + "." : "Research quote loaded with " + status + " data for " + lastResult_.quote.symbol + ".");
    } else {
        state.setStatus("Could not fetch research quote: " + lastResult_.error, true);
    }
}

void StockResearchView::fetchHistory(MarketDataService& marketDataService, TechnicalIndicatorService& technicalIndicatorService, AppState& state, const std::string& range)
{
    std::string symbol = hasResult_ && !lastResult_.quote.symbol.empty() ? lastResult_.quote.symbol : searchSymbol_;
    symbol = uppercase(trim(symbol));
    if (symbol.empty()) {
        state.setStatus("Enter a ticker before refreshing historical prices.", true);
        return;
    }

    searchSymbol_ = symbol;
    historyRange_ = range;
    lastHistoryResult_ = marketDataService.fetchHistoricalPrices(symbol, historyRange_, "1d");
    historyRequestHasRun_ = true;
    lastTechnicalResult_ = technicalIndicatorService.calculateAndCache(symbol, lastHistoryResult_.provider.empty() ? marketDataService.providerName() : lastHistoryResult_.provider);
    technicalRequestHasRun_ = true;

    if (lastHistoryResult_.success) {
        const std::string indicatorSummary = lastTechnicalResult_.success ? " Indicators calculated." : " Indicator calculation failed: " + lastTechnicalResult_.error;
        if (lastHistoryResult_.fromCache) {
            state.setStatus("Historical daily OHLCV loaded from cache for " + lastHistoryResult_.symbol + ": " +
                std::to_string(lastHistoryResult_.rows.size()) + " rows available. Source: " + lastHistoryResult_.provider + "." + indicatorSummary,
                !lastTechnicalResult_.success);
        } else {
            state.setStatus("Historical daily OHLCV refreshed for " + lastHistoryResult_.symbol + ": " +
                std::to_string(lastHistoryResult_.rowsStored) + " rows added/updated. Source: " + lastHistoryResult_.provider + "." + indicatorSummary,
                !lastTechnicalResult_.success);
        }
    } else if (lastTechnicalResult_.success) {
        state.setStatus("Historical price refresh failed, but cached technical indicators are available for " + lastTechnicalResult_.snapshot.symbol + ".", false);
    } else {
        state.setStatus("Could not fetch historical prices: " + lastHistoryResult_.error, true);
    }
}

void StockResearchView::clearResult(AppState& state)
{
    searchSymbol_.clear();
    lastResult_ = MarketQuoteResult {};
    lastHistoryResult_ = HistoricalPriceResult {};
    lastTechnicalResult_ = TechnicalIndicatorResult {};
    hasResult_ = false;
    historyRequestHasRun_ = false;
    technicalRequestHasRun_ = false;
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
    UiTheme::pushPanelStyle();
    ImGui::BeginChild("ResearchToolbar", ImVec2(0.0f, 58.0f), true, ImGuiWindowFlags_NoScrollbar);
    ImGui::AlignTextToFramePadding();
    ImGui::TextColored(UiTheme::TextSecondary, "Ticker");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(150.0f);
    const bool enterPressed = ImGui::InputText("##ResearchTicker", &searchSymbol_, ImGuiInputTextFlags_EnterReturnsTrue);
    searchSymbol_ = uppercase(searchSymbol_);

    ImGui::SameLine();
    UiTheme::pushButtonStyle(UiTheme::NeonMagenta);
    if (ImGui::Button("Search / Fetch", ImVec2(120.0f, 0.0f)) || enterPressed) {
        fetchSymbol(marketDataService, state);
    }
    UiTheme::popButtonStyle();

    ImGui::SameLine();
    const bool canAdd = hasResult_ && !lastResult_.quote.symbol.empty() && !watchlistContains(state, lastResult_.quote.symbol);
    if (!canAdd) {
        ImGui::BeginDisabled();
    }
    UiTheme::pushButtonStyle(UiTheme::ElectricCyan);
    if (ImGui::Button("Add to Watchlist", ImVec2(135.0f, 0.0f))) {
        addCurrentQuoteToWatchlist(state, watchlistRepository, reloadData);
    }
    UiTheme::popButtonStyle();
    if (!canAdd) {
        ImGui::EndDisabled();
    }

    ImGui::SameLine();
    if (!hasResult_) {
        ImGui::BeginDisabled();
    }
    UiTheme::pushButtonStyle(UiTheme::ElectricCyan);
    if (ImGui::Button("Refresh", ImVec2(82.0f, 0.0f))) {
        fetchSymbol(marketDataService, state);
    }
    UiTheme::popButtonStyle();
    if (!hasResult_) {
        ImGui::EndDisabled();
    }

    ImGui::SameLine();
    UiTheme::pushButtonStyle(UiTheme::TextMuted);
    if (ImGui::Button("Clear", ImVec2(70.0f, 0.0f))) {
        clearResult(state);
    }
    UiTheme::popButtonStyle();

    ImGui::SameLine();
    ImGui::TextColored(UiTheme::TextMuted, "Data source: %s", marketDataService.providerName());
    ImGui::EndChild();
    UiTheme::popPanelStyle();
}

void StockResearchView::renderStatusStrip(const MarketDataService& marketDataService)
{
    const std::string status = statusLabel(lastResult_, hasResult_);
    UiTheme::pushPanelStyle();
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
    UiTheme::popPanelStyle();
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

void StockResearchView::renderHistoryPanel(MarketDataService& marketDataService, TechnicalIndicatorService& technicalIndicatorService, AppState& state)
{
    ImGui::TextColored(UiTheme::TextSecondary, "Daily OHLCV Cache");
    ImGui::TextWrapped("Fetches daily open, high, low, close, adjusted close, and volume for future RSI, MACD, and volume views.");
    ImGui::Spacing();

    const bool hasSymbol = !trim(searchSymbol_).empty() || (hasResult_ && !lastResult_.quote.symbol.empty());
    if (!hasSymbol) {
        ImGui::TextColored(UiTheme::TextMuted, "Enter a ticker to fetch historical data.");
    }

    constexpr std::array<const char*, 7> Ranges = { "1M", "3M", "6M", "YTD", "1Y", "2Y", "5Y" };
    if (!hasSymbol) {
        ImGui::BeginDisabled();
    }
    for (const char* range : Ranges) {
        ImGui::PushID(range);
        const bool selected = historyRange_ == range;
        UiTheme::pushButtonStyle(selected ? UiTheme::NeonMagenta : UiTheme::ElectricCyan);
        if (ImGui::Button(range, ImVec2(54.0f, 0.0f))) {
            fetchHistory(marketDataService, technicalIndicatorService, state, range);
        }
        UiTheme::popButtonStyle();
        ImGui::PopID();
        ImGui::SameLine();
    }
    UiTheme::pushButtonStyle(UiTheme::ElectricCyan);
    if (ImGui::Button("Refresh History", ImVec2(132.0f, 0.0f))) {
        fetchHistory(marketDataService, technicalIndicatorService, state, historyRange_);
    }
    UiTheme::popButtonStyle();
    if (!hasSymbol) {
        ImGui::EndDisabled();
    }

    ImGui::Spacing();
    if (historyRequestHasRun_) {
        if (lastHistoryResult_.success) {
            const ImVec4 statusColor = lastHistoryResult_.fromCache ? UiTheme::TextWarning : UiTheme::TextSuccess;
            ImGui::TextColored(statusColor, "%s history: %zu rows available, %d rows added/updated.",
                lastHistoryResult_.symbol.c_str(),
                lastHistoryResult_.rows.size(),
                lastHistoryResult_.rowsStored);
            ImGui::TextColored(UiTheme::TextSecondary, "Range: %s  Interval: %s  Source: %s",
                lastHistoryResult_.range.c_str(),
                lastHistoryResult_.interval.c_str(),
                lastHistoryResult_.provider.c_str());
            ImGui::TextColored(UiTheme::TextSecondary, "Last refresh: %s",
                lastHistoryResult_.fetchedAt.empty() ? "N/A" : lastHistoryResult_.fetchedAt.c_str());
            if (!lastHistoryResult_.error.empty()) {
                ImGui::TextColored(UiTheme::TextWarning, "%s", lastHistoryResult_.error.c_str());
            }
        } else {
            ImGui::TextColored(UiTheme::TextDanger, "%s", lastHistoryResult_.error.empty()
                    ? "No historical data is available for this ticker."
                    : lastHistoryResult_.error.c_str());
        }
    } else {
        ImGui::TextColored(UiTheme::TextMuted, "No historical refresh has run for this symbol yet.");
    }

    ImGui::Separator();
    ImGui::TextColored(UiTheme::TextMuted, "Technical indicator snapshots update after historical refresh.");
}

void StockResearchView::renderTechnicalsPanel(TechnicalIndicatorService& technicalIndicatorService)
{
    const std::string symbol = hasResult_ && !lastResult_.quote.symbol.empty() ? lastResult_.quote.symbol : std::string();
    if (!symbol.empty() && (!technicalRequestHasRun_ || lastTechnicalResult_.snapshot.symbol != symbol)) {
        std::string error;
        const std::optional<TechnicalIndicatorSnapshot> cached = technicalIndicatorService.cachedSnapshot(symbol, "Yahoo Finance", error);
        technicalRequestHasRun_ = true;
        if (cached.has_value()) {
            lastTechnicalResult_ = TechnicalIndicatorResult {};
            lastTechnicalResult_.success = true;
            lastTechnicalResult_.fromCache = true;
            lastTechnicalResult_.snapshot = *cached;
        } else {
            lastTechnicalResult_ = TechnicalIndicatorResult {};
            lastTechnicalResult_.error = error.empty() ? "No cached technical indicators are available. Refresh history first." : error;
        }
    }

    if (!hasResult_) {
        ImGui::TextColored(UiTheme::TextMuted, "Fetch a quote, then refresh history to calculate technical indicators.");
        return;
    }

    if (!lastTechnicalResult_.success) {
        ImGui::TextColored(UiTheme::TextMuted, "%s", lastTechnicalResult_.error.empty()
                ? "Refresh historical data to calculate RSI, MACD, and volume context."
                : lastTechnicalResult_.error.c_str());
        ImGui::TextWrapped("Not enough historical data to calculate this indicator.");
        return;
    }

    const TechnicalIndicatorSnapshot& snapshot = lastTechnicalResult_.snapshot;
    ImGui::TextColored(UiTheme::TextSecondary, "Historical data source: %s", snapshot.provider.c_str());
    ImGui::TextColored(UiTheme::TextSecondary, "Calculated for: %s", snapshot.calculatedForDate.empty() ? "N/A" : snapshot.calculatedForDate.c_str());
    ImGui::TextColored(UiTheme::TextSecondary, "Calculated at: %s", snapshot.calculatedAt.empty() ? "N/A" : snapshot.calculatedAt.c_str());
    ImGui::Separator();
    renderTechnicalMetricGrid({
        { "RSI 14", { optionalTechnicalNumber(snapshot.rsi14, 1), UiTheme::ElectricCyan } },
        { "MACD Line", { optionalTechnicalNumber(snapshot.macdLine, 4), UiTheme::ElectricCyan } },
        { "MACD Signal", { optionalTechnicalNumber(snapshot.macdSignal, 4), UiTheme::NeonMagenta } },
        { "MACD Histogram", { optionalTechnicalNumber(snapshot.macdHistogram, 4), movementColor(snapshot.macdHistogram) } },
        { "Latest Volume", { formatLargeNumber(snapshot.latestVolume), UiTheme::ElectricCyan } },
        { "20D Avg Volume", { formatLargeNumber(snapshot.avgVolume20), UiTheme::TextSecondary } },
        { "50D Avg Volume", { formatLargeNumber(snapshot.avgVolume50), UiTheme::TextSecondary } },
        { "Volume vs 20D Avg", { optionalPercent(snapshot.volumeVsAvg20Percent, true), movementColor(snapshot.volumeVsAvg20Percent) } },
    });

    if (!snapshot.rsi14.has_value() || !snapshot.macdHistogram.has_value() || !snapshot.avgVolume20.has_value()) {
        ImGui::TextColored(UiTheme::TextMuted, "Not enough historical data to calculate every indicator.");
    }
    ImGui::TextColored(UiTheme::TextMuted, "Indicators are informational only and do not change user signals.");
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
