// SPDX-License-Identifier: MIT
#pragma once

#include "models/StockPickResearchReport.hpp"

#include <filesystem>
#include <string>

namespace StockPickReportPdfExporter {
std::string defaultFileName(const StockPickResearchReport& report);
bool exportPdf(const StockPickResearchReport& report, const std::filesystem::path& outputPath, std::string& error);
}
