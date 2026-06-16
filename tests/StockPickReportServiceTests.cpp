// SPDX-License-Identifier: MIT
#include "app/AppState.hpp"
#include "services/StockPickReportDocxExporter.hpp"
#include "services/StockPickReportPdfExporter.hpp"
#include "services/StockPickReportService.hpp"
#include "util/Date.hpp"

#include <array>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

namespace {

void expectTrue(bool condition, const char* message)
{
    if (!condition) {
        std::cerr << message << '\n';
        std::exit(1);
    }
}

void expectNear(double actual, double expected, const char* message)
{
    if (std::fabs(actual - expected) > 0.005) {
        std::cerr << message << " Expected " << expected << ", got " << actual << '\n';
        std::exit(1);
    }
}

const StockPickResearchReportHoldingRow* rowForTicker(const StockPickResearchReport& report, const std::string& ticker)
{
    for (const StockPickResearchReportHoldingRow& row : report.holdings) {
        if (row.ticker == ticker) {
            return &row;
        }
    }
    return nullptr;
}

AppState sampleState()
{
    AppState state;
    state.accounts.push_back(Account { 1, "Trading Account", "Brokerage", "Test Broker", 0.0, 500.0, {}, "Active" });
    state.accounts.push_back(Account { 2, "Other Account", "Brokerage", "Test Broker", 0.0, 0.0, {}, "Active" });

    Holding tracked;
    tracked.id = 1;
    tracked.accountId = 1;
    tracked.ticker = "VALA";
    tracked.assetName = "Validation Alpha Corp";
    tracked.shares = 10.0;
    tracked.averageCost = 18.0;
    tracked.currentPrice = 25.0;
    tracked.targetAllocationPercent = 40.0;
    tracked.status = "Active";
    state.holdings.push_back(tracked);

    Holding missingPrice;
    missingPrice.id = 2;
    missingPrice.accountId = 1;
    missingPrice.ticker = "MISS";
    missingPrice.assetName = "Missing Price Industries";
    missingPrice.shares = 5.0;
    missingPrice.averageCost = 12.0;
    missingPrice.currentPrice = 0.0;
    missingPrice.status = "Active";
    state.holdings.push_back(missingPrice);

    Holding otherAccount;
    otherAccount.id = 3;
    otherAccount.accountId = 2;
    otherAccount.ticker = "OTHR";
    otherAccount.assetName = "Other Account Holding";
    otherAccount.shares = 1.0;
    otherAccount.averageCost = 1.0;
    otherAccount.currentPrice = 9999.0;
    otherAccount.targetAllocationPercent = 100.0;
    otherAccount.status = "Active";
    state.holdings.push_back(otherAccount);

    WatchlistItem item;
    item.id = 1;
    item.ticker = "VALA";
    item.assetName = "Validation Alpha Corp";
    item.buySignalPrice = 30.0;
    item.sellSignalPrice = 60.0;
    item.currentPrice = 25.0;
    item.signalStatus = "Buy";
    item.priority = "High";
    state.watchlist.push_back(item);

    return state;
}

std::string readBinaryFile(const std::filesystem::path& path)
{
    std::ifstream file(path, std::ios::binary);
    return std::string(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>());
}

void expectContains(const std::string& haystack, const std::string& needle, const char* message)
{
    if (haystack.find(needle) == std::string::npos) {
        std::cerr << message << '\n';
        std::exit(1);
    }
}

void expectOccurrencesAtLeast(const std::string& haystack, const std::string& needle, int minimum, const char* message)
{
    int count = 0;
    std::size_t position = 0;
    while ((position = haystack.find(needle, position)) != std::string::npos) {
        ++count;
        position += needle.size();
    }
    if (count < minimum) {
        std::cerr << message << " Expected at least " << minimum << ", got " << count << '\n';
        std::exit(1);
    }
}

bool keepReportTestOutput()
{
#ifdef _MSC_VER
    char* value = nullptr;
    std::size_t length = 0;
    const bool found = _dupenv_s(&value, &length, "ICC_KEEP_REPORT_TEST_OUTPUT") == 0 && value != nullptr && length > 0;
    std::free(value);
    return found;
#else
    return std::getenv("ICC_KEEP_REPORT_TEST_OUTPUT") != nullptr;
#endif
}

void validateDocxExport(const StockPickResearchReport& report)
{
    const std::string expectedFileName =
        "ICC_Weekly_Stock_Pick_Helper_Trading_Account_" + Date::todayIso8601() + ".docx";
    expectTrue(StockPickReportDocxExporter::defaultFileName(report) == expectedFileName, "Default weekly DOCX filename should match the requested format.");

    StockPickResearchReport filenameReport = report;
    filenameReport.accountName = "Trading: Account / Test";
    expectTrue(
        StockPickReportDocxExporter::defaultFileName(filenameReport) ==
            "ICC_Weekly_Stock_Pick_Helper_Trading_Account_Test_" + Date::todayIso8601() + ".docx",
        "Default DOCX filename should sanitize account names.");

    const std::filesystem::path outputPath = std::filesystem::temp_directory_path() / expectedFileName;
    std::error_code removeError;
    std::filesystem::remove(outputPath, removeError);

    std::string exportError;
    expectTrue(StockPickReportDocxExporter::exportDocx(report, outputPath, exportError), exportError.c_str());
    expectTrue(std::filesystem::exists(outputPath), "DOCX export should create a file.");

    const std::string contents = readBinaryFile(outputPath);
    expectContains(contents, "Weekly Stock Pick Helper Research Report", "DOCX should include the report title.");
    expectContains(contents, "Selected account: Trading Account", "DOCX should include the selected account.");
    expectContains(contents, "Total account value", "DOCX should include the account summary.");
    expectContains(contents, "Validation Alpha Corp", "DOCX should include tracked holdings.");
    expectContains(contents, "Not Tracked", "DOCX should include untracked signal fallback.");
    expectContains(contents, "N/A", "DOCX should include missing-value fallback text.");
    expectContains(contents, "For personal research only. Not financial advice.", "DOCX should include the disclaimer.");

    const std::filesystem::path wrongExtensionPath = std::filesystem::temp_directory_path() / "ICC_Report_Validation.txt";
    const std::filesystem::path forcedDocxPath = wrongExtensionPath;
    std::filesystem::remove(wrongExtensionPath, removeError);
    std::filesystem::remove(forcedDocxPath.parent_path() / "ICC_Report_Validation.docx", removeError);
    expectTrue(StockPickReportDocxExporter::exportDocx(report, wrongExtensionPath, exportError), "DOCX export should normalize non-DOCX extensions.");
    expectTrue(std::filesystem::exists(forcedDocxPath.parent_path() / "ICC_Report_Validation.docx"), "DOCX export should write with a .docx extension.");
    std::filesystem::remove(forcedDocxPath.parent_path() / "ICC_Report_Validation.docx", removeError);

    if (!keepReportTestOutput()) {
        std::filesystem::remove(outputPath, removeError);
    } else {
        std::cout << "DOCX export path: " << outputPath.string() << '\n';
    }
}

StockPickResearchReport manyHoldingReport(const StockPickResearchReport& report)
{
    StockPickResearchReport many = report;
    many.holdings.clear();
    const std::array<std::string, 4> signals { "Buy", "Sell", "Hold", "Not Tracked" };
    for (int index = 0; index < 64; ++index) {
        StockPickResearchReportHoldingRow row = report.holdings[static_cast<std::size_t>(index) % report.holdings.size()];
        row.ticker = "ROW" + std::to_string(index + 1);
        row.assetName = "Validation Multi Page Holding " + std::to_string(index + 1);
        row.signalStatus = signals[static_cast<std::size_t>(index) % signals.size()];
        row.tracked = row.signalStatus != "Not Tracked";
        row.hasCurrentPrice = index % 13 != 0;
        row.currentPrice = row.hasCurrentPrice ? 10.0 + static_cast<double>(index) : 0.0;
        row.hasMarketValue = row.hasCurrentPrice;
        row.marketValue = row.hasMarketValue ? row.currentPrice * row.shares : 0.0;
        row.hasBuyLevel = index % 5 != 0;
        row.buyLevel = row.hasBuyLevel ? 8.0 + static_cast<double>(index) : 0.0;
        row.priority = index % 3 == 0 ? "High" : (index % 3 == 1 ? "Medium" : "Low");
        row.hasPriority = true;
        many.holdings.push_back(row);
    }
    many.holdingCount = static_cast<int>(many.holdings.size());
    return many;
}

void validatePdfExport(const StockPickResearchReport& weekly, const StockPickResearchReport& monthly)
{
    const std::string weeklyFileName =
        "ICC_Weekly_Stock_Pick_Helper_Trading_Account_" + Date::todayIso8601() + ".pdf";
    const std::string monthlyFileName =
        "ICC_Monthly_Stock_Pick_Helper_Trading_Account_" + Date::todayIso8601() + ".pdf";
    expectTrue(StockPickReportPdfExporter::defaultFileName(weekly) == weeklyFileName, "Default weekly PDF filename should match the requested format.");
    expectTrue(StockPickReportPdfExporter::defaultFileName(monthly) == monthlyFileName, "Default monthly PDF filename should match the requested format.");

    StockPickResearchReport filenameReport = weekly;
    filenameReport.accountName = "Trading: Account / Test";
    expectTrue(
        StockPickReportPdfExporter::defaultFileName(filenameReport) ==
            "ICC_Weekly_Stock_Pick_Helper_Trading_Account_Test_" + Date::todayIso8601() + ".pdf",
        "Default PDF filename should sanitize account names.");

    const std::filesystem::path outputPath = std::filesystem::temp_directory_path() / weeklyFileName;
    std::error_code removeError;
    std::filesystem::remove(outputPath, removeError);

    std::string exportError;
    expectTrue(StockPickReportPdfExporter::exportPdf(weekly, outputPath, exportError), exportError.c_str());
    expectTrue(std::filesystem::exists(outputPath), "PDF export should create a file.");

    const std::string contents = readBinaryFile(outputPath);
    expectContains(contents, "%PDF-1.4", "PDF should have a valid PDF header.");
    expectContains(contents, "Weekly Stock Pick Helper Research Report", "PDF should include the report title.");
    expectContains(contents, "Selected account: Trading Account", "PDF should include the selected account.");
    expectContains(contents, "Total Account Value", "PDF should include the account summary.");
    expectContains(contents, "$750.00", "PDF should include formatted account summary values.");
    expectContains(contents, "Validation Alpha Corp", "PDF should include tracked holdings.");
    expectContains(contents, "Buy", "PDF should include signal labels.");
    expectContains(contents, "Not Tracked", "PDF should include untracked signal fallback.");
    expectContains(contents, "N/A", "PDF should include missing-value fallback text.");
    expectContains(contents, "For personal research only. Not financial advice.", "PDF should include the disclaimer.");

    const std::filesystem::path monthlyOutputPath = std::filesystem::temp_directory_path() / monthlyFileName;
    std::filesystem::remove(monthlyOutputPath, removeError);
    expectTrue(StockPickReportPdfExporter::exportPdf(monthly, monthlyOutputPath, exportError), "Monthly PDF export should succeed.");
    const std::string monthlyContents = readBinaryFile(monthlyOutputPath);
    expectContains(monthlyContents, "Monthly Stock Pick Helper Research Report", "Monthly PDF should include the monthly report title.");
    if (!keepReportTestOutput()) {
        std::filesystem::remove(monthlyOutputPath, removeError);
    } else {
        std::cout << "Monthly PDF export path: " << monthlyOutputPath.string() << '\n';
    }

    const std::filesystem::path wrongExtensionPath = std::filesystem::temp_directory_path() / "ICC_Report_Validation.txt";
    const std::filesystem::path forcedPdfPath = wrongExtensionPath.parent_path() / "ICC_Report_Validation.pdf";
    std::filesystem::remove(wrongExtensionPath, removeError);
    std::filesystem::remove(forcedPdfPath, removeError);
    expectTrue(StockPickReportPdfExporter::exportPdf(weekly, wrongExtensionPath, exportError), "PDF export should normalize non-PDF extensions.");
    expectTrue(std::filesystem::exists(forcedPdfPath), "PDF export should write with a .pdf extension.");
    std::filesystem::remove(forcedPdfPath, removeError);

    const StockPickResearchReport many = manyHoldingReport(weekly);
    const std::filesystem::path multiPagePath = std::filesystem::temp_directory_path() / "ICC_Report_Multi_Page_Validation.pdf";
    std::filesystem::remove(multiPagePath, removeError);
    expectTrue(StockPickReportPdfExporter::exportPdf(many, multiPagePath, exportError), "Multi-page PDF export should succeed.");
    const std::string multiPageContents = readBinaryFile(multiPagePath);
    expectContains(multiPageContents, "Validation Multi Page Holding 64", "Multi-page PDF should include the final holding.");
    expectOccurrencesAtLeast(multiPageContents, "Ticker", 2, "Multi-page PDF should repeat table headers.");
    expectOccurrencesAtLeast(multiPageContents, "/Type /Page", 2, "Multi-page PDF should contain multiple page objects.");
    if (!keepReportTestOutput()) {
        std::filesystem::remove(multiPagePath, removeError);
    } else {
        std::cout << "Multi-page PDF export path: " << multiPagePath.string() << '\n';
    }

    if (!keepReportTestOutput()) {
        std::filesystem::remove(outputPath, removeError);
    } else {
        std::cout << "PDF export path: " << outputPath.string() << '\n';
    }
}

} // namespace

