// SPDX-License-Identifier: MIT
#pragma once

#include "models/StockPickResearchReport.hpp"

#include <filesystem>
#include <string>

namespace StockPickReportDocxExporter {
std::string defaultFileName(const StockPickResearchReport& report);
bool exportDocx(const StockPickResearchReport& report, const std::filesystem::path& outputPath, std::string& error);
}
