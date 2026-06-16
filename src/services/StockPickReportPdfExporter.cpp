// SPDX-License-Identifier: MIT
#include "services/StockPickReportPdfExporter.hpp"

#include "services/StockPickReportExportCommon.hpp"
#include "util/Money.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

namespace {

struct PdfColor {
    double red = 0.0;
    double green = 0.0;
    double blue = 0.0;
};

struct PdfCell {
    std::string text;
    double width = 0.0;
    bool numeric = false;
    bool bold = false;
    PdfColor textColor { 0.11, 0.15, 0.20 };
    PdfColor fillColor { 1.0, 1.0, 1.0 };
    bool hasFill = false;
};

constexpr double PageWidth = 792.0;
constexpr double PageHeight = 612.0;
constexpr double Margin = 36.0;
constexpr double TableWidth = PageWidth - Margin * 2.0;
constexpr double TopY = PageHeight - Margin;
constexpr double FooterLineY = 42.0;
constexpr double BottomContentY = 58.0;
constexpr double HeaderRowHeight = 25.0;
constexpr double DataRowHeight = 18.0;

const PdfColor Black { 0.10, 0.13, 0.17 };
const PdfColor Muted { 0.45, 0.51, 0.58 };
const PdfColor HeaderFill { 0.08, 0.12, 0.20 };
const PdfColor HeaderText { 0.86, 0.92, 1.0 };
const PdfColor Border { 0.74, 0.80, 0.86 };
const PdfColor RowAlt { 0.96, 0.98, 1.0 };
const PdfColor BuyText { 0.00, 0.48, 0.30 };
const PdfColor SellText { 0.78, 0.18, 0.35 };
const PdfColor HoldText { 0.10, 0.45, 0.70 };
const PdfColor BuyFill { 0.91, 0.97, 0.94 };
const PdfColor SellFill { 0.99, 0.92, 0.95 };
const PdfColor HoldFill { 0.92, 0.96, 0.99 };
const PdfColor NotTrackedFill { 0.94, 0.95, 0.96 };

std::string formatPdfNumber(double value)
{
    std::ostringstream stream;
    stream << std::fixed << std::setprecision(2) << value;
    std::string result = stream.str();
    while (result.size() > 1 && result.back() == '0') {
        result.pop_back();
    }
    if (!result.empty() && result.back() == '.') {
        result.pop_back();
    }
    return result;
}

std::string colorCommand(const PdfColor& color, const char* command)
{
    std::ostringstream stream;
    stream << formatPdfNumber(color.red) << ' '
           << formatPdfNumber(color.green) << ' '
           << formatPdfNumber(color.blue) << ' '
           << command << '\n';
    return stream.str();
}

std::string printableText(std::string_view value)
{
    std::string result;
    result.reserve(value.size());
    bool previousWasSpace = false;
    for (const unsigned char character : value) {
        char output = '?';
        if (character == '\r' || character == '\n' || character == '\t') {
            output = ' ';
        } else if (character >= 32 && character <= 126) {
            output = static_cast<char>(character);
        }

        if (output == ' ') {
            if (!previousWasSpace) {
                result += output;
                previousWasSpace = true;
            }
        } else {
            result += output;
            previousWasSpace = false;
        }
    }
    return result;
}

std::string escapePdfText(std::string_view value)
{
    const std::string printable = printableText(value);
    std::string escaped;
    escaped.reserve(printable.size());
    for (const char character : printable) {
        if (character == '(' || character == ')' || character == '\\') {
            escaped += '\\';
        }
        escaped += character;
    }
    return escaped;
}

double approximateTextWidth(std::string_view value, double fontSize)
{
    double units = 0.0;
    for (const unsigned char character : value) {
        if (character == ' ') {
            units += 0.26;
        } else if (std::isdigit(character) != 0) {
            units += 0.53;
        } else if (std::isupper(character) != 0) {
            units += 0.60;
        } else if (character == '.' || character == ',' || character == '%' || character == '$' || character == '/' || character == '-') {
            units += 0.32;
        } else {
            units += 0.50;
        }
    }
    return units * fontSize;
}

std::string truncateToWidth(const std::string& value, double maxWidth, double fontSize)
{
    std::string text = printableText(value);
    if (approximateTextWidth(text, fontSize) <= maxWidth) {
        return text;
    }

    const std::string suffix = "...";
    while (!text.empty() && approximateTextWidth(text + suffix, fontSize) > maxWidth) {
        text.pop_back();
    }
    return text.empty() ? suffix : text + suffix;
}

void addRect(std::ostringstream& content, double x, double y, double width, double height, const PdfColor& fill)
{
    content << colorCommand(fill, "rg")
            << formatPdfNumber(x) << ' ' << formatPdfNumber(y) << ' '
            << formatPdfNumber(width) << ' ' << formatPdfNumber(height) << " re f\n";
}

void addStrokeRect(std::ostringstream& content, double x, double y, double width, double height, const PdfColor& color)
{
    content << "0.45 w\n"
            << colorCommand(color, "RG")
            << formatPdfNumber(x) << ' ' << formatPdfNumber(y) << ' '
            << formatPdfNumber(width) << ' ' << formatPdfNumber(height) << " re S\n";
}

void addLine(std::ostringstream& content, double x1, double y1, double x2, double y2, const PdfColor& color)
{
    content << "0.45 w\n"
            << colorCommand(color, "RG")
            << formatPdfNumber(x1) << ' ' << formatPdfNumber(y1) << " m "
            << formatPdfNumber(x2) << ' ' << formatPdfNumber(y2) << " l S\n";
}

void addText(std::ostringstream& content, double x, double y, const std::string& font, double fontSize, const PdfColor& color, const std::string& text)
{
    content << colorCommand(color, "rg")
            << "BT /" << font << ' ' << formatPdfNumber(fontSize) << " Tf "
            << formatPdfNumber(x) << ' ' << formatPdfNumber(y) << " Td ("
            << escapePdfText(text) << ") Tj ET\n";
}

std::vector<std::string> wrapText(const std::string& value, double maxWidth, double fontSize, int maxLines)
{
    std::vector<std::string> words;
    std::istringstream input(printableText(value));
    std::string word;
    while (input >> word) {
        words.push_back(word);
    }

    std::vector<std::string> lines;
    std::string current;
    for (const std::string& next : words) {
        const std::string candidate = current.empty() ? next : current + " " + next;
        if (!current.empty() && approximateTextWidth(candidate, fontSize) > maxWidth) {
            lines.push_back(current);
            current = next;
            if (static_cast<int>(lines.size()) == maxLines) {
                break;
            }
        } else {
            current = candidate;
        }
    }
    if (!current.empty() && static_cast<int>(lines.size()) < maxLines) {
        lines.push_back(current);
    }
    if (lines.empty()) {
        lines.push_back(truncateToWidth(value, maxWidth, fontSize));
    }
    if (static_cast<int>(lines.size()) == maxLines && approximateTextWidth(lines.back(), fontSize) > maxWidth) {
        lines.back() = truncateToWidth(lines.back(), maxWidth, fontSize);
    }
    return lines;
}

void drawCell(std::ostringstream& content, double x, double topY, double height, const PdfCell& cell, bool header)
{
    const double bottomY = topY - height;
    if (cell.hasFill || header) {
        addRect(content, x, bottomY, cell.width, height, header ? HeaderFill : cell.fillColor);
    }
    addStrokeRect(content, x, bottomY, cell.width, height, Border);

    const double fontSize = header ? 6.3 : 6.7;
    const std::string font = (header || cell.bold) ? "F2" : "F1";
    const PdfColor textColor = header ? HeaderText : cell.textColor;
    const double maxTextWidth = std::max(4.0, cell.width - 7.0);

    if (header) {
        const std::vector<std::string> lines = wrapText(cell.text, maxTextWidth, fontSize, 2);
        const double firstBaseline = lines.size() > 1 ? topY - 9.0 : topY - 14.5;
        for (std::size_t index = 0; index < lines.size(); ++index) {
            addText(content, x + 3.5, firstBaseline - static_cast<double>(index) * 8.0, font, fontSize, textColor, truncateToWidth(lines[index], maxTextWidth, fontSize));
        }
        return;
    }

    const std::string text = truncateToWidth(cell.text, maxTextWidth, fontSize);
    double textX = x + 3.5;
    if (cell.numeric) {
        textX = x + cell.width - 3.5 - approximateTextWidth(text, fontSize);
        textX = std::max(textX, x + 3.5);
    }
    addText(content, textX, topY - 11.7, font, fontSize, textColor, text);
}

void drawRow(std::ostringstream& content, double topY, const std::vector<PdfCell>& cells, double rowHeight, bool header)
{
    double x = Margin;
    for (const PdfCell& cell : cells) {
        drawCell(content, x, topY, rowHeight, cell, header);
        x += cell.width;
    }
}

void drawSectionTitle(std::ostringstream& content, double x, double y, const std::string& text)
{
    addText(content, x, y, "F2", 10.5, PdfColor { 0.12, 0.31, 0.48 }, text);
}

void drawFooter(std::ostringstream& content, int pageNumber)
{
    addLine(content, Margin, FooterLineY, PageWidth - Margin, FooterLineY, PdfColor { 0.82, 0.86, 0.90 });
    addText(content, Margin, 25.0, "F1", 8.0, Muted, "For personal research only. Not financial advice.");
    addText(content, PageWidth - Margin - 44.0, 25.0, "F1", 8.0, Muted, "Page " + std::to_string(pageNumber));
}

std::vector<PdfCell> holdingsHeaderCells()
{
    const std::array<double, 11> widths {
        45.0, 134.0, 50.0, 58.0, 65.0, 62.0, 62.0, 64.0, 64.0, 58.0, 58.0,
    };
    const std::array<const char*, 11> labels {
        "Ticker",
        "Company / Name",
        "Shares",
        "Current Price",
        "Market Value",
        "Current Allocation %",
        "Target Allocation %",
        "Allocation Difference",
        "Current Signal",
        "Buy Level",
        "Priority",
    };

    std::vector<PdfCell> cells;
    cells.reserve(widths.size());
    for (std::size_t index = 0; index < widths.size(); ++index) {
        cells.push_back(PdfCell { labels[index], widths[index], false, true });
    }
    return cells;
}

std::string optionalMoney(double value, bool available)
{
    return available ? Money::format(value) : "N/A";
}

std::string optionalPercent(double value, bool available, bool includeSign = false)
{
    return available ? Money::formatPercent(value, includeSign) : "N/A";
}

PdfColor signalTextColor(const std::string& signal)
{
    if (signal == "Buy") {
        return BuyText;
    }
    if (signal == "Sell") {
        return SellText;
    }
    if (signal == "Not Tracked") {
        return Muted;
    }
    return HoldText;
}

PdfColor signalFillColor(const std::string& signal)
{
    if (signal == "Buy") {
        return BuyFill;
    }
    if (signal == "Sell") {
        return SellFill;
    }
    if (signal == "Not Tracked") {
        return NotTrackedFill;
    }
    return HoldFill;
}

std::vector<PdfCell> holdingsRowCells(const StockPickResearchReportHoldingRow& row, bool alternateFill)
{
    const std::array<double, 11> widths {
        45.0, 134.0, 50.0, 58.0, 65.0, 62.0, 62.0, 64.0, 64.0, 58.0, 58.0,
    };
    const std::array<std::string, 11> values {
        row.ticker.empty() ? "N/A" : row.ticker,
        row.assetName.empty() ? "N/A" : row.assetName,
        Money::formatQuantity(row.shares),
        optionalMoney(row.currentPrice, row.hasCurrentPrice),
        optionalMoney(row.marketValue, row.hasMarketValue),
        Money::formatPercent(row.currentAllocationPercent),
        optionalPercent(row.targetAllocationPercent, row.hasTargetAllocation),
        optionalPercent(row.allocationDifferencePercent, row.hasAllocationDifference, true),
        row.signalStatus,
        optionalMoney(row.buyLevel, row.hasBuyLevel),
        row.hasPriority ? row.priority : "N/A",
    };

    std::vector<PdfCell> cells;
    cells.reserve(values.size());
    for (std::size_t index = 0; index < values.size(); ++index) {
        PdfCell cell;
        cell.text = values[index];
        cell.width = widths[index];
        cell.numeric = (index >= 2 && index <= 7) || index == 9;
        cell.hasFill = alternateFill;
        cell.fillColor = RowAlt;
        if (index == 8) {
            cell.textColor = signalTextColor(row.signalStatus);
            cell.fillColor = signalFillColor(row.signalStatus);
            cell.hasFill = true;
            cell.bold = row.signalStatus != "Not Tracked";
        } else if ((!row.hasCurrentPrice && (index == 3 || index == 4)) || (index == 6 && !row.hasTargetAllocation) ||
            (index == 7 && !row.hasAllocationDifference) || (index == 9 && !row.hasBuyLevel) || (index == 10 && !row.hasPriority)) {
            cell.textColor = Muted;
        }
        cells.push_back(cell);
    }
    return cells;
}

void drawSummary(std::ostringstream& content, double& y, const StockPickResearchReport& report)
{
    drawSectionTitle(content, Margin, y, "Account Summary");
    y -= 11.0;

    const double cellWidth = TableWidth / 4.0;
    const std::array<std::string, 4> labels {
        "Total Account Value",
        "Holdings Value",
        "Cash Balance",
        "Number of Holdings",
    };
    const std::array<std::string, 4> values {
        Money::format(report.totalAccountValue),
        Money::format(report.holdingsValue),
        Money::format(report.cashBalance),
        std::to_string(report.holdingCount),
    };

    std::vector<PdfCell> labelCells;
    std::vector<PdfCell> valueCells;
    for (std::size_t index = 0; index < labels.size(); ++index) {
        labelCells.push_back(PdfCell { labels[index], cellWidth, false, true });
        PdfCell valueCell { values[index], cellWidth, true, false };
        valueCell.textColor = Black;
        valueCells.push_back(valueCell);
    }

    drawRow(content, y, labelCells, 18.0, true);
    y -= 18.0;
    drawRow(content, y, valueCells, 20.0, false);
    y -= 30.0;
}

void drawReportTitle(std::ostringstream& content, const StockPickResearchReport& report, bool continued)
{
    addText(content, Margin, TopY, "F2", continued ? 12.0 : 16.0, Black, report.title + (continued ? " (continued)" : ""));
    addText(content, Margin, TopY - 16.0, "F1", 8.7, Muted, "Generated: " + report.generatedAt);
    addText(content, Margin, TopY - 28.0, "F1", 8.7, Muted, "Selected account: " + report.accountName);
}

void drawHoldingsHeader(std::ostringstream& content, double& y)
{
    drawSectionTitle(content, Margin, y, "Holdings / Allocation / Signals");
    y -= 11.0;
    drawRow(content, y, holdingsHeaderCells(), HeaderRowHeight, true);
    y -= HeaderRowHeight;
}

void beginPage(std::ostringstream& content, const StockPickResearchReport& report, bool firstPage, double& y)
{
    content.str(std::string());
    content.clear();
    drawReportTitle(content, report, !firstPage);
    y = firstPage ? TopY - 52.0 : TopY - 44.0;
    if (firstPage) {
        drawSummary(content, y, report);
    }
    drawHoldingsHeader(content, y);
}

void finishPage(std::ostringstream& content, std::vector<std::string>& pages, int pageNumber)
{
    drawFooter(content, pageNumber);
    pages.push_back(content.str());
}

std::vector<std::string> buildPageStreams(const StockPickResearchReport& report)
{
    std::vector<std::string> pages;
    std::ostringstream content;
    double y = TopY;
    int pageNumber = 1;

    beginPage(content, report, true, y);

    if (report.holdings.empty()) {
        PdfCell cell;
        cell.text = "No holdings for this account";
        cell.width = TableWidth;
        cell.textColor = Muted;
        cell.hasFill = true;
        cell.fillColor = RowAlt;
        drawRow(content, y, { cell }, DataRowHeight, false);
        y -= DataRowHeight;
    } else {
        for (std::size_t index = 0; index < report.holdings.size(); ++index) {
            if (y - DataRowHeight < BottomContentY) {
                finishPage(content, pages, pageNumber);
                ++pageNumber;
                beginPage(content, report, false, y);
            }

            drawRow(content, y, holdingsRowCells(report.holdings[index], index % 2 == 1), DataRowHeight, false);
            y -= DataRowHeight;
        }
    }

    finishPage(content, pages, pageNumber);
    return pages;
}

std::string pdfStreamObject(const std::string& stream)
{
    std::ostringstream object;
    object << "<< /Length " << stream.size() << " >>\nstream\n"
           << stream
           << "endstream";
    return object.str();
}

std::string pageObject(int pagesObjectId, int contentObjectId)
{
    std::ostringstream object;
    object << "<< /Type /Page /Parent " << pagesObjectId << " 0 R "
           << "/MediaBox [0 0 " << formatPdfNumber(PageWidth) << ' ' << formatPdfNumber(PageHeight) << "] "
           << "/Resources << /Font << /F1 1 0 R /F2 2 0 R >> >> "
           << "/Contents " << contentObjectId << " 0 R >>";
    return object.str();
}

std::string pagesObject(const std::vector<int>& pageObjectIds)
{
    std::ostringstream object;
    object << "<< /Type /Pages /Kids [";
    for (const int pageObjectId : pageObjectIds) {
        object << pageObjectId << " 0 R ";
    }
    object << "] /Count " << pageObjectIds.size() << " >>";
    return object.str();
}

bool writePdf(const std::filesystem::path& outputPath, const std::vector<std::string>& pageStreams, std::string& error)
{
    if (pageStreams.empty()) {
        error = "The generated PDF report has no pages.";
        return false;
    }

    const int pagesObjectId = 3;
    const int catalogObjectId = 4;
    std::vector<std::string> objects(5);
    objects[1] = "<< /Type /Font /Subtype /Type1 /BaseFont /Helvetica /Encoding /WinAnsiEncoding >>";
    objects[2] = "<< /Type /Font /Subtype /Type1 /BaseFont /Helvetica-Bold /Encoding /WinAnsiEncoding >>";
    objects[4] = "<< /Type /Catalog /Pages 3 0 R >>";

    std::vector<int> pageObjectIds;
    int nextObjectId = 5;
    for (const std::string& pageStream : pageStreams) {
        const int contentObjectId = nextObjectId++;
        const int pageObjectId = nextObjectId++;
        if (static_cast<int>(objects.size()) <= pageObjectId) {
            objects.resize(static_cast<std::size_t>(pageObjectId) + 1);
        }
        objects[contentObjectId] = pdfStreamObject(pageStream);
        objects[pageObjectId] = pageObject(pagesObjectId, contentObjectId);
        pageObjectIds.push_back(pageObjectId);
    }
    objects[pagesObjectId] = pagesObject(pageObjectIds);

    std::ostringstream document;
    document << "%PDF-1.4\n";
    std::vector<std::size_t> offsets(objects.size(), 0);
    for (std::size_t objectId = 1; objectId < objects.size(); ++objectId) {
        offsets[objectId] = static_cast<std::size_t>(document.tellp());
        document << objectId << " 0 obj\n" << objects[objectId] << "\nendobj\n";
    }

    const std::size_t xrefOffset = static_cast<std::size_t>(document.tellp());
    document << "xref\n0 " << objects.size() << "\n"
             << "0000000000 65535 f \n";
    for (std::size_t objectId = 1; objectId < objects.size(); ++objectId) {
        document << std::setw(10) << std::setfill('0') << offsets[objectId] << " 00000 n \n";
    }
    document << "trailer\n<< /Size " << objects.size() << " /Root " << catalogObjectId << " 0 R >>\n"
             << "startxref\n" << xrefOffset << "\n%%EOF\n";

    const std::filesystem::path parent = outputPath.parent_path();
    if (!parent.empty()) {
        std::error_code existsError;
        if (!std::filesystem::exists(parent, existsError) || existsError) {
            error = "The selected export folder does not exist.";
            return false;
        }
    }

    std::ofstream file(outputPath, std::ios::binary | std::ios::trunc);
    if (!file) {
        error = "Could not create the PDF report file. Check the folder permissions and try again.";
        return false;
    }

    const std::string bytes = document.str();
    file.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
    if (!file) {
        error = "Could not finish writing the PDF report file.";
        return false;
    }

    return true;
}

} // namespace

namespace StockPickReportPdfExporter {

std::string defaultFileName(const StockPickResearchReport& report)
{
    return StockPickReportExportCommon::defaultFileName(report, ".pdf");
}

bool exportPdf(const StockPickResearchReport& report, const std::filesystem::path& outputPath, std::string& error)
{
    error.clear();
    if (outputPath.empty()) {
        error = "Choose a destination for the PDF report.";
        return false;
    }

    return writePdf(StockPickReportExportCommon::withExtension(outputPath, ".pdf"), buildPageStreams(report), error);
}

}
