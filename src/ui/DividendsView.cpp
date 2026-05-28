// SPDX-License-Identifier: MIT
#include "ui/DividendsView.hpp"

#include "app/AppState.hpp"
#include "repositories/DividendRepository.hpp"
#include "services/PortfolioCalculator.hpp"
#include "ui/UiTheme.hpp"
#include "util/Date.hpp"
#include "util/Money.hpp"

#include <imgui.h>
#include <imgui_stdlib.h>

#include <algorithm>
#include <cctype>
#include <string>

namespace {

constexpr const char* DividendEditorPopup = "Dividend Editor";
constexpr const char* DeleteDividendPopup = "Delete Dividend";

std::string lowerCopy(std::string value)
{
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char character) {
        return static_cast<char>(std::tolower(character));
    });
    return value;
}

bool containsCaseInsensitive(const std::string& value, const std::string& query)
{
    return lowerCopy(value).find(lowerCopy(query)) != std::string::npos;
}

bool matchesFilter(const Dividend& dividend, const std::string& query, const char* accountName)
{
    if (query.empty()) {
        return true;
    }

    return containsCaseInsensitive(dividend.ticker, query) ||
        containsCaseInsensitive(dividend.assetName, query) ||
        containsCaseInsensitive(dividend.dateReceived, query) ||
        containsCaseInsensitive(accountName, query) ||
        containsCaseInsensitive(dividend.notes, query);
}

}

void DividendsView::render(AppState& state, DividendRepository& repository, const std::function<void()>& reloadData)
{
    UiTheme::sectionHeading("Dividends", "Track cash income and reinvested distributions manually.");

    const DividendSummary summary = PortfolioCalculator::calculateDividends(
        state.dividends,
        Date::currentMonthPrefix(),
        Date::currentYearPrefix());

    const float availableWidth = ImGui::GetContentRegionAvail().x;
    const float gap = ImGui::GetStyle().ItemSpacing.x;
    const float cardWidth = (availableWidth - (gap * 2.0f)) / 3.0f;

    ImGui::BeginChild("DividendMonthCard", ImVec2(cardWidth, 74.0f), true);
    ImGui::TextColored(UiTheme::MutedText, "This Month");
    ImGui::TextColored(UiTheme::Amber, "%s", Money::format(summary.thisMonth).c_str());
    ImGui::EndChild();
    ImGui::SameLine();
    ImGui::BeginChild("DividendYearCard", ImVec2(cardWidth, 74.0f), true);
    ImGui::TextColored(UiTheme::MutedText, "This Year");
    ImGui::TextColored(UiTheme::Amber, "%s", Money::format(summary.thisYear).c_str());
    ImGui::EndChild();
    ImGui::SameLine();
    ImGui::BeginChild("DividendLifetimeCard", ImVec2(cardWidth, 74.0f), true);
    ImGui::TextColored(UiTheme::MutedText, "Lifetime");
    ImGui::TextColored(UiTheme::Amber, "%s", Money::format(summary.lifetime).c_str());
    ImGui::EndChild();

    ImGui::Spacing();

    if (ImGui::Button("Add Dividend")) {
        openCreate();
        ImGui::OpenPopup(DividendEditorPopup);
    }
    ImGui::SameLine();
    ImGui::SetNextItemWidth(300.0f);
    ImGui::InputTextWithHint("##DividendSearch", "Search dividends", &searchText_);

    ImGui::Spacing();

    int visibleRows = 0;
    if (state.dividends.empty()) {
        UiTheme::emptyState("No dividends yet", "Add cash income as distributions arrive.");
    } else if (ImGui::BeginTable("DividendsTable", 8, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable | ImGuiTableFlags_Reorderable | ImGuiTableFlags_Hideable | ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_ScrollX)) {
        ImGui::TableSetupColumn("Date", ImGuiTableColumnFlags_WidthFixed, 100.0f);
        ImGui::TableSetupColumn("Ticker", ImGuiTableColumnFlags_WidthFixed, 76.0f);
        ImGui::TableSetupColumn("Asset", ImGuiTableColumnFlags_WidthStretch, 1.2f);
        ImGui::TableSetupColumn("Account", ImGuiTableColumnFlags_WidthStretch, 1.0f);
        ImGui::TableSetupColumn("Amount", ImGuiTableColumnFlags_WidthFixed, 110.0f);
        ImGui::TableSetupColumn("Reinvested", ImGuiTableColumnFlags_WidthFixed, 92.0f);
        ImGui::TableSetupColumn("Updated", ImGuiTableColumnFlags_WidthFixed, 140.0f);
        ImGui::TableSetupColumn("Actions", ImGuiTableColumnFlags_WidthFixed, 128.0f);
        ImGui::TableHeadersRow();

        for (const Dividend& dividend : state.dividends) {
            const char* accountName = accountNameFor(state, dividend.accountId);
            if (!matchesFilter(dividend, searchText_, accountName)) {
                continue;
            }

            ++visibleRows;
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::Text("%s", dividend.dateReceived.c_str());
            ImGui::TableNextColumn();
            ImGui::Text("%s", dividend.ticker.c_str());
            ImGui::TableNextColumn();
            ImGui::Text("%s", dividend.assetName.c_str());
            ImGui::TableNextColumn();
            ImGui::TextColored(UiTheme::MutedText, "%s", accountName);
            ImGui::TableNextColumn();
            ImGui::TextColored(UiTheme::Amber, "%s", Money::format(dividend.amountReceived).c_str());
            ImGui::TableNextColumn();
            ImGui::TextColored(dividend.reinvested ? UiTheme::Gain : UiTheme::MutedText, "%s", dividend.reinvested ? "Yes" : "No");
            ImGui::TableNextColumn();
            ImGui::TextColored(UiTheme::MutedText, "%s", dividend.updatedAt.c_str());
            ImGui::TableNextColumn();
            ImGui::PushID(dividend.id);
            if (ImGui::SmallButton("Edit")) {
                openEdit(dividend);
                ImGui::OpenPopup(DividendEditorPopup);
            }
            ImGui::SameLine();
            if (ImGui::SmallButton("Delete")) {
                deleteId_ = dividend.id;
                deleteName_ = dividend.dateReceived + " " + dividend.ticker;
                ImGui::OpenPopup(DeleteDividendPopup);
            }
            ImGui::PopID();
        }

        ImGui::EndTable();
        if (visibleRows == 0) {
            ImGui::TextColored(UiTheme::MutedText, "No dividends match the current search.");
        }
    }

    drawEditor(state, repository, reloadData);
    drawDeleteConfirmation(state, repository, reloadData);
}