int main()
{
    const AppState state = sampleState();
    const std::optional<StockPickResearchReport> weekly = StockPickReportService::buildResearchReport(state, 1, StockPickReportPeriod::Weekly);
    expectTrue(weekly.has_value(), "Weekly report should be generated for a valid account.");
    expectTrue(weekly->title == "Weekly Stock Pick Helper Research Report", "Weekly title should match requested wording.");
    expectTrue(weekly->accountName == "Trading Account", "Report should use the selected account name.");
    expectNear(weekly->totalAccountValue, 750.0, "Total account value should be selected account holdings plus cash.");
    expectNear(weekly->holdingsValue, 250.0, "Holdings value should include only selected account priced holdings.");
    expectNear(weekly->cashBalance, 500.0, "Cash balance should come from the selected account.");
    expectTrue(weekly->holdingCount == 2, "Report should count only selected account holdings.");
    expectTrue(rowForTicker(*weekly, "OTHR") == nullptr, "Report must not include holdings from other accounts.");

    const StockPickResearchReportHoldingRow* tracked = rowForTicker(*weekly, "VALA");
    expectTrue(tracked != nullptr, "Tracked selected-account holding should appear.");
    expectTrue(tracked->hasCurrentPrice, "Tracked holding should have current price.");
    expectNear(tracked->currentAllocationPercent, 33.333333, "Allocation should use selected account total value.");
    expectTrue(tracked->hasTargetAllocation, "Target allocation should be available when set.");
    expectTrue(tracked->hasAllocationDifference, "Allocation difference should be available when target is set.");
    expectTrue(tracked->signalStatus == "Buy", "Tracked holding should use watchlist signal status.");
    expectTrue(tracked->hasBuyLevel && tracked->buyLevel == 30.0, "Tracked holding should include buy level.");
    expectTrue(tracked->hasPriority && tracked->priority == "High", "Tracked holding should include priority.");

    const StockPickResearchReportHoldingRow* missing = rowForTicker(*weekly, "MISS");
    expectTrue(missing != nullptr, "Missing-price selected-account holding should appear.");
    expectTrue(!missing->hasCurrentPrice, "Missing current price should be represented as unavailable.");
    expectTrue(!missing->tracked && missing->signalStatus == "Not Tracked", "Untracked holding should report Not Tracked.");

    const std::optional<StockPickResearchReport> monthly = StockPickReportService::buildResearchReport(state, 1, StockPickReportPeriod::Monthly);
    expectTrue(monthly.has_value(), "Monthly report should be generated for a valid account.");
    expectTrue(monthly->title == "Monthly Stock Pick Helper Research Report", "Monthly title should match requested wording.");
    expectTrue(!StockPickReportService::buildResearchReport(state, 999, StockPickReportPeriod::Weekly).has_value(), "Missing account should not generate a report.");
    validateDocxExport(*weekly);
    validatePdfExport(*weekly, *monthly);

    std::cout << "Stock pick report service tests passed.\n";
    return 0;
}
