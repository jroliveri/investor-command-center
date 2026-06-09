// SPDX-License-Identifier: MIT
#include "db/Database.hpp"
#include "db/Migrations.hpp"
#include "models/SignalRules.hpp"
#include "models/TechnicalIndicatorSnapshot.hpp"
#include "models/WatchlistItem.hpp"
#include "repositories/AppSettingsRepository.hpp"
#include "repositories/MarketPriceHistoryRepository.hpp"
#include "repositories/TechnicalIndicatorCacheRepository.hpp"
#include "repositories/WatchlistRepository.hpp"
#include "services/SignalRulesService.hpp"
#include "services/TechnicalIndicatorService.hpp"
#include "services/WatchlistIndicatorDisplay.hpp"
#include "services/WatchlistSignalService.hpp"

#include <cmath>
#include <iostream>
#include <optional>
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

void expectNear(double actual, double expected, const std::string& message)
{
    if (std::fabs(actual - expected) > 0.000001) {
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

void expectSignalEquivalent(const WatchlistSignalResult& actual, const WatchlistSignalResult& expected, const std::string& message)
{
    expectEqual(actual.signal, expected.signal, message + " signal");
    expectTrue(actual.priceConditionMet == expected.priceConditionMet, message + " price condition");
    expectTrue(actual.sellConditionMet == expected.sellConditionMet, message + " sell condition");
    expectTrue(actual.rsiConditionMet == expected.rsiConditionMet, message + " RSI condition");
    expectTrue(actual.macdConditionMet == expected.macdConditionMet, message + " MACD condition");
    expectTrue(actual.hasCurrentPrice == expected.hasCurrentPrice, message + " current price availability");
    expectTrue(actual.hasRsi == expected.hasRsi, message + " RSI availability");
    expectTrue(actual.hasMacd == expected.hasMacd, message + " MACD availability");
    expectEqual(actual.reasonText, expected.reasonText, message + " reason text");
}

std::string twoDigit(int value)
{
    return (value < 10 ? "0" : "") + std::to_string(value);
}

MarketPriceHistoryRow historyRow(double close, int dayOffset = 0)
{
    MarketPriceHistoryRow row;
    row.symbol = "TST";
    row.provider = "Yahoo Finance";
    const int month = 1 + (dayOffset / 28);
    const int day = 1 + (dayOffset % 28);
    row.priceDate = "2026-" + twoDigit(month) + "-" + twoDigit(day);
    row.closePrice = close;
    return row;
}

void testIndicatorDisplayText()
{
    TechnicalIndicatorSnapshot snapshot;
    snapshot.rsi14 = 54.26;
    snapshot.macdSignal = -0.12344;
    snapshot.macdLine = 0.2346;
    snapshot.macdHistogram = 0.3579;
    const std::optional<TechnicalIndicatorSnapshot> indicatorSnapshot = snapshot;

    expectEqual(WatchlistIndicatorDisplay::rsiDisplayText(indicatorSnapshot), "54.3", "RSI renders with one decimal");
    expectEqual(WatchlistIndicatorDisplay::macdSignalDisplayText(indicatorSnapshot), "-0.1234", "MACD Signal renders signal-line value");
    expectEqual(WatchlistIndicatorDisplay::macdLineHistogramDisplayText(indicatorSnapshot), "Line 0.235 / Hist 0.358", "MACD line/hist text remains available");
    expectEqual(WatchlistIndicatorDisplay::rsiDisplayText(std::nullopt), "N/A", "Missing RSI data renders safely");
    expectEqual(WatchlistIndicatorDisplay::macdSignalDisplayText(std::nullopt), "N/A", "Missing MACD Signal data renders safely");
}

void testMomentumDisplayText()
{
    std::vector<MarketPriceHistoryRow> rows {
        historyRow(100.0),
        historyRow(101.0),
        historyRow(102.0),
        historyRow(103.0),
        historyRow(104.0),
        historyRow(105.0),
        historyRow(106.0),
        historyRow(107.0),
    };
    const std::optional<WatchlistMomentum7D> rising = WatchlistIndicatorDisplay::calculateMomentum7D(rows);
    expectTrue(rising.has_value(), "Momentum 7D calculates with eight closing prices");
    expectTrue(rising->rising, "Momentum 7D detects rising closes");
    expectNear(rising->percent, 7.0, "Momentum 7D percent uses close from seven trading sessions ago");
    expectEqual(WatchlistIndicatorDisplay::momentumDisplayText(rising), "Rising +7.00%", "Momentum 7D renders rising label and percent");

    rows.back().closePrice = 100.0;
    const std::optional<WatchlistMomentum7D> flat = WatchlistIndicatorDisplay::calculateMomentum7D(rows);
    expectTrue(flat.has_value() && flat->flat, "Momentum 7D detects flat closes");
    expectEqual(WatchlistIndicatorDisplay::momentumDisplayText(flat), "Flat 0.00%", "Momentum 7D renders flat label");

    rows.back().closePrice = 90.0;
    const std::optional<WatchlistMomentum7D> falling = WatchlistIndicatorDisplay::calculateMomentum7D(rows);
    expectTrue(falling.has_value() && !falling->rising && !falling->flat, "Momentum 7D detects falling closes");
    expectEqual(WatchlistIndicatorDisplay::momentumDisplayText(falling), "Falling -10.00%", "Momentum 7D renders falling label and percent");

    rows.pop_back();
    expectTrue(!WatchlistIndicatorDisplay::calculateMomentum7D(rows).has_value(), "Momentum 7D handles insufficient data");
    expectEqual(WatchlistIndicatorDisplay::momentumDisplayText(std::nullopt), "—", "Missing Momentum 7D renders safely");
}

void testConfigurableMomentumLookback()
{
    std::vector<MarketPriceHistoryRow> rows {
        historyRow(100.0),
        historyRow(101.0),
        historyRow(102.0),
        historyRow(103.0),
        historyRow(104.0),
    };

    const std::optional<WatchlistMomentum7D> twoSessionMomentum = WatchlistIndicatorDisplay::calculateMomentum(rows, 2);
    expectTrue(twoSessionMomentum.has_value(), "Configurable Momentum calculates with enough closing prices");
    expectNear(twoSessionMomentum->percent, (2.0 / 102.0) * 100.0, "Configurable Momentum percent uses requested lookback");
    expectTrue(!WatchlistIndicatorDisplay::calculateMomentum(rows, 7).has_value(), "Configurable Momentum reports insufficient data for longer lookback");
    expectTrue(!WatchlistIndicatorDisplay::calculateMomentum(rows, 0).has_value(), "Configurable Momentum rejects non-positive lookback");
}

void testTechnicalIndicatorCalculationFixture()
{
    Database database;
    std::string error;
    expectTrue(database.open(":memory:"), "In-memory database opens for technical indicator fixture");
    expectTrue(Migrations::run(database, error), "Migrations run for technical indicator fixture: " + error);

    MarketPriceHistoryRepository historyRepository(database);
    TechnicalIndicatorCacheRepository cacheRepository(database);
    TechnicalIndicatorService technicalIndicatorService(historyRepository, cacheRepository);

    std::vector<MarketPriceHistoryRow> rows;
    rows.reserve(35);
    for (int index = 0; index < 35; ++index) {
        MarketPriceHistoryRow row = historyRow(static_cast<double>(index + 1), index);
        row.symbol = "calc";
        row.volume = 1000.0 + static_cast<double>(index);
        rows.push_back(row);
    }

    int rowsStored = 0;
    expectTrue(historyRepository.upsertMany(rows, rowsStored, error), "Technical indicator fixture history stores: " + error);
    expectEqual(rowsStored, 35, "Technical indicator fixture stores every close");

    const TechnicalIndicatorResult result = technicalIndicatorService.calculateAndCache(" calc ");
    expectTrue(result.success, "Technical indicators calculate from known close fixture: " + result.error);
    expectEqual(result.snapshot.symbol, "CALC", "Technical indicator calculation normalizes symbol");
    expectTrue(result.snapshot.rsi14.has_value(), "Known fixture produces RSI");
    expectTrue(result.snapshot.macdLine.has_value(), "Known fixture produces MACD line");
    expectTrue(result.snapshot.macdSignal.has_value(), "Known fixture produces MACD signal line");
    expectTrue(result.snapshot.macdHistogram.has_value(), "Known fixture produces MACD histogram");
    expectNear(*result.snapshot.rsi14, 100.0, "Known rising close fixture preserves RSI calculation");
    expectNear(*result.snapshot.macdLine, 7.0, "Known rising close fixture preserves MACD line calculation");
    expectNear(*result.snapshot.macdSignal, 7.0, "Known rising close fixture preserves MACD signal calculation");
    expectNear(*result.snapshot.macdHistogram, 0.0, "Known rising close fixture preserves MACD histogram calculation");

    const std::optional<TechnicalIndicatorSnapshot> cached = technicalIndicatorService.cachedSnapshot("CALC", "Yahoo Finance", error);
    expectTrue(error.empty(), "Cached technical indicator lookup does not error: " + error);
    expectTrue(cached.has_value(), "Calculated technical indicators persist to cache");
    expectEqual(WatchlistIndicatorDisplay::rsiDisplayText(cached), "100.0", "Cached RSI display uses calculated value");
    expectEqual(WatchlistIndicatorDisplay::macdSignalDisplayText(cached), "7.0000", "Cached MACD Signal display uses signal-line value");
}

void testWatchlistRowDisplayValues()
{
    WatchlistItem item;
    item.ticker = "TST";
    item.assetName = "Test Corp";
    item.currentPrice = 95.0;
    item.buySignalPrice = 100.0;
    item.sellSignalPrice = 120.0;

    TechnicalIndicatorSnapshot snapshot;
    snapshot.rsi14 = 50.0;
    snapshot.macdSignal = 1.25;
    snapshot.macdHistogram = 0.20;
    const std::optional<TechnicalIndicatorSnapshot> indicators = snapshot;

    std::vector<MarketPriceHistoryRow> rows {
        historyRow(100.0),
        historyRow(101.0),
        historyRow(102.0),
        historyRow(103.0),
        historyRow(104.0),
        historyRow(105.0),
        historyRow(106.0),
        historyRow(107.0),
    };
    const std::optional<WatchlistMomentum7D> momentum = WatchlistIndicatorDisplay::calculateMomentum7D(rows);
    const WatchlistSignalResult signal = WatchlistSignalService::calculateSignal(item, indicators, SignalRulesService::defaults());

    expectEqual(WatchlistIndicatorDisplay::rsiDisplayText(indicators), "50.0", "Watchlist row renders RSI with full indicator data");
    expectEqual(WatchlistIndicatorDisplay::macdSignalDisplayText(indicators), "1.2500", "Watchlist row renders MACD Signal with full indicator data");
    expectEqual(WatchlistIndicatorDisplay::momentumDisplayText(momentum), "Rising +7.00%", "Watchlist row renders Momentum 7D with full history data");
    expectEqual(signal.signal, "Buy", "Watchlist row renders existing Buy/Sell/Hold output with full data");

    const WatchlistSignalResult missingIndicatorSignal = WatchlistSignalService::calculateSignal(item, std::nullopt, SignalRulesService::defaults());
    expectEqual(WatchlistIndicatorDisplay::rsiDisplayText(std::nullopt), "N/A", "Watchlist row renders missing RSI safely");
    expectEqual(WatchlistIndicatorDisplay::macdSignalDisplayText(std::nullopt), "N/A", "Watchlist row renders missing MACD Signal safely");
    expectEqual(WatchlistIndicatorDisplay::momentumDisplayText(std::nullopt), "—", "Watchlist row renders missing Momentum 7D safely");
    expectEqual(missingIndicatorSignal.signal, "Hold", "Watchlist row keeps Buy/Sell/Hold safe when indicators are missing");
}

void testSignalRulesDefaultsAndValidation()
{
    const SignalRules defaults = SignalRulesService::defaults();
    expectNear(defaults.rsiBuyMin, 40.0, "Default RSI lower threshold preserves current behavior");
    expectNear(defaults.rsiBuyMax, 60.0, "Default RSI upper threshold preserves current behavior");
    expectNear(defaults.macdHistogramMin, 0.0, "Default MACD histogram threshold preserves current behavior");
    expectEqual(defaults.momentumLookbackSessions, 7, "Default Momentum lookback is seven sessions");

    std::string error;
    expectTrue(SignalRulesService::validate(defaults, error), "Default signal rules validate");

    SignalRules invalid = defaults;
    invalid.rsiBuyMin = -1.0;
    expectTrue(!SignalRulesService::validate(invalid, error), "RSI lower threshold below 0 is invalid");

    invalid = defaults;
    invalid.rsiBuyMax = 40.0;
    expectTrue(!SignalRulesService::validate(invalid, error), "RSI upper threshold must be greater than lower threshold");

    invalid = defaults;
    invalid.momentumLookbackSessions = 0;
    expectTrue(!SignalRulesService::validate(invalid, error), "Momentum lookback must be positive");
}

void testSignalOutputUnchanged()
{
    TechnicalIndicatorSnapshot snapshot;
    snapshot.rsi14 = 50.0;
    snapshot.macdHistogram = 0.25;
    const std::optional<TechnicalIndicatorSnapshot> indicatorSnapshot = snapshot;

    expectEqual(WatchlistSignalService::calculateSignal(95.0, 100.0, 120.0, indicatorSnapshot).signal, "Buy", "Buy still requires price, RSI, and MACD histogram confirmation");
    expectEqual(WatchlistSignalService::calculateSignal(130.0, 100.0, 120.0, indicatorSnapshot).signal, "Sell", "Sell remains price-only");
    expectEqual(WatchlistSignalService::calculateSignal(95.0, 100.0, 120.0, std::nullopt).signal, "Hold", "Missing indicators still prevent Buy");
    expectEqual(WatchlistSignalService::calculateSignal(110.0, 100.0, 120.0, indicatorSnapshot).signal, "Hold", "Unreached thresholds remain Hold");

    const SignalRules defaults = SignalRulesService::defaults();
    expectEqual(WatchlistSignalService::calculateSignal(95.0, 100.0, 120.0, indicatorSnapshot, defaults).signal, "Buy", "Default signal rules keep Buy output unchanged");
    expectSignalEquivalent(
        WatchlistSignalService::calculateSignal(95.0, 100.0, 120.0, indicatorSnapshot, defaults),
        WatchlistSignalService::calculateSignal(95.0, 100.0, 120.0, indicatorSnapshot),
        "Explicit defaults match no-rules Buy path");
    expectSignalEquivalent(
        WatchlistSignalService::calculateSignal(130.0, 100.0, 120.0, indicatorSnapshot, defaults),
        WatchlistSignalService::calculateSignal(130.0, 100.0, 120.0, indicatorSnapshot),
        "Explicit defaults match no-rules Sell path");
    expectSignalEquivalent(
        WatchlistSignalService::calculateSignal(95.0, 100.0, 120.0, std::nullopt, defaults),
        WatchlistSignalService::calculateSignal(95.0, 100.0, 120.0, std::nullopt),
        "Explicit defaults match no-rules missing indicator path");
    expectSignalEquivalent(
        WatchlistSignalService::calculateSignal(0.0, 100.0, 120.0, indicatorSnapshot, defaults),
        WatchlistSignalService::calculateSignal(0.0, 100.0, 120.0, indicatorSnapshot),
        "Explicit defaults match no-rules missing price path");

    TechnicalIndicatorSnapshot weakRsi = snapshot;
    weakRsi.rsi14 = 70.0;
    const std::optional<TechnicalIndicatorSnapshot> weakRsiSnapshot = weakRsi;
    expectSignalEquivalent(
        WatchlistSignalService::calculateSignal(95.0, 100.0, 120.0, weakRsiSnapshot, defaults),
        WatchlistSignalService::calculateSignal(95.0, 100.0, 120.0, weakRsiSnapshot),
        "Explicit defaults match no-rules RSI rejection path");

    TechnicalIndicatorSnapshot weakMacd = snapshot;
    weakMacd.macdHistogram = -0.01;
    const std::optional<TechnicalIndicatorSnapshot> weakMacdSnapshot = weakMacd;
    expectSignalEquivalent(
        WatchlistSignalService::calculateSignal(95.0, 100.0, 120.0, weakMacdSnapshot, defaults),
        WatchlistSignalService::calculateSignal(95.0, 100.0, 120.0, weakMacdSnapshot),
        "Explicit defaults match no-rules MACD rejection path");

    SignalRules stricter = defaults;
    stricter.rsiBuyMin = 55.0;
    expectEqual(WatchlistSignalService::calculateSignal(95.0, 100.0, 120.0, indicatorSnapshot, stricter).signal, "Hold", "Changed RSI threshold uses the same Buy formula");

    stricter = defaults;
    stricter.macdHistogramMin = 0.30;
    expectEqual(WatchlistSignalService::calculateSignal(95.0, 100.0, 120.0, indicatorSnapshot, stricter).signal, "Hold", "Changed MACD histogram threshold uses the same Buy formula");
}

void testSignalRulesPersistence()
{
    Database database;
    std::string error;
    expectTrue(database.open(":memory:"), "In-memory database opens for signal rules settings");
    expectTrue(Migrations::run(database, error), "Migrations run for signal rules settings: " + error);

    AppSettingsRepository settingsRepository(database);
    const SignalRules missingSettings = SignalRulesService::load(settingsRepository, error);
    expectTrue(error.empty(), "Missing signal rule settings load defaults without error: " + error);
    expectNear(missingSettings.rsiBuyMin, SignalRulesDefaults::RsiBuyMin, "Missing settings fallback restores default RSI lower threshold");
    expectNear(missingSettings.rsiBuyMax, SignalRulesDefaults::RsiBuyMax, "Missing settings fallback restores default RSI upper threshold");
    expectNear(missingSettings.macdHistogramMin, SignalRulesDefaults::MacdHistogramMin, "Missing settings fallback restores default MACD histogram threshold");
    expectEqual(missingSettings.momentumLookbackSessions, SignalRulesDefaults::MomentumLookbackSessions, "Missing settings fallback restores default Momentum lookback");

    SignalRules rules = SignalRulesService::defaults();
    rules.rsiBuyMin = 35.5;
    rules.rsiBuyMax = 65.5;
    rules.macdHistogramMin = -0.125;
    rules.momentumLookbackSessions = 10;

    expectTrue(SignalRulesService::save(settingsRepository, rules, error), "Signal rules save to app_settings: " + error);
    const SignalRules loaded = SignalRulesService::load(settingsRepository, error);
    expectTrue(error.empty(), "Signal rules reload without error: " + error);
    expectNear(loaded.rsiBuyMin, 35.5, "Saved RSI lower threshold reloads");
    expectNear(loaded.rsiBuyMax, 65.5, "Saved RSI upper threshold reloads");
    expectNear(loaded.macdHistogramMin, -0.125, "Saved MACD histogram threshold reloads");
    expectEqual(loaded.momentumLookbackSessions, 10, "Saved Momentum lookback reloads");

    expectTrue(SignalRulesService::resetToDefaults(settingsRepository, error), "Signal rules reset persists defaults: " + error);
    const SignalRules reset = SignalRulesService::load(settingsRepository, error);
    expectTrue(error.empty(), "Reset signal rules reload without error: " + error);
    expectNear(reset.rsiBuyMin, SignalRulesDefaults::RsiBuyMin, "Reset RSI lower threshold restores default");
    expectNear(reset.rsiBuyMax, SignalRulesDefaults::RsiBuyMax, "Reset RSI upper threshold restores default");
    expectNear(reset.macdHistogramMin, SignalRulesDefaults::MacdHistogramMin, "Reset MACD histogram threshold restores default");
    expectEqual(reset.momentumLookbackSessions, SignalRulesDefaults::MomentumLookbackSessions, "Reset Momentum lookback restores default");

    expectTrue(settingsRepository.setString("signal_rules.rsi_buy_min", "70.0", error), "Invalid settings test writes RSI lower: " + error);
    expectTrue(settingsRepository.setString("signal_rules.rsi_buy_max", "40.0", error), "Invalid settings test writes RSI upper: " + error);
    const SignalRules invalidReload = SignalRulesService::load(settingsRepository, error);
    expectTrue(!error.empty(), "Invalid saved signal rules produce a non-blocking load warning");
    expectNear(invalidReload.rsiBuyMin, SignalRulesDefaults::RsiBuyMin, "Invalid saved settings fallback restores default RSI lower threshold");
    expectNear(invalidReload.rsiBuyMax, SignalRulesDefaults::RsiBuyMax, "Invalid saved settings fallback restores default RSI upper threshold");
}

void testWatchlistRepositoryActions()
{
    Database database;
    std::string error;
    expectTrue(database.open(":memory:"), "In-memory database opens");
    expectTrue(Migrations::run(database, error), "Migrations run for watchlist action smoke test: " + error);

    WatchlistRepository repository(database);
    int watchlistId = 0;
    expectTrue(repository.defaultWatchlistId(watchlistId, error), "Default watchlist id is available: " + error);

    WatchlistItem item;
    item.watchlistId = watchlistId;
    item.ticker = "abc";
    item.assetName = "ABC Corp";
    item.assetType = "Stock";
    item.priority = "Medium";
    item.buySignalPrice = 90.0;
    item.sellSignalPrice = 120.0;
    item.currentPrice = 100.0;
    item.signalStatus = "Hold";

    expectTrue(repository.create(item, error), "Watchlist item create still works: " + error);
    item.assetName = "ABC Corporation";
    expectTrue(repository.update(item, error), "Watchlist item update still works: " + error);

    const std::vector<WatchlistItem> items = repository.listByWatchlist(watchlistId, error);
    expectTrue(error.empty(), "Watchlist item list does not error: " + error);
    expectTrue(!items.empty() && items.front().ticker == "ABC" && items.front().assetName == "ABC Corporation", "Watchlist item persists normalized ticker and update");

    expectTrue(repository.remove(item.id, error), "Watchlist item remove still works: " + error);
    const std::vector<WatchlistItem> afterRemove = repository.listByWatchlist(watchlistId, error);
    expectTrue(afterRemove.empty(), "Watchlist item remove leaves selected watchlist empty");
}

} // namespace

int main()
{
    testIndicatorDisplayText();
    testMomentumDisplayText();
    testConfigurableMomentumLookback();
    testTechnicalIndicatorCalculationFixture();
    testWatchlistRowDisplayValues();
    testSignalRulesDefaultsAndValidation();
    testSignalOutputUnchanged();
    testSignalRulesPersistence();
    testWatchlistRepositoryActions();

    if (Failures > 0) {
        std::cerr << Failures << " test failure(s).\n";
        return 1;
    }

    std::cout << "Watchlist indicator display tests passed.\n";
    return 0;
}
