// SPDX-License-Identifier: MIT
#include "ui/HoldingsView.hpp"

#include "app/AppState.hpp"
#include "repositories/HoldingRepository.hpp"
#include "services/PortfolioCalculator.hpp"
#include "ui/UiTheme.hpp"
#include "util/Money.hpp"

#include <imgui.h>
#include <imgui_stdlib.h>

#include <algorithm>
#include <array>
#include <cctype>

namespace {

constexpr const char* HoldingEditorPopup = "Holding Editor";
constexpr const char* DeleteHoldingPopup = "Delete Holding";

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

bool matchesFilter(const Holding& holding, const std::string& query, const char* accountName)
{
    if (query.empty()) {
        return true;
    }

    return containsCaseInsensitive(holding.ticker, query) ||
        containsCaseInsensitive(holding.assetName, query) ||
        containsCaseInsensitive(holding.assetType, query) ||
        containsCaseInsensitive(accountName, query) ||
        containsCaseInsensitive(holding.notes, query);
}

template <std::size_t Size>
void drawStringCombo(const char* label, std::string& value, const std::array<const char*, Size>& options)
{
    const char* preview = value.empty() ? options[0] : value.c_str();
    if (ImGui::BeginCombo(label, preview)) {
        for (const char* option : options) {
            const bool selected = value == option;
            if (ImGui::Selectable(option, selected)) {
                value = option;
            }
            if (selected) {
                ImGui::SetItemDefaultFocus();
            }
        }
        ImGui::EndCombo();
    }
}

}

void HoldingsView::render(AppState& state, HoldingRepository& repository, const std::function<void()>& reloadData)
{
    UiTheme::sectionHeading("Holdings", "Manual lots with local gain/loss calculations.");

    if (state.accounts.empty()) {
        ImGui::BeginDisabled();
        ImGui::Button("Add Holding");
        ImGui::EndDisabled();
        ImGui::SameLine();
        ImGui::TextColored(UiTheme::MutedText, "Create an account first.");
    } else if (ImGui::Button("Add Holding")) {
        openCreate(state);
        ImGui::OpenPopup(HoldingEditorPopup);
    }
    ImGui::SameLine();
    ImGui::SetNextItemWidth(300.0f);
    ImGui::InputTextWithHint("##HoldingSearch", "Search holdings", &searchText_);

    ImGui::Spacing();

    int visibleRows = 0;
    if (state.holdings.empty()) {
        UiTheme::emptyState("No holdings yet", "Add a holding after creating at least one account.");
    } else if (ImGui::BeginTable("HoldingsTable", 11, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable | ImGuiTableFlags_Reorderable | ImGuiTableFlags_Hideable | ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_ScrollX)) {
        ImGui::TableSetupColumn("Ticker", ImGuiTableColumnFlags_WidthFixed, 76.0f);
        ImGui::TableSetupColumn("Asset", ImGuiTableColumnFlags_WidthStretch, 1.2f);
        ImGui::TableSetupColumn("Account", ImGuiTableColumnFlags_WidthStretch, 1.0f);
        ImGui::TableSetupColumn("Type", ImGuiTableColumnFlags_WidthFixed, 96.0f);
        ImGui::TableSetupColumn("Shares", ImGuiTableColumnFlags_WidthFixed, 92.0f);
        ImGui::TableSetupColumn("Avg Cost", ImGuiTableColumnFlags_WidthFixed, 96.0f);
        ImGui::TableSetupColumn("Price", ImGuiTableColumnFlags_WidthFixed, 96.0f);
        ImGui::TableSetupColumn("Cost Basis", ImGuiTableColumnFlags_WidthFixed, 110.0f);
        ImGui::TableSetupColumn("Market Value", ImGuiTableColumnFlags_WidthFixed, 116.0f);
        ImGui::TableSetupColumn("Gain/Loss", ImGuiTableColumnFlags_WidthFixed, 126.0f);
        ImGui::TableSetupColumn("Actions", ImGuiTableColumnFlags_WidthFixed, 128.0f);
        ImGui::TableHeadersRow();

        for (const Holding& holding : state.holdings) {
            const char* accountName = accountNameFor(state, holding.accountId);
            if (!matchesFilter(holding, searchText_, accountName)) {
                continue;
            }

            const HoldingMetrics metrics = PortfolioCalculator::calculateHolding(holding);
            ++visibleRows;

            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::Text("%s", holding.ticker.c_str());
            ImGui::TableNextColumn();
            ImGui::Text("%s", holding.assetName.c_str());
            ImGui::TableNextColumn();
            ImGui::TextColored(UiTheme::MutedText, "%s", accountName);
            ImGui::TableNextColumn();
            ImGui::TextColored(UiTheme::MutedText, "%s", holding.assetType.c_str());
            ImGui::TableNextColumn();
            ImGui::Text("%s", Money::formatQuantity(holding.shares).c_str());
            ImGui::TableNextColumn();
            ImGui::Text("%s", Money::format(holding.averageCost).c_str());
            ImGui::TableNextColumn();
            ImGui::Text("%s", Money::format(holding.currentPrice).c_str());
            ImGui::TableNextColumn();
            ImGui::Text("%s", Money::format(metrics.costBasis).c_str());
            ImGui::TableNextColumn();
            ImGui::Text("%s", Money::format(metrics.marketValue).c_str());
            ImGui::TableNextColumn();
            ImGui::TextColored(UiTheme::moneyColor(metrics.gainLossDollar), "%s / %s",
                Money::format(metrics.gainLossDollar).c_str(),
                Money::formatPercent(metrics.gainLossPercent).c_str());
            ImGui::TableNextColumn();
            ImGui::PushID(holding.id);
            if (ImGui::SmallButton("Edit")) {
                openEdit(holding);
                ImGui::OpenPopup(HoldingEditorPopup);
            }
            ImGui::SameLine();
            if (ImGui::SmallButton("Delete")) {
                deleteId_ = holding.id;
                deleteName_ = holding.ticker + " - " + holding.assetName;
                ImGui::OpenPopup(DeleteHoldingPopup);
            }
            ImGui::PopID();
        }

        ImGui::EndTable();
        if (visibleRows == 0) {
            ImGui::TextColored(UiTheme::MutedText, "No holdings match the current search.");
        }
    }

    drawEditor(state, repository, reloadData);
    drawDeleteConfirmation(state, repository, reloadData);
}

