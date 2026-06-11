# SPDX-License-Identifier: MIT
cmake_minimum_required(VERSION 3.24)

project(InvestorCommandCenter VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(FetchContent)

FetchContent_Declare(
    imgui
    GIT_REPOSITORY https://github.com/ocornut/imgui.git
    GIT_TAG v1.90.9
    GIT_SHALLOW TRUE
)

FetchContent_MakeAvailable(imgui)

FetchContent_Declare(
    sqlite_amalgamation
    URL https://www.sqlite.org/2026/sqlite-amalgamation-3530100.zip
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)

FetchContent_MakeAvailable(sqlite_amalgamation)

file(GLOB SQLITE3_SOURCES CONFIGURE_DEPENDS
    "${sqlite_amalgamation_SOURCE_DIR}/sqlite3.c"
    "${sqlite_amalgamation_SOURCE_DIR}/*/sqlite3.c"
)

if(NOT SQLITE3_SOURCES)
    message(FATAL_ERROR "SQLite amalgamation did not contain sqlite3.c")
endif()

list(GET SQLITE3_SOURCES 0 SQLITE3_SOURCE)
get_filename_component(SQLITE3_INCLUDE_DIR "${SQLITE3_SOURCE}" DIRECTORY)

add_library(sqlite3 STATIC "${SQLITE3_SOURCE}")
target_include_directories(sqlite3 PUBLIC "${SQLITE3_INCLUDE_DIR}")
target_compile_definitions(sqlite3 PUBLIC
    SQLITE_THREADSAFE=1
    SQLITE_DEFAULT_FOREIGN_KEYS=1
    SQLITE_OMIT_LOAD_EXTENSION
)

add_library(imgui STATIC
    "${imgui_SOURCE_DIR}/imgui.cpp"
    "${imgui_SOURCE_DIR}/imgui_demo.cpp"
    "${imgui_SOURCE_DIR}/imgui_draw.cpp"
    "${imgui_SOURCE_DIR}/imgui_tables.cpp"
    "${imgui_SOURCE_DIR}/imgui_widgets.cpp"
    "${imgui_SOURCE_DIR}/backends/imgui_impl_dx11.cpp"
    "${imgui_SOURCE_DIR}/backends/imgui_impl_win32.cpp"
    "${imgui_SOURCE_DIR}/misc/cpp/imgui_stdlib.cpp"
)

target_include_directories(imgui PUBLIC
    "${imgui_SOURCE_DIR}"
    "${imgui_SOURCE_DIR}/backends"
    "${imgui_SOURCE_DIR}/misc/cpp"
)

target_compile_definitions(imgui PUBLIC
    IMGUI_ENABLE_WIN32_DEFAULT_IME_FUNCTIONS
    WIN32_LEAN_AND_MEAN
    NOMINMAX
)

if(MSVC)
    target_compile_options(imgui PRIVATE /utf-8)
    target_compile_options(sqlite3 PRIVATE /utf-8)
endif()

set(APP_SOURCES
    src/main.cpp
    src/app/App.hpp
    src/app/App.cpp
    src/app/AppState.hpp
    src/app/AppState.cpp
    src/app/SidebarModel.hpp
    src/app/SidebarModel.cpp
    src/db/Database.hpp
    src/db/Database.cpp
    src/db/Migrations.hpp
    src/db/Migrations.cpp
    src/models/Account.hpp
    src/models/CapitalGainAllocationRule.hpp
    src/models/DashboardChartSetting.hpp
    src/models/DashboardWidget.hpp
    src/models/Holding.hpp
    src/models/ImportBatch.hpp
    src/models/MarketPriceHistory.hpp
    src/models/MarketQuote.hpp
    src/models/PortfolioSnapshot.hpp
    src/models/SignalRules.hpp
    src/models/TechnicalIndicatorSettings.hpp
    src/models/TechnicalIndicatorSnapshot.hpp
    src/models/Transaction.hpp
    src/models/Dividend.hpp
    src/models/Goal.hpp
    src/models/Watchlist.hpp
    src/models/WatchlistItem.hpp
    src/models/WatchlistPriceRefresh.hpp
    src/net/HttpClient.hpp
    src/net/HttpClient.cpp
    src/repositories/AccountRepository.hpp
    src/repositories/AccountRepository.cpp
    src/repositories/AppSettingsRepository.hpp
    src/repositories/AppSettingsRepository.cpp
    src/repositories/CapitalGainAllocationRepository.hpp
    src/repositories/CapitalGainAllocationRepository.cpp
    src/repositories/DashboardChartSettingsRepository.hpp
    src/repositories/DashboardChartSettingsRepository.cpp
    src/repositories/DashboardLayoutRepository.hpp
    src/repositories/DashboardLayoutRepository.cpp
    src/repositories/HoldingRepository.hpp
    src/repositories/HoldingRepository.cpp
    src/repositories/ImportBatchRepository.hpp
    src/repositories/ImportBatchRepository.cpp
    src/repositories/MarketPriceHistoryRepository.hpp
    src/repositories/MarketPriceHistoryRepository.cpp
    src/repositories/MarketQuoteCacheRepository.hpp
    src/repositories/MarketQuoteCacheRepository.cpp
    src/repositories/TechnicalIndicatorCacheRepository.hpp
    src/repositories/TechnicalIndicatorCacheRepository.cpp
    src/repositories/PortfolioSnapshotRepository.hpp
    src/repositories/PortfolioSnapshotRepository.cpp
    src/repositories/TransactionRepository.hpp
    src/repositories/TransactionRepository.cpp
    src/repositories/DividendRepository.hpp
    src/repositories/DividendRepository.cpp
    src/repositories/GoalRepository.hpp
    src/repositories/GoalRepository.cpp
    src/repositories/WatchlistRepository.hpp
    src/repositories/WatchlistRepository.cpp
    src/services/PortfolioCalculator.hpp
    src/services/PortfolioCalculator.cpp
    src/services/CapitalGainAllocationService.hpp
    src/services/CapitalGainAllocationService.cpp
    src/services/CsvImportService.hpp
    src/services/CsvImportService.cpp
    src/services/PerformanceCalculator.hpp
    src/services/PerformanceCalculator.cpp
    src/services/DashboardService.hpp
    src/services/DashboardService.cpp
    src/services/DashboardLayoutService.hpp
    src/services/DashboardLayoutService.cpp
    src/services/TransactionService.hpp
    src/services/TransactionService.cpp
    src/services/HoldingsCsvImport.hpp
    src/services/HoldingsCsvImport.cpp
    src/services/MarketDataProvider.hpp
    src/services/MarketDataProvider.cpp
    src/services/MarketDataService.hpp
    src/services/MarketDataService.cpp
    src/services/TechnicalIndicatorSettingsService.hpp
    src/services/TechnicalIndicatorSettingsService.cpp
    src/services/TechnicalIndicatorService.hpp
    src/services/TechnicalIndicatorService.cpp
    src/services/WatchlistIndicatorDisplay.hpp
    src/services/WatchlistIndicatorDisplay.cpp
    src/services/WatchlistSignalService.hpp
    src/services/WatchlistSignalService.cpp
    src/services/YahooFinanceProvider.hpp
    src/services/YahooFinanceProvider.cpp
    src/ui/DashboardView.hpp
    src/ui/DashboardView.cpp
    src/ui/AccountsView.hpp
    src/ui/AccountsView.cpp
    src/ui/HoldingsView.hpp
    src/ui/HoldingsView.cpp
    src/ui/TransactionsView.hpp
    src/ui/TransactionsView.cpp
    src/ui/DividendsView.hpp
    src/ui/DividendsView.cpp
    src/ui/GoalsView.hpp
    src/ui/GoalsView.cpp
    src/ui/WatchlistView.hpp
    src/ui/WatchlistView.cpp
    src/ui/SettingsView.hpp
    src/ui/SettingsView.cpp
    src/ui/ImportCsvView.hpp
    src/ui/ImportCsvView.cpp
    src/ui/StockResearchView.hpp
    src/ui/StockResearchView.cpp
    src/ui/widgets/DatePicker.hpp
    src/ui/widgets/DatePicker.cpp
    src/ui/widgets/TerminalPanel.hpp
    src/ui/widgets/TerminalPanel.cpp
    src/ui/UiTheme.hpp
    src/ui/UiTheme.cpp
    src/util/Money.hpp
    src/util/Money.cpp
    src/util/Date.hpp
    src/util/Date.cpp
)

add_executable(InvestorCommandCenter WIN32 ${APP_SOURCES})

target_include_directories(InvestorCommandCenter PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
)

target_link_libraries(InvestorCommandCenter PRIVATE
    imgui
    sqlite3
    d3d11
    dxgi
    d3dcompiler
    dwmapi
    imm32
    comdlg32
    winhttp
)

target_compile_definitions(InvestorCommandCenter PRIVATE
    WIN32_LEAN_AND_MEAN
    NOMINMAX
    UNICODE
    _UNICODE
)

if(MSVC)
    target_compile_options(InvestorCommandCenter PRIVATE /W4 /permissive- /utf-8)
endif()

source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}/src" PREFIX "Source" FILES ${APP_SOURCES})

