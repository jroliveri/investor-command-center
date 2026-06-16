// SPDX-License-Identifier: MIT
#include "services/StockPickReportExportCommon.hpp"

#include "util/Date.hpp"

#include <algorithm>
#include <cctype>

namespace StockPickReportExportCommon {

const char* periodLabel(StockPickReportPeriod period)
{
    return period == StockPickReportPeriod::Monthly ? "Monthly" : "Weekly";
}

std::string sanitizedFileNamePart(const std::string& value)
{
    std::string sanitized;
    sanitized.reserve(value.size());
    bool previousWasSeparator = false;

    for (const unsigned char character : value) {
        const bool invalid = character < 32 || character == '<' || character == '>' || character == ':' || character == '"' || character == '/' ||
            character == '\\' || character == '|' || character == '?' || character == '*';
        const bool separator = invalid || std::isspace(character) != 0;
        if (separator) {
            if (!previousWasSeparator && !sanitized.empty()) {
                sanitized += '_';
                previousWasSeparator = true;
            }
            continue;
        }

        sanitized += static_cast<char>(character);
        previousWasSeparator = false;
    }

    while (!sanitized.empty() && (sanitized.back() == '_' || sanitized.back() == '.')) {
        sanitized.pop_back();
    }

    return sanitized.empty() ? "Account" : sanitized;
}

std::string defaultFileName(const StockPickResearchReport& report, const std::string& extension)
{
    return std::string("ICC_") + periodLabel(report.period) + "_Stock_Pick_Helper_" +
        sanitizedFileNamePart(report.accountName) + "_" + Date::todayIso8601() + extension;
}

std::filesystem::path withExtension(std::filesystem::path path, const std::string& extension)
{
    std::string normalizedExtension = extension.empty() || extension.front() == '.' ? extension : "." + extension;
    std::transform(normalizedExtension.begin(), normalizedExtension.end(), normalizedExtension.begin(), [](unsigned char character) {
        return static_cast<char>(std::tolower(character));
    });

    std::string currentExtension = path.extension().string();
    std::transform(currentExtension.begin(), currentExtension.end(), currentExtension.begin(), [](unsigned char character) {
        return static_cast<char>(std::tolower(character));
    });

    if (currentExtension != normalizedExtension) {
        path.replace_extension(normalizedExtension);
    }
    return path;
}

}
