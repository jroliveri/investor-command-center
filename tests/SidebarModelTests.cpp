// SPDX-License-Identifier: MIT
#include "app/SidebarModel.hpp"

#include "util/Money.hpp"

#include <algorithm>
#include <iostream>
#include <string>
#include <vector>

namespace {

int Failures = 0;

void expectTrue(bool condition, const std::string& message)
{
    if (!condition) {
        ++Failures;
        std::cerr << "FAIL: " << message << '\n';
    }
}

void expectEqual(const std::string& actual, const std::string& expected, const std::string& message)
{
    if (actual != expected) {
        ++Failures;
        std::cerr << "FAIL: " << message << " expected [" << expected << "] actual [" << actual << "]\n";
    }
}

void expectEqual(int actual, int expected, const std::string& message)
{
    if (actual != expected) {
        ++Failures;
        std::cerr << "FAIL: " << message << " expected [" << expected << "] actual [" << actual << "]\n";
    }
}

const SidebarModel::MetricRow* findMetric(const SidebarModel::PortfolioSummaryCard& card, const std::string& label)
{
    const auto result = std::find_if(card.metrics.begin(), card.metrics.end(), [&label](const SidebarModel::MetricRow& metric) {
        return metric.label == label;
    });
    return result == card.metrics.end() ? nullptr : &*result;
}

void expectMetricValue(const SidebarModel::PortfolioSummaryCard& card, const std::string& label, const std::string& expected)
{
    const SidebarModel::MetricRow* metric = findMetric(card, label);
    expectTrue(metric != nullptr, label + " metric exists");
    if (metric != nullptr) {
        expectEqual(metric->value, expected, label + " renders");
    }
}

WatchlistItem watchlistItem(const std::string& symbol, double price)
{
    WatchlistItem item;
    item.ticker = symbol;
    item.assetName = symbol + " Corp";
    item.currentPrice = price;
    item.buySignalPrice = price - 10.0;
    item.sellSignalPrice = price + 10.0;
    return item;
}

MarketQuote quote(const std::string& symbol, double price, double percentChange)
{
    MarketQuote marketQuote;
    marketQuote.symbol = symbol;
    marketQuote.provider = "Yahoo Finance";
    marketQuote.currentPrice = price;
    marketQuote.priceChangePercent = percentChange;
    return marketQuote;
}

void testSidebarOrderAndRemovedCards()
{
    const std::vector<std::string> order = SidebarModel::sidebarWidgetOrder();
    expectEqual(static_cast<int>(order.size()), 3, "Sidebar exposes exactly three top-level cards");
    expectEqual(order[0], "Portfolio Summary", "Portfolio Summary is first");
    expectEqual(order[1], "Watchlists", "Watchlists is second");
    expectEqual(order[2], "Data Status", "Data Status is third");

    expectTrue(std::find(order.begin(), order.end(), "Navigation") == order.end(), "Navigation card is not in sidebar order");
    expectTrue(std::find(order.begin(), order.end(), "Portfolio Snapshot") == order.end(), "Separate Portfolio Snapshot card is not in sidebar order");
    expectTrue(std::find(order.begin(), order.end(), "Accounts") == order.end(), "Separate Accounts card is not in sidebar order");
}

void testPortfolioSummaryCardContent()
{
    std::vector<Account> accounts;
    Account checking;
    checking.id = 1;
    checking.accountName = "Checking";
    checking.cashBalance = 50.0;
    accounts.push_back(checking);

    Account brokerage;
    brokerage.id = 2;
    brokerage.accountName = "Brokerage";
    brokerage.cashBalance = 25.0;
    accounts.push_back(brokerage);

    std::vector<Holding> holdings;
    Holding aapl;
    aapl.accountId = 1;
    aapl.ticker = "AAPL";
    aapl.shares = 10.0;
    aapl.averageCost = 100.0;
    aapl.currentPrice = 120.0;
    holdings.push_back(aapl);

    Holding msft;
    msft.accountId = 2;
    msft.ticker = "MSFT";
    msft.shares = 5.0;
    msft.averageCost = 200.0;
    msft.currentPrice = 210.0;
    holdings.push_back(msft);

    const PortfolioSummary portfolio = PortfolioCalculator::calculateSummary(accounts, holdings);

    RealizedGainSummary realized;
    realized.thisYear = 123.45;

    PerformanceComparison performance;
    performance.hasDaily = true;
    performance.daily = 67.89;

    const SidebarModel::PortfolioSummaryCard card = SidebarModel::buildPortfolioSummaryCard(
        portfolio,
        realized,
        performance,
        accounts,
        holdings);

    expectEqual(card.title, "Portfolio Summary", "Combined portfolio card title renders");
    expectEqual(card.totalValue, Money::format(portfolio.accountBalance), "Total portfolio value renders");
    expectEqual(card.subtitle, "Holdings plus cash", "Portfolio subtitle renders");

    expectMetricValue(card, "Holdings Value", Money::format(portfolio.holdingsMarketValue));
    expectMetricValue(card, "Cash Balance", Money::format(portfolio.cashBalance));
    expectMetricValue(card, "Unrealized G/L", Money::format(portfolio.gainLossDollar));
    expectMetricValue(card, "Realized G/L", Money::format(realized.thisYear));
    expectMetricValue(card, "Daily Movement", Money::format(performance.daily));

    expectEqual(static_cast<int>(card.accounts.size()), 2, "Account rows render inside the combined card");
    expectEqual(card.accounts[0].name, "Checking", "First account name renders");
    expectEqual(card.accounts[0].value, "$1,250.00", "First account balance renders");
    expectEqual(card.accounts[1].name, "Brokerage", "Second account name renders");
    expectEqual(card.accounts[1].value, "$1,075.00", "Second account balance renders");
}

void testPortfolioSummaryAccountStatusesAndLimits()
{
    std::vector<Account> accounts;
    const int totalAccounts = SidebarModel::AccountPreviewLimit + 1;
    for (int index = 0; index < totalAccounts; ++index) {
        Account account;
        account.id = index + 1;
        account.accountName = "Account " + std::to_string(index + 1);
        account.cashBalance = 10.0;
        account.status = index == 1 ? "Dormant" : "Active";
        accounts.push_back(account);
    }

    const PortfolioSummary portfolio = PortfolioCalculator::calculateSummary(accounts, {});
    const SidebarModel::PortfolioSummaryCard card = SidebarModel::buildPortfolioSummaryCard(
        portfolio,
        RealizedGainSummary {},
        PerformanceComparison {},
        accounts,
        {});

    expectEqual(static_cast<int>(card.accounts.size()), SidebarModel::AccountPreviewLimit, "Account preview is capped");
    expectEqual(card.hiddenAccountCount, totalAccounts - SidebarModel::AccountPreviewLimit, "Hidden account count is exposed for View all affordance");
    expectTrue(!card.accounts[0].showStatus, "Active status is hidden in compact account rows");
    expectTrue(card.accounts[1].showStatus, "Meaningful non-active account status remains visible");
    expectEqual(card.accounts[1].status, "Dormant", "Non-active account status text is preserved");
}

void testWatchlistsTabsAndRows()
{
    std::vector<SidebarModel::WatchlistGroup> groups {
        SidebarModel::WatchlistGroup { "Main", { watchlistItem("AAPL", 203.14), watchlistItem("MSFT", 471.22) } },
        SidebarModel::WatchlistGroup { "Income", { watchlistItem("SCHD", 78.44) } },
        SidebarModel::WatchlistGroup { "Growth", {} },
    };
    const std::vector<MarketQuote> quotes {
        quote("AAPL", 203.14, 0.8),
        quote("MSFT", 471.22, -0.2),
        quote("SCHD", 78.44, 0.1),
    };

    const SidebarModel::WatchlistsCard main = SidebarModel::buildWatchlistsCard(groups, 0, quotes);
    expectEqual(main.title, "Watchlists", "Watchlists card title renders");
    expectEqual(static_cast<int>(main.tabs.size()), 3, "Multiple watchlist tabs render");
    expectEqual(main.tabs[0], "Main", "Main tab renders");
    expectEqual(main.tabs[1], "Income", "Income tab renders");
    expectEqual(main.rows[0].symbol, "AAPL", "Selected Main tab renders AAPL");
    expectEqual(main.rows[0].price, "$203.14", "Watchlist current price renders");
    expectEqual(main.rows[0].change, "+0.80%", "Watchlist percent change renders");

    const SidebarModel::WatchlistsCard income = SidebarModel::buildWatchlistsCard(groups, 1, quotes);
    expectEqual(income.selectedTab, 1, "Selected tab state changes");
    expectEqual(static_cast<int>(income.rows.size()), 1, "Selected Income tab renders its own rows");
    expectEqual(income.rows[0].symbol, "SCHD", "Switching tabs changes visible symbols");

    const SidebarModel::WatchlistsCard empty = SidebarModel::buildWatchlistsCard(groups, 2, quotes);
    expectTrue(empty.empty, "Empty watchlist state is exposed");
    expectEqual(empty.emptyText, "No symbols in this watchlist.", "Empty watchlist message is useful");
}

void testWatchlistsDefaultGroupAndLimits()
{
    std::vector<WatchlistItem> items;
    const int totalSymbols = SidebarModel::WatchlistPreviewLimit + 2;
    for (int index = 0; index < totalSymbols; ++index) {
        items.push_back(watchlistItem("SYM" + std::to_string(index + 1), 10.0 + index));
    }

    const std::vector<SidebarModel::WatchlistGroup> groups = SidebarModel::defaultWatchlistGroups(items);
    const SidebarModel::WatchlistsCard card = SidebarModel::buildWatchlistsCard(groups, 0, {});

    expectEqual(static_cast<int>(groups.size()), 1, "Single-list watchlist data becomes one forward-compatible group");
    expectEqual(groups[0].name, "Main", "Default watchlist group is Main");
    expectEqual(static_cast<int>(card.rows.size()), SidebarModel::WatchlistPreviewLimit, "Sidebar watchlist row count is capped");
    expectEqual(card.hiddenSymbolCount, totalSymbols - SidebarModel::WatchlistPreviewLimit, "Hidden symbol count supports compact +N more row");
    expectTrue(card.showCreateControl && !card.createEnabled, "Create tab can render disabled until a safe create action exists");
}

void testDataStatusHealthyAndWarnings()
{
    SidebarModel::RefreshSummary dashboard;
    dashboard.hasRun = true;
    dashboard.provider = "Yahoo Finance";
    dashboard.lastRefreshedAt = "2026-06-10 04:48:00";
    dashboard.refreshedSymbols = 3;
    dashboard.warning = "Current prices refreshed for dashboard display only. Holdings records and snapshots were not modified.";

    const SidebarModel::DataStatusCard healthy = SidebarModel::buildDataStatusCard(
        dashboard,
        SidebarModel::RefreshSummary {},
        "Yahoo Finance",
        "0.3.0",
        "",
        "2026-06-10");

    expectEqual(healthy.title, "Data Status", "Data Status card title renders");
    expectEqual(healthy.connectionLine, "Connected - Yahoo Finance", "Healthy status folds SQLite into Connected");
    expectEqual(healthy.lastRefreshLine, "Last refresh 04:48 - Backup monthly", "Last refresh and backup reminder render compactly");
    expectEqual(healthy.versionLine, "v0.3.0", "App version renders");
    expectTrue(!healthy.abnormal, "Display-only dashboard refresh note is not elevated as abnormal");

    SidebarModel::RefreshSummary watchlist;
    watchlist.hasRun = true;
    watchlist.provider = "Yahoo Finance";
    watchlist.lastRefreshedAt = "2026-06-10 05:00:00";
    watchlist.failedSymbols = 2;
    watchlist.cachedSymbols = 1;
    watchlist.warning = "Some watchlist prices are using cached Yahoo Finance quotes because live data was unavailable.";

    const SidebarModel::DataStatusCard warning = SidebarModel::buildDataStatusCard(
        dashboard,
        watchlist,
        "Yahoo Finance",
        "0.3.0",
        "",
        "2026-06-10");

    expectTrue(warning.abnormal && warning.refreshIssue, "Refresh warning state is elevated");
    expectTrue(warning.issueSummary.find("2 failed") != std::string::npos, "Failed symbol count appears in warning summary");
    expectTrue(warning.issueSummary.find("1 cached") != std::string::npos, "Cached symbol count appears in warning summary");

    const SidebarModel::DataStatusCard databaseIssue = SidebarModel::buildDataStatusCard(
        dashboard,
        SidebarModel::RefreshSummary {},
        "Yahoo Finance",
        "0.3.0",
        "Could not open database",
        "2026-06-10");

    expectTrue(databaseIssue.abnormal && databaseIssue.databaseIssue, "Database errors are elevated");
    expectEqual(databaseIssue.connectionLine, "Database issue - Yahoo Finance", "Database issue replaces healthy connected label");
    expectEqual(databaseIssue.issueSummary, "Database issue", "Database issue summary renders");
}

} // namespace

int main()
{
    testSidebarOrderAndRemovedCards();
    testPortfolioSummaryCardContent();
    testPortfolioSummaryAccountStatusesAndLimits();
    testWatchlistsTabsAndRows();
    testWatchlistsDefaultGroupAndLimits();
    testDataStatusHealthyAndWarnings();

    if (Failures > 0) {
        std::cerr << Failures << " sidebar test failure(s).\n";
        return 1;
    }

    std::cout << "Sidebar model tests passed.\n";
    return 0;
}