void DividendsView::openCreate()
{
    draft_ = Dividend {};
    draft_.dateReceived = Date::todayIso8601();
    editing_ = false;
    formError_.clear();
}

void DividendsView::openEdit(const Dividend& dividend)
{
    draft_ = dividend;
    editing_ = true;
    formError_.clear();
}

void DividendsView::drawEditor(AppState& state, DividendRepository& repository, const std::function<void()>& reloadData)
{
    if (!ImGui::BeginPopupModal(DividendEditorPopup, nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        return;
    }

    ImGui::Text("%s", editing_ ? "Edit dividend" : "Add dividend");
    ImGui::Separator();

    const char* accountPreview = accountNameFor(state, draft_.accountId);
    if (ImGui::BeginCombo("Account", accountPreview)) {
        if (ImGui::Selectable("Unassigned", draft_.accountId == 0)) {
            draft_.accountId = 0;
        }
        for (const Account& account : state.accounts) {
            const bool selected = draft_.accountId == account.id;
            if (ImGui::Selectable(account.accountName.c_str(), selected)) {
                draft_.accountId = account.id;
            }
            if (selected) {
                ImGui::SetItemDefaultFocus();
            }
        }
        ImGui::EndCombo();
    }

    ImGui::InputText("Ticker", &draft_.ticker, ImGuiInputTextFlags_CharsUppercase | ImGuiInputTextFlags_CharsNoBlank);
    ImGui::InputText("Asset name", &draft_.assetName);
    ImGui::InputText("Date received", &draft_.dateReceived);
    ImGui::TextColored(UiTheme::MutedText, "Use YYYY-MM-DD.");
    ImGui::InputDouble("Amount received", &draft_.amountReceived, 0.0, 0.0, "%.2f");
    ImGui::Checkbox("Reinvested", &draft_.reinvested);
    ImGui::InputTextMultiline("Notes", &draft_.notes, ImVec2(440.0f, 86.0f));

    UiTheme::formError(formError_);

    if (ImGui::Button(editing_ ? "Save Changes" : "Create Dividend", ImVec2(140.0f, 0.0f))) {
        std::string error;
        const bool saved = editing_ ? repository.update(draft_, error) : repository.create(draft_, error);
        if (saved) {
            reloadData();
            state.setStatus(editing_ ? "Dividend updated." : "Dividend created.");
            ImGui::CloseCurrentPopup();
        } else {
            formError_ = error;
        }
    }

    ImGui::SameLine();
    if (ImGui::Button("Cancel", ImVec2(100.0f, 0.0f))) {
        ImGui::CloseCurrentPopup();
    }

    ImGui::EndPopup();
}

void DividendsView::drawDeleteConfirmation(AppState& state, DividendRepository& repository, const std::function<void()>& reloadData)
{
    if (!ImGui::BeginPopupModal(DeleteDividendPopup, nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        return;
    }

    ImGui::Text("Delete dividend?");
    ImGui::TextColored(UiTheme::MutedText, "%s", deleteName_.c_str());

    if (ImGui::Button("Delete", ImVec2(100.0f, 0.0f))) {
        std::string error;
        if (repository.remove(deleteId_, error)) {
            reloadData();
            state.setStatus("Dividend deleted.");
            ImGui::CloseCurrentPopup();
        } else {
            state.setStatus("Could not delete dividend: " + error, true);
            ImGui::CloseCurrentPopup();
        }
    }

    ImGui::SameLine();
    if (ImGui::Button("Cancel", ImVec2(100.0f, 0.0f))) {
        ImGui::CloseCurrentPopup();
    }

    ImGui::EndPopup();
}

const char* DividendsView::accountNameFor(const AppState& state, int accountId) const
{
    if (accountId <= 0) {
        return "Unassigned";
    }

    for (const Account& account : state.accounts) {
        if (account.id == accountId) {
            return account.accountName.c_str();
        }
    }

    return "Unknown account";
}
