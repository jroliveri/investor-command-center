// SPDX-License-Identifier: MIT
#pragma once

#include "models/Account.hpp"
#include "models/Holding.hpp"
#include "models/MarketQuote.hpp"
#include "models/WatchlistItem.hpp"
#include "services/PerformanceCalculator.hpp"
#include "services/PortfolioCalculator.hpp"

#include <string>
#include <vector>

namespace SidebarModel {

constexpr int AccountPreviewLimit = 5;
constexpr int WatchlistPreviewLimit = 8;

enum class Tone {
    Primary,
    Secondary,
    Success,
    Warning,
    Danger,
    Money
};

struct MetricRow {
    std::string label;
    std::string value;
    Tone tone = Tone::Secondary;
    double toneValue = 0.0;
};

struct AccountRow {
    std::string name;
    std::string value;
    std::string status;
    bool showStatus = false;
};

struct PortfolioSummaryCard {
    std::string title;
    std::string totalValue;
    std::string subtitle;
    std::vector<MetricRow> metrics;
    std::vector<AccountRow> accounts;
    int hiddenAccountCount = 0;
};

struct WatchlistGroup {
    std::string name;
    std::vector<WatchlistItem> items;
};

struct WatchlistRow {
    std::string symbol;
    std::string price;
    std::string change;
    std::string signalStatus;
};

struct WatchlistsCard {
    std::string title;
    std::vector<std::string> tabs;
    int selectedTab = 0;
    std::vector<WatchlistRow> rows;
    bool empty = false;
    std::string emptyText;
    int hiddenSymbolCount = 0;
    bool showCreateControl = true;
    bool createEnabled = false;
};

struct RefreshSummary {
    bool hasRun = false;
    std::string provider = "Yahoo Finance";
    std::string lastRefreshedAt;
    int refreshedSymbols = 0;
    int failedSymbols = 0;
    int cachedSymbols = 0;
    std::string warning;
};

struct DataStatusCard {
    std::string title;
    std::string connectionLine;
    std::string lastRefreshLine;
    std::string versionLine;
    bool abnormal = false;
    bool databaseIssue = false;
    bool refreshIssue = false;
    std::string issueSummary;
    std::string issueDetail;
};

std::vector<std::string> sidebarWidgetOrder();
std::string truncate(const std::string& value, std::size_t maxLength);
bool isMeaningfulAccountStatus(const Account& account);

PortfolioSummaryCard buildPortfolioSummaryCard(
    const PortfolioSummary& portfolio,
    const RealizedGainSummary& realizedGains,
    const PerformanceComparison& performance,
    const std::vector<Account>& accounts,
    const std::vector<Holding>& holdings,
    int accountPreviewLimit = AccountPreviewLimit);

std::vector<WatchlistGroup> defaultWatchlistGroups(const std::vector<WatchlistItem>& items);

WatchlistsCard buildWatchlistsCard(
    const std::vector<WatchlistGroup>& groups,
    int selectedTab,
    const std::vector<MarketQuote>& cachedQuotes,
    int symbolPreviewLimit = WatchlistPreviewLimit);

std::string compactTimestamp(const std::string& timestamp, const std::string& today);
bool isAbnormalRefreshStatus(const RefreshSummary& refresh);

DataStatusCard buildDataStatusCard(
    const RefreshSummary& dashboardRefresh,
    const RefreshSummary& watchlistRefresh,
    const std::string& providerFallback,
    const std::string& appVersion,
    const std::string& databaseError,
    const std::string& today);

} // namespace SidebarModel
