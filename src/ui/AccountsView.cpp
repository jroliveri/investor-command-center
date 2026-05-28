// SPDX-License-Identifier: MIT
#include "ui/AccountsView.hpp"

#include "app/AppState.hpp"
#include "repositories/AccountRepository.hpp"
#include "services/PortfolioCalculator.hpp"
#include "ui/UiTheme.hpp"
#include "util/Money.hpp"

#include <imgui.h>
#include <imgui_stdlib.h>

#include <algorithm>
#include <array>
#include <cctype>

namespace {

constexpr const char* AccountEditorPopup = "Account Editor";
constexpr const char* DeleteAccountPopup = "Delete Account";

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

bool matchesFilter(const Account& account, const std::string& query)
{
    if (query.empty()) {
        return true;
    }

    return containsCaseInsensitive(account.accountName, query) ||
        containsCaseInsensitive(account.accountType, query) ||
        containsCaseInsensitive(account.institutionName, query) ||
        containsCaseInsensitive(account.status, query) ||
        containsCaseInsensitive(account.notes, query);
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

void AccountsView::render(AppState& state, AccountRepository& repository, const std::function<void()>& reloadData)
{
    UiTheme::sectionHeading("Accounts", "Cash is entered manually; total balance is calculated from holdings plus cash.");

    if (ImGui::Button("Add Account")) {
        openCreate();
        ImGui::OpenPopup(AccountEditorPopup);
    }
    ImGui::SameLine();
    ImGui::SetNextItemWidth(300.0f);
    ImGui::InputTextWithHint("##AccountSearch", "Search accounts", &searchText_);

    ImGui::Spacing();

    int visibleRows = 0;
    if (state.accounts.empty()) {
        UiTheme::emptyState("No accounts yet", "Add one account to start tracking balances and holdings.");
    } else if (ImGui::BeginTable("AccountsTable", 9, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable | ImGuiTableFlags_Reorderable | ImGuiTableFlags_Hideable | ImGuiTableFlags_SizingStretchProp)) {
        ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthStretch, 1.3f);
        ImGui::TableSetupColumn("Type");
        ImGui::TableSetupColumn("Institution");
        ImGui::TableSetupColumn("Calculated Balance", ImGuiTableColumnFlags_WidthFixed, 138.0f);
        ImGui::TableSetupColumn("Holdings", ImGuiTableColumnFlags_WidthFixed, 110.0f);
        ImGui::TableSetupColumn("Cash", ImGuiTableColumnFlags_WidthFixed, 110.0f);
        ImGui::TableSetupColumn("Status", ImGuiTableColumnFlags_WidthFixed, 84.0f);
        ImGui::TableSetupColumn("Updated", ImGuiTableColumnFlags_WidthFixed, 140.0f);
        ImGui::TableSetupColumn("Actions", ImGuiTableColumnFlags_WidthFixed, 128.0f);
        ImGui::TableHeadersRow();

        for (const Account& account : state.accounts) {
            if (!matchesFilter(account, searchText_)) {
                continue;
            }

            ++visibleRows;
            const AccountMetrics metrics = PortfolioCalculator::calculateAccount(account, state.holdings);
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::Text("%s", account.accountName.c_str());
            ImGui::TableNextColumn();
            ImGui::TextColored(UiTheme::MutedText, "%s", account.accountType.c_str());
            ImGui::TableNextColumn();
            ImGui::Text("%s", account.institutionName.c_str());
            ImGui::TableNextColumn();
            ImGui::Text("%s", Money::format(metrics.calculatedBalance).c_str());
            ImGui::TableNextColumn();
            ImGui::TextColored(UiTheme::MutedText, "%s", Money::format(metrics.holdingsMarketValue).c_str());
            ImGui::TableNextColumn();
            ImGui::Text("%s", Money::format(account.cashBalance).c_str());
            ImGui::TableNextColumn();
            ImGui::TextColored(account.status == "Active" ? UiTheme::Gain : UiTheme::MutedText, "%s", account.status.c_str());
            ImGui::TableNextColumn();
            ImGui::TextColored(UiTheme::MutedText, "%s", account.updatedAt.c_str());
            ImGui::TableNextColumn();
            ImGui::PushID(account.id);
            if (ImGui::SmallButton("Edit")) {
                openEdit(account);
                ImGui::OpenPopup(AccountEditorPopup);
            }
            ImGui::SameLine();
            if (ImGui::SmallButton("Delete")) {
                deleteId_ = account.id;
                deleteName_ = account.accountName;
                ImGui::OpenPopup(DeleteAccountPopup);
            }
            ImGui::PopID();
        }

        ImGui::EndTable();
        if (visibleRows == 0) {
            ImGui::TextColored(UiTheme::MutedText, "No accounts match the current search.");
        }
    }

    drawEditor(state, repository, reloadData);
    drawDeleteConfirmation(state, repository, reloadData);
}

void AccountsView::openCreate()
{
    draft_ = Account {};
    draft_.accountType = "Brokerage";
    draft_.status = "Active";
    editing_ = false;
    formError_.clear();
}

void AccountsView::openEdit(const Account& account)
{
    draft_ = account;
    editing_ = true;
    formError_.clear();
}

void AccountsView::drawEditor(AppState& state, AccountRepository& repository, const std::function<void()>& reloadData)
{
    if (!ImGui::BeginPopupModal(AccountEditorPopup, nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        return;
    }

    ImGui::Text("%s", editing_ ? "Edit account" : "Add account");
    ImGui::Separator();

    ImGui::InputText("Account name", &draft_.accountName);
    drawStringCombo("Account type", draft_.accountType, std::array {
        "Brokerage",
        "Retirement",
        "Savings",
        "Checking",
        "Crypto",
        "Other",
    });
    ImGui::InputText("Institution", &draft_.institutionName);
    ImGui::InputDouble("Cash balance", &draft_.cashBalance, 0.0, 0.0, "%.2f");
    ImGui::TextColored(UiTheme::MutedText, "Account balance is calculated as holdings market value plus cash balance.");
    drawStringCombo("Status", draft_.status, std::array {
        "Active",
        "Closed",
        "Watch",
    });
    ImGui::InputTextMultiline("Notes", &draft_.notes, ImVec2(420.0f, 86.0f));

    UiTheme::formError(formError_);

    if (ImGui::Button(editing_ ? "Save Changes" : "Create Account", ImVec2(140.0f, 0.0f))) {
        std::string error;
        const bool saved = editing_ ? repository.update(draft_, error) : repository.create(draft_, error);
        if (saved) {
            reloadData();
            state.setStatus(editing_ ? "Account updated." : "Account created.");
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

void AccountsView::drawDeleteConfirmation(AppState& state, AccountRepository& repository, const std::function<void()>& reloadData)
{
    if (!ImGui::BeginPopupModal(DeleteAccountPopup, nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        return;
    }

    ImGui::Text("Delete account?");
    ImGui::TextColored(UiTheme::MutedText, "%s", deleteName_.c_str());
    ImGui::TextColored(UiTheme::Amber, "Holdings must be deleted first.");

    if (ImGui::Button("Delete", ImVec2(100.0f, 0.0f))) {
        std::string error;
        if (repository.remove(deleteId_, error)) {
            reloadData();
            state.setStatus("Account deleted.");
            ImGui::CloseCurrentPopup();
        } else {
            state.setStatus("Could not delete account: " + error, true);
            ImGui::CloseCurrentPopup();
        }
    }

    ImGui::SameLine();
    if (ImGui::Button("Cancel", ImVec2(100.0f, 0.0f))) {
        ImGui::CloseCurrentPopup();
    }

    ImGui::EndPopup();
}
