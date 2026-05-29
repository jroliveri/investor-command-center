// SPDX-License-Identifier: MIT
#pragma once

#include "services/HoldingsCsvImport.hpp"

#include <functional>
#include <string>
#include <vector>

class AppState;
class CsvImportService;

class ImportCsvView {
public:
    void render(AppState& state, CsvImportService& importService, const std::function<void()>& reloadData);

private:
    void browseForCsv();
    void loadPreview(const AppState& state);
    void reloadWithHeaderRow(const AppState& state);
    void rebuildPreview(const AppState& state);
    void drawAccountSelector(AppState& state);
    void drawFileSection(const AppState& state);
    void drawHeaderDetection(const AppState& state);
    bool drawImportFieldCombo(std::size_t columnIndex);
    void drawMappingEditor(const AppState& state);
    void drawPreviewTable();
    void drawValidationSummary(AppState& state, CsvImportService& importService, const std::function<void()>& reloadData);
    void importValidRows(AppState& state, CsvImportService& importService, const std::function<void()>& reloadData);
    std::string sampleValueForColumn(std::size_t columnIndex) const;
    std::string mappingPreviewForField(HoldingsCsvImportField field) const;
    const char* mappingStatusForColumn(std::size_t columnIndex) const;
    void setColumnMapping(std::size_t columnIndex, HoldingsCsvImportField field);

    std::string csvPath_;
    CsvTable table_;
    HoldingsCsvColumnMapping mapping_;
    std::vector<HoldingsCsvPreviewRow> previewRows_;
    int selectedAccountId_ = 0;
    int headerRowInput_ = 0;
    std::string loadError_;
    std::string importMessage_;
};
