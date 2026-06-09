// SPDX-License-Identifier: MIT
#include "ui/DividendsView.hpp"

#include "app/AppState.hpp"
#include "repositories/DividendRepository.hpp"
#include "services/PortfolioCalculator.hpp"
#include "ui/UiTheme.hpp"
#include "ui/widgets/DatePicker.hpp"
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
constexpr const char* NewDividendEditorPopup = "Dividend Editor###dividend_edit_popup_new";

std::string dividendEditorPopupId(int dividendId)
{
    return "Dividend Editor###dividend_edit_popup_" + std::to_string(dividendId);
}

std::string dividendDeletePopupId(int dividendId)
{
    return "Delete Dividend###dividend_delete_popup_" + std::to_string(dividendId);
}

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

    UiTheme::metricCard("This Month", Money::format(summary.thisMonth).c_str(), "Recorded income", UiTheme::Amber, ImVec2(cardWidth, 74.0f));
    ImGui::SameLine();
    UiTheme::metricCard("This Year", Money::format(summary.thisYear).c_str(), "Recorded income", UiTheme::Amber, ImVec2(cardWidth, 74.0f));
    ImGui::SameLine();
    UiTheme::metricCard("Lifetime", Money::format(summary.lifetime).c_str(), "Recorded income", UiTheme::Amber, ImVec2(cardWidth, 74.0f));

    ImGui::Spacing();

    UiTheme::pushButtonStyle(UiTheme::NeonMagenta);
    if (ImGui::Button("Add Dividend")) {
        openCreate();
        openEditorPopup_ = true;
    }
    UiTheme::popButtonStyle();
    ImGui::SameLine();
    ImGui::SetNextItemWidth(300.0f);
    ImGui::InputTextWithHint("##DividendSearch", "Search dividends", &searchText_);

    ImGui::Spacing();

    UiTheme::pushPanelStyle();
    ImGui::BeginChild("DividendsTablePanel", ImVec2(0.0f, 0.0f), true);
    int visibleRows = 0;
    if (state.dividends.empty()) {
        UiTheme::emptyState("No dividends yet", "Add cash income as distributions arrive.");
    } else {
        UiTheme::pushTableStyle();
        if (ImGui::BeginTable("DividendsTable", 8, ImGuiTableFlags_BordersInnerH | ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable | ImGuiTableFlags_Reorderable | ImGuiTableFlags_Hideable | ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_ScrollX)) {
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
            DatePicker::drawTableDate(dividend.dateReceived);
            ImGui::TableNextColumn();
            ImGui::Text("%s", dividend.ticker.c_str());
            ImGui::TableNextColumn();
            ImGui::Text("%s", dividend.assetName.c_str());
            ImGui::TableNextColumn();
            ImGui::TextColored(UiTheme::MutedText, "%s", accountName);
            ImGui::TableNextColumn();
            UiTheme::textRightAligned(Money::format(dividend.amountReceived).c_str(), UiTheme::Gain);
            ImGui::TableNextColumn();
            UiTheme::badge(dividend.reinvested ? "Yes" : "No", dividend.reinvested ? UiTheme::Gain : UiTheme::TextMuted);
            ImGui::TableNextColumn();
            ImGui::TextColored(UiTheme::MutedText, "%s", dividend.updatedAt.c_str());
            ImGui::TableNextColumn();
            const std::string editButtonId = "Edit##edit_button_" + std::to_string(dividend.id);
            UiTheme::pushButtonStyle(UiTheme::ElectricCyan);
            if (ImGui::SmallButton(editButtonId.c_str())) {
                openEdit(dividend);
                openEditorPopup_ = true;
            }
            UiTheme::popButtonStyle();
            ImGui::SameLine();
            const std::string deleteButtonId = "Delete##delete_button_" + std::to_string(dividend.id);
            UiTheme::pushButtonStyle(UiTheme::Loss);
            if (ImGui::SmallButton(deleteButtonId.c_str())) {
                deleteId_ = dividend.id;
                deleteName_ = dividend.dateReceived + " " + dividend.ticker;
                deletePopupId_ = dividendDeletePopupId(dividend.id);
                openDeletePopup_ = true;
            }
            UiTheme::popButtonStyle();
        }

        ImGui::EndTable();
        if (visibleRows == 0) {
            ImGui::TextColored(UiTheme::MutedText, "No dividends match the current search.");
        }
        }
        UiTheme::popTableStyle();
    }
    ImGui::EndChild();
    UiTheme::popPanelStyle();

    if (openEditorPopup_) {
        ImGui::OpenPopup(editorPopupId_.empty() ? DividendEditorPopup : editorPopupId_.c_str());
        openEditorPopup_ = false;
    }
    if (openDeletePopup_) {
        ImGui::OpenPopup(deletePopupId_.empty() ? DeleteDividendPopup : deletePopupId_.c_str());
        openDeletePopup_ = false;
    }

    drawEditor(state, repository, reloadData);
    drawDeleteConfirmation(state, repository, reloadData);
}

void DividendsView::openCreate()
{
    draft_ = Dividend {};
    draft_.dateReceived = Date::todayIso8601();
    editing_ = false;
    editorPopupId_ = NewDividendEditorPopup;
    formError_.clear();
}

void DividendsView::openEdit(const Dividend& dividend)
{
    draft_ = dividend;
    editing_ = true;
    editorPopupId_ = dividendEditorPopupId(dividend.id);
    formError_.clear();
}

void DividendsView::drawEditor(AppState& state, DividendRepository& repository, const std::function<void()>& reloadData)
{
    const char* popupId = editorPopupId_.empty() ? DividendEditorPopup : editorPopupId_.c_str();
    if (!ImGui::BeginPopupModal(popupId, nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
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
    DatePicker::draw("Date received", draft_.dateReceived);
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
    const char* popupId = deletePopupId_.empty() ? DeleteDividendPopup : deletePopupId_.c_str();
    if (!ImGui::BeginPopupModal(popupId, nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
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
