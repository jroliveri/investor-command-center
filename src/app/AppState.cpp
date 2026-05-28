// SPDX-License-Identifier: MIT
#include "app/AppState.hpp"

#include <utility>

void AppState::setStatus(std::string message, bool isError)
{
    statusMessage = std::move(message);
    statusIsError = isError;
}

void AppState::clearStatus()
{
    statusMessage.clear();
    statusIsError = false;
}

const char* sectionTitle(AppSection section)
{
    switch (section) {
    case AppSection::Dashboard:
        return "Dashboard";
    case AppSection::Accounts:
        return "Accounts";
    case AppSection::Holdings:
        return "Holdings";
    case AppSection::Transactions:
        return "Transactions";
    case AppSection::Dividends:
        return "Dividends";
    case AppSection::Goals:
        return "Goals";
    case AppSection::Watchlist:
        return "Watchlist";
    case AppSection::ImportCsv:
        return "Import CSV";
    case AppSection::Reports:
        return "Reports";
    case AppSection::Settings:
        return "Settings";
    }

    return "Dashboard";
}

const std::array<SectionInfo, 10>& allSections()
{
    static const std::array<SectionInfo, 10> sections {{
        { AppSection::Dashboard, "Dashboard" },
        { AppSection::Accounts, "Accounts" },
        { AppSection::Holdings, "Holdings" },
        { AppSection::Transactions, "Transactions" },
        { AppSection::Dividends, "Dividends" },
        { AppSection::Goals, "Goals" },
        { AppSection::Watchlist, "Watchlist" },
        { AppSection::ImportCsv, "Import CSV" },
        { AppSection::Reports, "Reports" },
        { AppSection::Settings, "Settings" },
    }};

    return sections;
}
