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

std::string optionalMoney(const std::optional<double>& value)
{
    return value.has_value() ? Money::format(*value) : "N/A";
}

std::string optionalNumber(const std::optional<double>& value, int decimals = 2)
{
    return value.has_value() ? Money::formatNumber(*value, decimals) : "N/A";
}

std::string optionalPercent(const std::optional<double>& value)
{
    return value.has_value() ? Money::formatPercent(*value) : "N/A";
}

void metricRow(const char* label, const std::string& value, ImVec4 color = ImVec4(1.0f, 1.0f, 1.0f, 1.0f))
{
    ImGui::TextColored(UiTheme::MutedText, "%s", label);
    ImGui::SameLine(190.0f);
    ImGui::TextColored(color, "%s", value.c_str());
}

bool watchlistContains(const AppState& state, const std::string& symbol)
{
    const std::string normalizedSymbol = uppercase(trim(symbol));
    return std::any_of(state.watchlist.begin(), state.watchlist.end(), [&normalizedSymbol](const WatchlistItem& item) {
        return uppercase(trim(item.ticker)) == normalizedSymbol;
    });
}

} // namespace

void StockResearchView::render(AppState& state,
    MarketDataService& marketDataService,
    WatchlistRepository& watchlistRepository,
    const std::function<void()>& reloadData)
{
    UiTheme::sectionHeading("Stock Research", "Informational quote research powered by Yahoo Finance. CSV import remains the portfolio update workflow.");

    ImGui::TextWrapped("Research data is informational, may be delayed or unavailable, and is not financial advice.");
    ImGui::Spacing();

    ImGui::SetNextItemWidth(220.0f);
    const bool enterPressed = ImGui::InputText("Ticker", &searchSymbol_, ImGuiInputTextFlags_EnterReturnsTrue);
    ImGui::SameLine();
    if (ImGui::Button("Search / Fetch") || enterPressed) {
        fetchSymbol(marketDataService, state);
    }
    ImGui::SameLine();
    ImGui::TextColored(UiTheme::MutedText, "Data source: %s", marketDataService.providerName());

    if (hasResult_) {
        const MarketQuote& quote = lastResult_.quote;
        ImGui::TextColored(UiTheme::MutedText, "Last fetched: %s", quote.fetchedAt.empty() ? "N/A" : quote.fetchedAt.c_str());
        if (!quote.quoteTime.empty()) {
            ImGui::SameLine();
            ImGui::TextColored(UiTheme::MutedText, "Quote time: %s", quote.quoteTime.c_str());
        }
        if (lastResult_.fromCache) {
            ImGui::TextColored(UiTheme::Amber, "%s", lastResult_.error.c_str());
        } else if (!lastResult_.error.empty()) {
            ImGui::TextColored(UiTheme::Amber, "%s", lastResult_.error.c_str());
        }
        if (!quote.rawStatus.empty()) {
            ImGui::TextColored(UiTheme::MutedText, "%s", quote.rawStatus.c_str());
        }
    } else if (!lastResult_.error.empty()) {
        ImGui::TextColored(UiTheme::Loss, "%s", lastResult_.error.c_str());
    }

    ImGui::Spacing();

    const float availableWidth = ImGui::GetContentRegionAvail().x;
    const float gap = ImGui::GetStyle().ItemSpacing.x;
    const float panelWidth = (availableWidth - gap) / 2.0f;

    if (TerminalPanel::begin("Basic Quote Summary", ImVec2(panelWidth, 260.0f))) {
        renderQuoteSummary();
    }
    TerminalPanel::end();
    ImGui::SameLine();
    if (TerminalPanel::begin("Key Metrics", ImVec2(panelWidth, 260.0f))) {
        renderMetrics();
    }
    TerminalPanel::end();

    ImGui::Spacing();

    if (TerminalPanel::begin("Price / History", ImVec2(panelWidth, 210.0f))) {
        renderHistoryPlaceholder();
    }
    TerminalPanel::end();
    ImGui::SameLine();
    if (TerminalPanel::begin("Notes / Watchlist", ImVec2(panelWidth, 210.0f))) {
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

void StockResearchView::fetchSymbol(MarketDataService& marketDataService, AppState& state)
{
    searchSymbol_ = uppercase(trim(searchSymbol_));
    lastResult_ = marketDataService.fetchQuote(searchSymbol_);
    hasResult_ = lastResult_.success;

    if (lastResult_.success) {
        state.setStatus(lastResult_.fromCache ? "Showing cached research quote for " + lastResult_.quote.symbol + "." : "Research quote fetched for " + lastResult_.quote.symbol + ".");
    } else {
        state.setStatus("Could not fetch research quote: " + lastResult_.error, true);
    }
}

void StockResearchView::renderQuoteSummary()
{
    if (!hasResult_) {
        ImGui::TextColored(UiTheme::MutedText, "Search for a ticker to view a quote summary.");
        return;
    }

    const MarketQuote& quote = lastResult_.quote;
    metricRow("Symbol", quote.symbol.empty() ? "N/A" : quote.symbol, UiTheme::Accent);
    metricRow("Company", quote.companyName.empty() ? "N/A" : quote.companyName);
    metricRow("Current Price", optionalMoney(quote.currentPrice), quote.currentPrice.has_value() ? UiTheme::Gain : UiTheme::MutedText);
    metricRow("Previous Close", optionalMoney(quote.previousClose));
    metricRow("Open", optionalMoney(quote.openPrice));
    metricRow("Day High", optionalMoney(quote.dayHigh));
    metricRow("Day Low", optionalMoney(quote.dayLow));
    metricRow("Currency", quote.currency.empty() ? "N/A" : quote.currency);
    metricRow("Exchange", quote.exchangeName.empty() ? "N/A" : quote.exchangeName);
}

void StockResearchView::renderMetrics()
{
    if (!hasResult_) {
        ImGui::TextColored(UiTheme::MutedText, "Metrics display when Yahoo Finance returns them.");
        return;
    }

    const MarketQuote& quote = lastResult_.quote;
    metricRow("52-Week High", optionalMoney(quote.fiftyTwoWeekHigh));
    metricRow("52-Week Low", optionalMoney(quote.fiftyTwoWeekLow));
    metricRow("Market Cap", optionalMoney(quote.marketCap));
    metricRow("Volume", optionalNumber(quote.volume, 0));
    metricRow("Average Volume", optionalNumber(quote.averageVolume, 0));
    metricRow("PE Ratio", optionalNumber(quote.peRatio));
    metricRow("EPS", optionalNumber(quote.eps));
    metricRow("Dividend Yield", optionalPercent(quote.dividendYield));
    metricRow("Beta", optionalNumber(quote.beta));
}

void StockResearchView::renderHistoryPlaceholder()
{
    ImGui::TextColored(UiTheme::Amber, "Historical chart placeholder");
    ImGui::TextWrapped("The provider abstraction includes a historical-price placeholder. A future pass can add range/interval charting without changing the Stock Research UI contract.");
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
}

void StockResearchView::renderWatchlistAction(AppState& state, WatchlistRepository& watchlistRepository, const std::function<void()>& reloadData)
{
    if (!hasResult_) {
        ImGui::TextColored(UiTheme::MutedText, "Fetch a quote before adding a symbol to the watchlist.");
        return;
    }

    const MarketQuote& quote = lastResult_.quote;
    ImGui::TextWrapped("Use this as a local note-taking shortcut only. It does not create trades, recommendations, alerts, or brokerage actions.");
    ImGui::Spacing();

    const bool alreadyWatching = watchlistContains(state, quote.symbol);
    if (alreadyWatching) {
        ImGui::TextColored(UiTheme::MutedText, "%s is already on the watchlist.", quote.symbol.c_str());
        return;
    }

    if (ImGui::Button("Add To Watchlist", ImVec2(160.0f, 0.0f))) {
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
        } else {
            state.setStatus("Could not add watchlist item: " + error, true);
        }
    }
}