void HoldingsView::openCreate(const AppState& state)
{
    draft_ = Holding {};
    draft_.assetType = "Stock";
    draft_.accountId = state.accounts.empty() ? 0 : state.accounts.front().id;
    editing_ = false;
    formError_.clear();
}

void HoldingsView::openEdit(const Holding& holding)
{
    draft_ = holding;
    editing_ = true;
    formError_.clear();
}

void HoldingsView::drawEditor(AppState& state, HoldingRepository& repository, const std::function<void()>& reloadData)
{
    if (!ImGui::BeginPopupModal(HoldingEditorPopup, nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        return;
    }

    ImGui::Text("%s", editing_ ? "Edit holding" : "Add holding");
    ImGui::Separator();

    const char* accountPreview = accountNameFor(state, draft_.accountId);
    if (ImGui::BeginCombo("Account", accountPreview)) {
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
    drawStringCombo("Asset type", draft_.assetType, std::array {
        "Stock",
        "ETF",
        "Mutual Fund",
        "Bond",
        "Cash Equivalent",
        "Crypto",
        "Other",
    });
    ImGui::InputDouble("Shares", &draft_.shares, 0.0, 0.0, "%.4f");
    ImGui::InputDouble("Average cost", &draft_.averageCost, 0.0, 0.0, "%.4f");
    ImGui::InputDouble("Current price", &draft_.currentPrice, 0.0, 0.0, "%.4f");
    ImGui::InputTextMultiline("Notes", &draft_.notes, ImVec2(440.0f, 86.0f));

    const HoldingMetrics metrics = PortfolioCalculator::calculateHolding(draft_);
    ImGui::Separator();
    ImGui::TextColored(UiTheme::MutedText, "Cost basis: %s", Money::format(metrics.costBasis).c_str());
    ImGui::TextColored(UiTheme::moneyColor(metrics.gainLossDollar), "Gain/Loss: %s (%s)",
        Money::format(metrics.gainLossDollar).c_str(),
        Money::formatPercent(metrics.gainLossPercent).c_str());

    UiTheme::formError(formError_);

    if (ImGui::Button(editing_ ? "Save Changes" : "Create Holding", ImVec2(140.0f, 0.0f))) {
        std::string error;
        const bool saved = editing_ ? repository.update(draft_, error) : repository.create(draft_, error);
        if (saved) {
            reloadData();
            state.setStatus(editing_ ? "Holding updated." : "Holding created.");
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

void HoldingsView::drawDeleteConfirmation(AppState& state, HoldingRepository& repository, const std::function<void()>& reloadData)
{
    if (!ImGui::BeginPopupModal(DeleteHoldingPopup, nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        return;
    }

    ImGui::Text("Delete holding?");
    ImGui::TextColored(UiTheme::MutedText, "%s", deleteName_.c_str());

    if (ImGui::Button("Delete", ImVec2(100.0f, 0.0f))) {
        std::string error;
        if (repository.remove(deleteId_, error)) {
            reloadData();
            state.setStatus("Holding deleted.");
            ImGui::CloseCurrentPopup();
        } else {
            state.setStatus("Could not delete holding: " + error, true);
            ImGui::CloseCurrentPopup();
        }
    }

    ImGui::SameLine();
    if (ImGui::Button("Cancel", ImVec2(100.0f, 0.0f))) {
        ImGui::CloseCurrentPopup();
    }

    ImGui::EndPopup();
}

const char* HoldingsView::accountNameFor(const AppState& state, int accountId) const
{
    for (const Account& account : state.accounts) {
        if (account.id == accountId) {
            return account.accountName.c_str();
        }
    }

    return "Unknown account";
}