set_property(TARGET InvestorCommandCenter PROPERTY
    VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
)

enable_testing()

add_executable(InvestorCommandCenterTests
    tests/WatchlistIndicatorDisplayTests.cpp
    src/db/Database.hpp
    src/db/Database.cpp
    src/db/Migrations.hpp
    src/db/Migrations.cpp
    src/models/MarketQuote.hpp
    src/models/SignalRules.hpp
    src/models/WatchlistItem.hpp
    src/models/WatchlistPriceRefresh.hpp
    src/repositories/AppSettingsRepository.hpp
    src/repositories/AppSettingsRepository.cpp
    src/repositories/MarketQuoteCacheRepository.hpp
    src/repositories/MarketQuoteCacheRepository.cpp
    src/repositories/WatchlistRepository.hpp
    src/repositories/WatchlistRepository.cpp
    src/services/MarketDataProvider.hpp
    src/services/MarketDataProvider.cpp
    src/services/MarketDataService.hpp
    src/services/MarketDataService.cpp
    src/services/SignalRulesService.hpp
    src/services/SignalRulesService.cpp
    src/services/WatchlistSignalService.hpp
    src/services/WatchlistSignalService.cpp
    src/util/Date.hpp
    src/util/Date.cpp
    src/util/Money.hpp
    src/util/Money.cpp
)

target_include_directories(InvestorCommandCenterTests PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
)

target_link_libraries(InvestorCommandCenterTests PRIVATE
    sqlite3
)

if(MSVC)
    target_compile_options(InvestorCommandCenterTests PRIVATE /W4 /permissive- /utf-8)
endif()

add_test(NAME InvestorCommandCenterTests COMMAND InvestorCommandCenterTests)
