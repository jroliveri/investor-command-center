// SPDX-License-Identifier: MIT
#include "app/SidebarModel.hpp"

#include "util/Money.hpp"

#include <algorithm>
#include <cctype>
#include <optional>

namespace SidebarModel {
namespace {

std::string normalizeSymbol(std::string symbol)
{
    symbol.erase(symbol.begin(), std::find_if(symbol.begin(), symbol.end(), [](unsigned char character) {
        return std::isspace(character) == 0;
    }));
    symbol.erase(std::find_if(symbol.rbegin(), symbol.rend(), [](unsigned char character) {
        return std::isspace(character) == 0;
    }).base(), symbol.end());
    std::transform(symbol.begin(), symbol.end(), symbol.begin(), [](unsigned char character) {
        return static_cast<char>(std::toupper(character));
    });
    return symbol;
}

std::optional<MarketQuote> quoteForSymbol(const std::vector<MarketQuote>& quotes, const std::string& symbol)
{
    const std::string normalizedSymbol = normalizeSymbol(symbol);
    for (const MarketQuote& quote : quotes) {
        if (normalizeSymbol(quote.symbol) == normalizedSymbol) {
            return quote;
        }
    }
    return std::nullopt;
}

std::string priceText(const WatchlistItem& item, const std::optional<MarketQuote>& quote)
{
    if (quote.has_value() && quote->currentPrice.has_value()) {
        return Money::format(*quote->currentPrice);
    }
    return item.currentPrice > 0.0 ? Money::format(item.currentPrice) : "N/A";
}

std::string changeText(const std::optional<MarketQuote>& quote)
{
    if (!quote.has_value()) {
        return "N/A";
    }
    if (quote->priceChangePercent.has_value()) {
        return Money::formatPercent(*quote->priceChangePercent, true);
    }
    if (quote->priceChangeDollar.has_value()) {
        return Money::format(*quote->priceChangeDollar);
    }
    return "N/A";
}

bool isNormalDashboardRefreshNote(const std::string& warning)
{
    return warning.find("display only") != std::string::npos || warning.find("display-only") != std::string::npos;
}

RefreshSummary latestRefresh(
    const RefreshSummary& dashboardRefresh,
    const RefreshSummary& watchlistRefresh,
    const std::string& providerFallback)
{
    RefreshSummary summary;
    summary.provider = providerFallback.empty() ? "Yahoo Finance" : providerFallback;

    const bool preferWatchlist = watchlistRefresh.hasRun &&
        (!dashboardRefresh.hasRun || watchlistRefresh.lastRefreshedAt >= dashboardRefresh.lastRefreshedAt);

    if (preferWatchlist) {
        summary = watchlistRefresh;
    } else if (dashboardRefresh.hasRun) {
        summary = dashboardRefresh;
    }

    if (summary.provider.empty()) {
        summary.provider = providerFallback.empty() ? "Yahoo Finance" : providerFallback;
    }
    return summary;
}

std::string refreshIssueSummary(const RefreshSummary& refresh)
{
    std::string summary = "Refresh warning";
    if (refresh.failedSymbols > 0) {
        summary += " - " + std::to_string(refresh.failedSymbols) + " failed";
    }
    if (refresh.cachedSymbols > 0) {
        summary += refresh.failedSymbols > 0 ? ", " : " - ";
        summary += std::to_string(refresh.cachedSymbols) + " cached";
    }
    return summary;
}

} // namespace

std::vector<std::string> sidebarWidgetOrder()
{
    return { "Portfolio Summary", "Watchlists", "Data Status" };
}

std::string truncate(const std::string& value, std::size_t maxLength)
{
    if (value.size() <= maxLength) {
        return value;
    }
    if (maxLength <= 3) {
        return value.substr(0, maxLength);
    }
    return value.substr(0, maxLength - 3) + "...";
}

bool isMeaningfulAccountStatus(const Account& account)
{
    return !account.status.empty() && account.status != "Active";
}

PortfolioSummaryCard buildPortfolioSummaryCard(
    const PortfolioSummary& portfolio,
    const RealizedGainSummary& realizedGains,
    const PerformanceComparison& performance,
    const std::vector<Account>& accounts,
    const std::vector<Holding>& holdings,
    int accountPreviewLimit)
{
    PortfolioSummaryCard card;
    card.title = "Portfolio Summary";
    card.totalValue = Money::format(portfolio.accountBalance);
    card.subtitle = "Holdings plus cash";
    card.metrics = {
        MetricRow { "Holdings Value", Money::format(portfolio.holdingsMarketValue), Tone::Success, portfolio.holdingsMarketValue },
        MetricRow { "Cash Balance", Money::format(portfolio.cashBalance), Tone::Warning, portfolio.cashBalance },
        MetricRow { "Unrealized G/L", Money::format(portfolio.gainLossDollar), Tone::Money, portfolio.gainLossDollar },
        MetricRow { "Realized G/L", Money::format(realizedGains.thisYear), Tone::Money, realizedGains.thisYear },
        MetricRow {
            "Daily Movement",
            performance.hasDaily ? Money::format(performance.daily) : "N/A",
            performance.hasDaily ? Tone::Money : Tone::Secondary,
            performance.hasDaily ? performance.daily : 0.0 },
    };

    const int visibleAccountCount = std::min<int>(accountPreviewLimit, static_cast<int>(accounts.size()));
    for (int index = 0; index < visibleAccountCount; ++index) {
        const Account& account = accounts[static_cast<std::size_t>(index)];
        const AccountMetrics metrics = PortfolioCalculator::calculateAccount(account, holdings);
        AccountRow row;
        row.name = account.accountName;
        row.value = Money::format(metrics.calculatedBalance);
        row.status = account.status;
        row.showStatus = isMeaningfulAccountStatus(account);
        card.accounts.push_back(row);
    }

    card.hiddenAccountCount = std::max(0, static_cast<int>(accounts.size()) - visibleAccountCount);
    return card;
}

std::vector<WatchlistGroup> defaultWatchlistGroups(const std::vector<WatchlistItem>& items)
{
    return { WatchlistGroup { "Main", items } };
}

WatchlistsCard buildWatchlistsCard(
    const std::vector<WatchlistGroup>& sourceGroups,
    int selectedTab,
    const std::vector<MarketQuote>& cachedQuotes,
    int symbolPreviewLimit)
{
    std::vector<WatchlistGroup> groups = sourceGroups;
    if (groups.empty()) {
        groups.push_back(WatchlistGroup { "Main", {} });
    }

    selectedTab = std::clamp(selectedTab, 0, static_cast<int>(groups.size()) - 1);

    WatchlistsCard card;
    card.title = "Watchlists";
    card.selectedTab = selectedTab;
    card.emptyText = "No symbols in this watchlist.";
    for (const WatchlistGroup& group : groups) {
        card.tabs.push_back(group.name);
    }

    const WatchlistGroup& selectedGroup = groups[static_cast<std::size_t>(selectedTab)];
    const int visibleSymbolCount = std::min<int>(symbolPreviewLimit, static_cast<int>(selectedGroup.items.size()));
    for (int index = 0; index < visibleSymbolCount; ++index) {
        const WatchlistItem& item = selectedGroup.items[static_cast<std::size_t>(index)];
        const std::optional<MarketQuote> quote = quoteForSymbol(cachedQuotes, item.ticker);
        card.rows.push_back(WatchlistRow {
            item.ticker,
            priceText(item, quote),
            changeText(quote),
            item.signalStatus.empty() ? "Hold" : item.signalStatus,
        });
    }

    card.empty = selectedGroup.items.empty();
    card.hiddenSymbolCount = std::max(0, static_cast<int>(selectedGroup.items.size()) - visibleSymbolCount);
    return card;
}

std::string compactTimestamp(const std::string& timestamp, const std::string& today)
{
    if (timestamp.empty()) {
        return "not run";
    }
    if (timestamp.size() >= 16 && timestamp[4] == '-' && timestamp[7] == '-' && timestamp[10] == ' ') {
        if (!today.empty() && timestamp.substr(0, 10) == today) {
            return timestamp.substr(11, 5);
        }
        return timestamp.substr(5, 5) + " " + timestamp.substr(11, 5);
    }
    return truncate(timestamp, 18);
}

bool isAbnormalRefreshStatus(const RefreshSummary& refresh)
{
    return refresh.failedSymbols > 0 || refresh.cachedSymbols > 0 || (!refresh.warning.empty() && !isNormalDashboardRefreshNote(refresh.warning));
}

DataStatusCard buildDataStatusCard(
    const RefreshSummary& dashboardRefresh,
    const RefreshSummary& watchlistRefresh,
    const std::string& providerFallback,
    const std::string& appVersion,
    const std::string& databaseError,
    const std::string& today)
{
    const RefreshSummary refresh = latestRefresh(dashboardRefresh, watchlistRefresh, providerFallback);
    DataStatusCard card;
    card.title = "Data Status";
    card.databaseIssue = !databaseError.empty();
    card.refreshIssue = isAbnormalRefreshStatus(refresh);
    card.abnormal = card.databaseIssue || card.refreshIssue;
    card.connectionLine = std::string(card.databaseIssue ? "Database issue" : "Connected") + " - " + refresh.provider;
    card.lastRefreshLine = "Last refresh " + compactTimestamp(refresh.lastRefreshedAt, today) + " - Backup monthly";
    card.versionLine = "v" + appVersion;

    if (card.databaseIssue) {
        card.issueSummary = "Database issue";
        card.issueDetail = truncate(databaseError, 58);
    } else if (card.refreshIssue) {
        card.issueSummary = refreshIssueSummary(refresh);
        card.issueDetail = truncate(refresh.warning, 58);
    }

    return card;
}

} // namespace SidebarModel
