// SPDX-License-Identifier: MIT
#include "ui/TransactionsView.hpp"

#include "app/AppState.hpp"
#include "repositories/TransactionRepository.hpp"
#include "ui/UiTheme.hpp"
#include "util/Date.hpp"
#include "util/Money.hpp"

#include <imgui.h>
#include <imgui_stdlib.h>

#include <algorithm>
#include <array>
#include <cctype>
#include <string>

namespace {

constexpr const char* TransactionEditorPopup = "Transaction Editor";
constexpr const char* DeleteTransactionPopup = "Delete Transaction";

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

bool matchesFilter(const Transaction& transaction, const std::string& query, const char* accountName)
{
    if (query.empty()) {
        return true;
    }

    return containsCaseInsensitive(transaction.ticker, query) ||
        containsCaseInsensitive(transaction.assetName, query) ||
        containsCaseInsensitive(transaction.transactionType, query) ||
        containsCaseInsensitive(transaction.transactionDate, query) ||
        containsCaseInsensitive(accountName, query) ||
        containsCaseInsensitive(transaction.notes, query);
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

void TransactionsView::render(AppState& state, TransactionRepository& repository, const std::function<void()>& reloadData)
{
    UiTheme::sectionHeading("Transactions", "Manual buys, sells, deposits, withdrawals, and other account activity.");

    if (ImGui::Button("Add Transaction")) {
        openCreate();
        ImGui::OpenPopup(TransactionEditorPopup);
    }
    ImGui::SameLine();
    ImGui::SetNextItemWidth(300.0f);
    ImGui::InputTextWithHint("##TransactionSearch", "Search transactions", &searchText_);

    ImGui::Spacing();

    int visibleRows = 0;
    if (state.transactions.empty()) {
        UiTheme::emptyState("No transactions yet", "Add manual activity to build a useful account feed.");
    } else if (ImGui::BeginTable("TransactionsTable", 9, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable | ImGuiTableFlags_Reorderable | ImGuiTableFlags_Hideable | ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_ScrollX)) {
        ImGui::TableSetupColumn("Date", ImGuiTableColumnFlags_WidthFixed, 100.0f);
        ImGui::TableSetupColumn("Type", ImGuiTableColumnFlags_WidthFixed, 105.0f);
        ImGui::TableSetupColumn("Ticker", ImGuiTableColumnFlags_WidthFixed, 76.0f);
        ImGui::TableSetupColumn("Asset", ImGuiTableColumnFlags_WidthStretch, 1.2f);
        ImGui::TableSetupColumn("Account", ImGuiTableColumnFlags_WidthStretch, 1.0f);
        ImGui::TableSetupColumn("Qty", ImGuiTableColumnFlags_WidthFixed, 88.0f);
        ImGui::TableSetupColumn("Price", ImGuiTableColumnFlags_WidthFixed, 92.0f);
        ImGui::TableSetupColumn("Total", ImGuiTableColumnFlags_WidthFixed, 110.0f);
        ImGui::TableSetupColumn("Actions", ImGuiTableColumnFlags_WidthFixed, 128.0f);
        ImGui::TableHeadersRow();

        for (const Transaction& transaction : state.transactions) {
            const char* accountName = accountNameFor(state, transaction.accountId);
            if (!matchesFilter(transaction, searchText_, accountName)) {
                continue;
            }

            ++visibleRows;
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::Text("%s", transaction.transactionDate.c_str());
            ImGui::TableNextColumn();
            ImGui::TextColored(UiTheme::Amber, "%s", transaction.transactionType.c_str());
            ImGui::TableNextColumn();
            ImGui::Text("%s", transaction.ticker.c_str());
            ImGui::TableNextColumn();
            ImGui::Text("%s", transaction.assetName.c_str());
            ImGui::TableNextColumn();
            ImGui::TextColored(UiTheme::MutedText, "%s", accountName);
            ImGui::TableNextColumn();
            ImGui::Text("%s", Money::formatQuantity(transaction.quantity).c_str());
            ImGui::TableNextColumn();
            ImGui::Text("%s", Money::format(transaction.price).c_str());
            ImGui::TableNextColumn();
            ImGui::Text("%s", Money::format(transaction.totalAmount).c_str());
            ImGui::TableNextColumn();
            ImGui::PushID(transaction.id);
            if (ImGui::SmallButton("Edit")) {
                openEdit(transaction);
                ImGui::OpenPopup(TransactionEditorPopup);
            }
            ImGui::SameLine();
            if (ImGui::SmallButton("Delete")) {
                deleteId_ = transaction.id;
                deleteName_ = transaction.transactionDate + " " + transaction.transactionType + " " + transaction.ticker;
                ImGui::OpenPopup(DeleteTransactionPopup);
            }
            ImGui::PopID();
        }

        ImGui::EndTable();
        if (visibleRows == 0) {
            ImGui::TextColored(UiTheme::MutedText, "No transactions match the current search.");
        }
    }

    drawEditor(state, repository, reloadData);
    drawDeleteConfirmation(state, repository, reloadData);
}

void TransactionsView::openCreate()
{
    draft_ = Transaction {};
    draft_.transactionType = "Buy";
    draft_.transactionDate = Date::todayIso8601();
    editing_ = false;
    formError_.clear();
}

void TransactionsView::openEdit(const Transaction& transaction)
{
    draft_ = transaction;
    editing_ = true;
    formError_.clear();
}

void TransactionsView::drawEditor(AppState& state, TransactionRepository& repository, const std::function<void()>& reloadData)
{
    if (!ImGui::BeginPopupModal(TransactionEditorPopup, nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        return;
    }

    ImGui::Text("%s", editing_ ? "Edit transaction" : "Add transaction");
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

    drawStringCombo("Type", draft_.transactionType, std::array {
        "Buy",
        "Sell",
        "Deposit",
        "Withdrawal",
        "Fee",
        "Transfer",
        "Other",
    });
    ImGui::InputText("Date", &draft_.transactionDate);
    ImGui::TextColored(UiTheme::MutedText, "Use YYYY-MM-DD.");
    ImGui::InputText("Ticker", &draft_.ticker, ImGuiInputTextFlags_CharsUppercase | ImGuiInputTextFlags_CharsNoBlank);
    ImGui::InputText("Asset name", &draft_.assetName);
    ImGui::InputDouble("Quantity", &draft_.quantity, 0.0, 0.0, "%.4f");
    ImGui::InputDouble("Price", &draft_.price, 0.0, 0.0, "%.4f");
    ImGui::InputDouble("Total amount", &draft_.totalAmount, 0.0, 0.0, "%.2f");
    if (ImGui::SmallButton("Use quantity x price")) {
        draft_.totalAmount = draft_.quantity * draft_.price;
    }
    ImGui::InputTextMultiline("Notes", &draft_.notes, ImVec2(440.0f, 86.0f));

    UiTheme::formError(formError_);

    if (ImGui::Button(editing_ ? "Save Changes" : "Create Transaction", ImVec2(156.0f, 0.0f))) {
        std::string error;
        const bool saved = editing_ ? repository.update(draft_, error) : repository.create(draft_, error);
        if (saved) {
            reloadData();
            state.setStatus(editing_ ? "Transaction updated." : "Transaction created.");
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

void TransactionsView::drawDeleteConfirmation(AppState& state, TransactionRepository& repository, const std::function<void()>& reloadData)
{
    if (!ImGui::BeginPopupModal(DeleteTransactionPopup, nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        return;
    }

    ImGui::Text("Delete transaction?");
    ImGui::TextColored(UiTheme::MutedText, "%s", deleteName_.c_str());

    if (ImGui::Button("Delete", ImVec2(100.0f, 0.0f))) {
        std::string error;
        if (repository.remove(deleteId_, error)) {
            reloadData();
            state.setStatus("Transaction deleted.");
            ImGui::CloseCurrentPopup();
        } else {
            state.setStatus("Could not delete transaction: " + error, true);
            ImGui::CloseCurrentPopup();
        }
    }

    ImGui::SameLine();
    if (ImGui::Button("Cancel", ImVec2(100.0f, 0.0f))) {
        ImGui::CloseCurrentPopup();
    }

    ImGui::EndPopup();
}

const char* TransactionsView::accountNameFor(const AppState& state, int accountId) const
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
