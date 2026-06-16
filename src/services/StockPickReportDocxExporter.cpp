// SPDX-License-Identifier: MIT
#include "services/StockPickReportDocxExporter.hpp"

#include "services/StockPickReportExportCommon.hpp"
#include "util/Date.hpp"
#include "util/Money.hpp"

#include <algorithm>
#include <array>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

namespace {

struct ZipSource {
    std::string name;
    std::string content;
};

struct ZipEntry {
    std::string name;
    std::vector<std::uint8_t> data;
    std::uint32_t crc = 0;
    std::uint32_t localHeaderOffset = 0;
};

std::string xmlEscape(std::string_view value)
{
    std::string result;
    result.reserve(value.size());
    for (const char character : value) {
        switch (character) {
        case '&':
            result += "&amp;";
            break;
        case '<':
            result += "&lt;";
            break;
        case '>':
            result += "&gt;";
            break;
        case '"':
            result += "&quot;";
            break;
        case '\'':
            result += "&apos;";
            break;
        default:
            result += character;
            break;
        }
    }
    return result;
}

std::string optionalMoney(double value, bool available)
{
    return available ? Money::format(value) : "N/A";
}

std::string optionalPercent(double value, bool available, bool includeSign = false)
{
    return available ? Money::formatPercent(value, includeSign) : "N/A";
}

std::string wRun(const std::string& text, bool bold = false, const char* color = nullptr, int halfPointSize = 20)
{
    std::ostringstream stream;
    stream << "<w:r>";
    if (bold || color != nullptr || halfPointSize > 0) {
        stream << "<w:rPr>";
        if (bold) {
            stream << "<w:b/>";
        }
        if (color != nullptr) {
            stream << "<w:color w:val=\"" << color << "\"/>";
        }
        if (halfPointSize > 0) {
            stream << "<w:sz w:val=\"" << halfPointSize << "\"/>";
            stream << "<w:szCs w:val=\"" << halfPointSize << "\"/>";
        }
        stream << "</w:rPr>";
    }
    stream << "<w:t xml:space=\"preserve\">" << xmlEscape(text) << "</w:t>";
    stream << "</w:r>";
    return stream.str();
}

std::string paragraph(const std::string& text, bool bold = false, int halfPointSize = 20, const char* color = nullptr)
{
    std::ostringstream stream;
    stream << "<w:p><w:pPr><w:spacing w:after=\"120\"/></w:pPr>" << wRun(text, bold, color, halfPointSize) << "</w:p>";
    return stream.str();
}

std::string heading(const std::string& text)
{
    std::ostringstream stream;
    stream << "<w:p><w:pPr><w:spacing w:before=\"160\" w:after=\"100\"/></w:pPr>"
           << wRun(text, true, "1F4E79", 24)
           << "</w:p>";
    return stream.str();
}

std::string tableCell(
    const std::string& text,
    int width,
    bool header = false,
    bool numeric = false,
    const char* textColor = nullptr,
    const char* fill = nullptr)
{
    std::ostringstream stream;
    stream << "<w:tc><w:tcPr><w:tcW w:w=\"" << width << "\" w:type=\"dxa\"/>";
    if (fill != nullptr) {
        stream << "<w:shd w:fill=\"" << fill << "\"/>";
    }
    stream << "<w:tcMar><w:top w:w=\"80\" w:type=\"dxa\"/><w:left w:w=\"80\" w:type=\"dxa\"/>"
           << "<w:bottom w:w=\"80\" w:type=\"dxa\"/><w:right w:w=\"80\" w:type=\"dxa\"/></w:tcMar></w:tcPr>";
    stream << "<w:p><w:pPr><w:spacing w:after=\"0\"/>";
    if (numeric) {
        stream << "<w:jc w:val=\"right\"/>";
    }
    stream << "</w:pPr>" << wRun(text, header, textColor, header ? 17 : 16) << "</w:p></w:tc>";
    return stream.str();
}

std::string tableRow(const std::vector<std::string>& cells, const std::vector<int>& widths, bool header = false)
{
    std::ostringstream stream;
    stream << "<w:tr>";
    for (std::size_t index = 0; index < cells.size(); ++index) {
        stream << tableCell(cells[index], widths[index], header, false, header ? "DDEBFF" : nullptr, header ? "172033" : nullptr);
    }
    stream << "</w:tr>";
    return stream.str();
}

std::string summaryRow(const std::string& label, const std::string& value)
{
    std::ostringstream stream;
    stream << "<w:tr>"
           << tableCell(label, 2800, true, false, "DDEBFF", "172033")
           << tableCell(value, 3600, false, true)
           << "</w:tr>";
    return stream.str();
}

const char* signalColor(const std::string& signal)
{
    if (signal == "Buy") {
        return "008A52";
    }
    if (signal == "Sell") {
        return "C8335B";
    }
    if (signal == "Not Tracked") {
        return "7A8794";
    }
    return "1672A5";
}

const char* signalFill(const std::string& signal)
{
    if (signal == "Buy") {
        return "E8F6EF";
    }
    if (signal == "Sell") {
        return "FCE8EF";
    }
    if (signal == "Not Tracked") {
        return "EEF1F4";
    }
    return "E8F3FB";
}

std::string holdingsRow(const StockPickResearchReportHoldingRow& row, const std::vector<int>& widths)
{
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

    std::ostringstream stream;
    stream << "<w:tr>";
    for (std::size_t index = 0; index < values.size(); ++index) {
        const bool numeric = index >= 2 && index <= 7 || index == 9;
        if (index == 8) {
            stream << tableCell(values[index], widths[index], false, false, signalColor(row.signalStatus), signalFill(row.signalStatus));
        } else {
            stream << tableCell(values[index], widths[index], false, numeric);
        }
    }
    stream << "</w:tr>";
    return stream.str();
}

std::string tableStart()
{
    return "<w:tbl><w:tblPr><w:tblW w:w=\"0\" w:type=\"auto\"/>"
           "<w:tblBorders>"
           "<w:top w:val=\"single\" w:sz=\"4\" w:space=\"0\" w:color=\"B8C7D9\"/>"
           "<w:left w:val=\"single\" w:sz=\"4\" w:space=\"0\" w:color=\"B8C7D9\"/>"
           "<w:bottom w:val=\"single\" w:sz=\"4\" w:space=\"0\" w:color=\"B8C7D9\"/>"
           "<w:right w:val=\"single\" w:sz=\"4\" w:space=\"0\" w:color=\"B8C7D9\"/>"
           "<w:insideH w:val=\"single\" w:sz=\"4\" w:space=\"0\" w:color=\"D8E2EF\"/>"
           "<w:insideV w:val=\"single\" w:sz=\"4\" w:space=\"0\" w:color=\"D8E2EF\"/>"
           "</w:tblBorders></w:tblPr>";
}

std::string tableEnd()
{
    return "</w:tbl>";
}

std::string buildDocumentXml(const StockPickResearchReport& report)
{
    const std::vector<int> holdingWidths {
        850, 2300, 950, 1150, 1200, 1250, 1250, 1250, 1150, 1050, 800,
    };

    std::ostringstream stream;
    stream << "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>"
           << "<w:document xmlns:w=\"http://schemas.openxmlformats.org/wordprocessingml/2006/main\">"
           << "<w:body>"
           << "<w:p><w:pPr><w:spacing w:after=\"180\"/></w:pPr>" << wRun(report.title, true, "111827", 34) << "</w:p>"
           << paragraph("Generated: " + report.generatedAt, false, 20, "4B5563")
           << paragraph("Selected account: " + report.accountName, false, 20, "4B5563")
           << heading("Account Summary")
           << tableStart()
           << summaryRow("Total account value", Money::format(report.totalAccountValue))
           << summaryRow("Holdings value", Money::format(report.holdingsValue))
           << summaryRow("Cash balance", Money::format(report.cashBalance))
           << summaryRow("Number of holdings", std::to_string(report.holdingCount))
           << tableEnd()
           << heading("Holdings / Allocation / Signals")
           << tableStart()
           << tableRow(
                  {
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
                  },
                  holdingWidths,
                  true);

    if (report.holdings.empty()) {
        stream << "<w:tr>" << tableCell("No holdings for this account", 14400, false, false, "7A8794") << "</w:tr>";
    } else {
        for (const StockPickResearchReportHoldingRow& row : report.holdings) {
            stream << holdingsRow(row, holdingWidths);
        }
    }

    stream << tableEnd()
           << paragraph("For personal research only. Not financial advice.", false, 18, "7A8794")
           << "<w:sectPr><w:pgSz w:w=\"15840\" w:h=\"12240\" w:orient=\"landscape\"/>"
           << "<w:pgMar w:top=\"720\" w:right=\"720\" w:bottom=\"720\" w:left=\"720\" w:header=\"360\" w:footer=\"360\" w:gutter=\"0\"/>"
           << "</w:sectPr>"
           << "</w:body></w:document>";
    return stream.str();
}

std::string buildContentTypesXml()
{
    return "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>"
           "<Types xmlns=\"http://schemas.openxmlformats.org/package/2006/content-types\">"
           "<Default Extension=\"rels\" ContentType=\"application/vnd.openxmlformats-package.relationships+xml\"/>"
           "<Default Extension=\"xml\" ContentType=\"application/xml\"/>"
           "<Override PartName=\"/docProps/app.xml\" ContentType=\"application/vnd.openxmlformats-officedocument.extended-properties+xml\"/>"
           "<Override PartName=\"/docProps/core.xml\" ContentType=\"application/vnd.openxmlformats-package.core-properties+xml\"/>"
           "<Override PartName=\"/word/document.xml\" ContentType=\"application/vnd.openxmlformats-officedocument.wordprocessingml.document.main+xml\"/>"
           "</Types>";
}

std::string buildRootRelationshipsXml()
{
    return "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>"
           "<Relationships xmlns=\"http://schemas.openxmlformats.org/package/2006/relationships\">"
           "<Relationship Id=\"rId1\" Type=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships/officeDocument\" Target=\"word/document.xml\"/>"
           "<Relationship Id=\"rId2\" Type=\"http://schemas.openxmlformats.org/package/2006/relationships/metadata/core-properties\" Target=\"docProps/core.xml\"/>"
           "<Relationship Id=\"rId3\" Type=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships/extended-properties\" Target=\"docProps/app.xml\"/>"
           "</Relationships>";
}

std::string generatedAtForCoreProperties(const StockPickResearchReport& report)
{
    if (report.generatedAt.size() == 19 && report.generatedAt[10] == ' ') {
        return report.generatedAt.substr(0, 10) + "T" + report.generatedAt.substr(11) + "Z";
    }
    return Date::todayIso8601() + "T00:00:00Z";
}

std::string buildCorePropertiesXml(const StockPickResearchReport& report)
{
    const std::string timestamp = generatedAtForCoreProperties(report);
    return "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>"
           "<cp:coreProperties xmlns:cp=\"http://schemas.openxmlformats.org/package/2006/metadata/core-properties\" "
           "xmlns:dc=\"http://purl.org/dc/elements/1.1/\" "
           "xmlns:dcterms=\"http://purl.org/dc/terms/\" "
           "xmlns:dcmitype=\"http://purl.org/dc/dcmitype/\" "
           "xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\">"
           "<dc:title>" +
        xmlEscape(report.title) +
        "</dc:title>"
        "<dc:creator>Investor Command Center</dc:creator>"
        "<cp:lastModifiedBy>Investor Command Center</cp:lastModifiedBy>"
        "<dcterms:created xsi:type=\"dcterms:W3CDTF\">" +
        timestamp +
        "</dcterms:created>"
        "<dcterms:modified xsi:type=\"dcterms:W3CDTF\">" +
        timestamp +
        "</dcterms:modified>"
        "</cp:coreProperties>";
}

std::string buildAppPropertiesXml()
{
    return "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>"
           "<Properties xmlns=\"http://schemas.openxmlformats.org/officeDocument/2006/extended-properties\" "
           "xmlns:vt=\"http://schemas.openxmlformats.org/officeDocument/2006/docPropsVTypes\">"
           "<Application>Investor Command Center</Application>"
           "</Properties>";
}

std::vector<ZipSource> buildDocxSources(const StockPickResearchReport& report)
{
    return {
        { "[Content_Types].xml", buildContentTypesXml() },
        { "_rels/.rels", buildRootRelationshipsXml() },
        { "docProps/core.xml", buildCorePropertiesXml(report) },
        { "docProps/app.xml", buildAppPropertiesXml() },
        { "word/document.xml", buildDocumentXml(report) },
    };
}

std::uint32_t crc32(const std::vector<std::uint8_t>& data)
{
    std::uint32_t crc = 0xFFFFFFFFu;
    for (const std::uint8_t byte : data) {
        crc ^= byte;
        for (int bit = 0; bit < 8; ++bit) {
            const std::uint32_t mask = static_cast<std::uint32_t>(-(static_cast<int>(crc & 1u)));
            crc = (crc >> 1u) ^ (0xEDB88320u & mask);
        }
    }
    return ~crc;
}

void appendUInt16(std::vector<std::uint8_t>& output, std::uint16_t value)
{
    output.push_back(static_cast<std::uint8_t>(value & 0xFFu));
    output.push_back(static_cast<std::uint8_t>((value >> 8u) & 0xFFu));
}

void appendUInt32(std::vector<std::uint8_t>& output, std::uint32_t value)
{
    appendUInt16(output, static_cast<std::uint16_t>(value & 0xFFFFu));
    appendUInt16(output, static_cast<std::uint16_t>((value >> 16u) & 0xFFFFu));
}

void appendBytes(std::vector<std::uint8_t>& output, std::string_view value)
{
    output.insert(output.end(), value.begin(), value.end());
}

void appendBytes(std::vector<std::uint8_t>& output, const std::vector<std::uint8_t>& value)
{
    output.insert(output.end(), value.begin(), value.end());
}

bool appendLocalEntry(std::vector<std::uint8_t>& output, ZipEntry& entry, std::string& error)
{
    if (output.size() > UINT32_MAX || entry.data.size() > UINT32_MAX || entry.name.size() > UINT16_MAX) {
        error = "The generated Word report is too large to export.";
        return false;
    }

    entry.localHeaderOffset = static_cast<std::uint32_t>(output.size());
    entry.crc = crc32(entry.data);

    appendUInt32(output, 0x04034B50u);
    appendUInt16(output, 20);
    appendUInt16(output, 0);
    appendUInt16(output, 0);
    appendUInt16(output, 0);
    appendUInt16(output, 33);
    appendUInt32(output, entry.crc);
    appendUInt32(output, static_cast<std::uint32_t>(entry.data.size()));
    appendUInt32(output, static_cast<std::uint32_t>(entry.data.size()));
    appendUInt16(output, static_cast<std::uint16_t>(entry.name.size()));
    appendUInt16(output, 0);
    appendBytes(output, entry.name);
    appendBytes(output, entry.data);
    return true;
}

bool appendCentralDirectory(std::vector<std::uint8_t>& output, const std::vector<ZipEntry>& entries, std::uint32_t centralDirectoryOffset, std::string& error)
{
    for (const ZipEntry& entry : entries) {
        if (entry.name.size() > UINT16_MAX || entry.data.size() > UINT32_MAX) {
            error = "The generated Word report is too large to export.";
            return false;
        }

        appendUInt32(output, 0x02014B50u);
        appendUInt16(output, 20);
        appendUInt16(output, 20);
        appendUInt16(output, 0);
        appendUInt16(output, 0);
        appendUInt16(output, 0);
        appendUInt16(output, 33);
        appendUInt32(output, entry.crc);
        appendUInt32(output, static_cast<std::uint32_t>(entry.data.size()));
        appendUInt32(output, static_cast<std::uint32_t>(entry.data.size()));
        appendUInt16(output, static_cast<std::uint16_t>(entry.name.size()));
        appendUInt16(output, 0);
        appendUInt16(output, 0);
        appendUInt16(output, 0);
        appendUInt16(output, 0);
        appendUInt32(output, 0);
        appendUInt32(output, entry.localHeaderOffset);
        appendBytes(output, entry.name);
    }

    if (output.size() > UINT32_MAX) {
        error = "The generated Word report is too large to export.";
        return false;
    }

    const std::uint32_t centralDirectorySize = static_cast<std::uint32_t>(output.size() - centralDirectoryOffset);
    appendUInt32(output, 0x06054B50u);
    appendUInt16(output, 0);
    appendUInt16(output, 0);
    appendUInt16(output, static_cast<std::uint16_t>(entries.size()));
    appendUInt16(output, static_cast<std::uint16_t>(entries.size()));
    appendUInt32(output, centralDirectorySize);
    appendUInt32(output, centralDirectoryOffset);
    appendUInt16(output, 0);
    return true;
}

bool writeZip(const std::filesystem::path& outputPath, const std::vector<ZipSource>& sources, std::string& error)
{
    if (sources.size() > UINT16_MAX) {
        error = "The generated Word report has too many parts to export.";
        return false;
    }

    std::vector<ZipEntry> entries;
    entries.reserve(sources.size());
    for (const ZipSource& source : sources) {
        entries.push_back(ZipEntry {
            source.name,
            std::vector<std::uint8_t>(source.content.begin(), source.content.end()),
        });
    }

    std::vector<std::uint8_t> package;
    for (ZipEntry& entry : entries) {
        if (!appendLocalEntry(package, entry, error)) {
            return false;
        }
    }

    if (package.size() > UINT32_MAX) {
        error = "The generated Word report is too large to export.";
        return false;
    }

    const std::uint32_t centralDirectoryOffset = static_cast<std::uint32_t>(package.size());
    if (!appendCentralDirectory(package, entries, centralDirectoryOffset, error)) {
        return false;
    }

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
        error = "Could not create the Word report file. Check the folder permissions and try again.";
        return false;
    }

    file.write(reinterpret_cast<const char*>(package.data()), static_cast<std::streamsize>(package.size()));
    if (!file) {
        error = "Could not finish writing the Word report file.";
        return false;
    }

    return true;
}

} // namespace

namespace StockPickReportDocxExporter {

std::string defaultFileName(const StockPickResearchReport& report)
{
    return StockPickReportExportCommon::defaultFileName(report, ".docx");
}

bool exportDocx(const StockPickResearchReport& report, const std::filesystem::path& outputPath, std::string& error)
{
    error.clear();
    if (outputPath.empty()) {
        error = "Choose a destination for the Word report.";
        return false;
    }

    return writeZip(StockPickReportExportCommon::withExtension(outputPath, ".docx"), buildDocxSources(report), error);
}

}
