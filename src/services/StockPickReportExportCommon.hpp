// SPDX-License-Identifier: MIT
#pragma once

#include "models/StockPickResearchReport.hpp"

#include <filesystem>
#include <string>

namespace StockPickReportExportCommon {
const char* periodLabel(StockPickReportPeriod period);
std::string sanitizedFileNamePart(const std::string& value);
std::string defaultFileName(const StockPickResearchReport& report, const std::string& extension);
std::filesystem::path withExtension(std::filesystem::path path, const std::string& extension);
}
