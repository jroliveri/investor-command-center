// SPDX-License-Identifier: MIT
#include "ui/TransactionsView.hpp"

#include "app/AppState.hpp"
#include "services/CapitalGainAllocationService.hpp"
#include "services/TransactionService.hpp"
#include "ui/UiTheme.hpp"
#include "ui/widgets/DatePicker.hpp"
#include "util/Date.hpp"
#include "util/Money.hpp"

#include <imgui.h>
#include <imgui_stdlib.h>

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <sstream>
#include <string>

namespace {

constexpr const char* TransactionEditorPopup = "Transaction Editor";
constexpr const char* DeleteTransactionPopup = "Delete Transaction";
constexpr const char* AllocationPopup = "Capital Gains Allocation";

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

bool isSell(const Transaction& transaction)
{
    return transaction.transactionType == "Sell";
}

bool isCashOutflow(const Transaction& transaction)
{
    return transaction.transactionType == "Buy" ||
        transaction.transactionType == "Withdrawal" ||
        transaction.transactionType == "Fee";
}

bool isCashInflow(const Transaction& transaction)
{
    return transaction.transactionType == "Sell" ||
        transaction.transactionType == "Dividend" ||
        transaction.transactionType == "Contribution" ||
        transaction.transactionType == "Interest";
}

ImVec4 transactionBadgeColor(const std::string& transactionType)
{
    if (transactionType == "Buy" || transactionType == "Dividend" || transactionType == "Contribution" || transactionType == "Interest") {
        return UiTheme::Gain;
    }
    if (transactionType == "Sell" || transactionType == "Withdrawal" || transactionType == "Fee") {
        return UiTheme::Loss;
    }
    if (transactionType == "Transfer") {
        return UiTheme::ElectricCyan;
    }
    return UiTheme::TextMuted;
}

ImVec4 transactionAmountColor(const Transaction& transaction)
{
    if (transaction.totalAmount < 0.0 || isCashOutflow(transaction)) {
        return UiTheme::Loss;
    }
    if (transaction.totalAmount > 0.0 && isCashInflow(transaction)) {
        return UiTheme::Gain;
    }
    return transaction.totalAmount == 0.0 ? UiTheme::TextMuted : UiTheme::TextSecondary;
}

std::string allocationSummaryText(const Transaction& transaction, const CapitalGainAllocationResult& result)
{
    std::ostringstream stream;
    stream << "Ticker: " << (transaction.ticker.empty() ? "(none)" : transaction.ticker) << '\n';
    stream << "Realized Gain: " << Money::format(result.realizedGain) << '\n';
    for (const CapitalGainAllocationLine& line : result.lines) {
        stream << line.category << ' ' << Money::formatPercent(line.percentage) << ": " << Money::format(line.amount) << '\n';
    }
    if (result.unallocatedAmount > 0.005) {
        stream << "Unallocated: " << Money::format(result.unallocatedAmount) << '\n';
    }
    return stream.str();
}

}

void TransactionsView::render(AppState& state, TransactionService& service, const std::function<void()>& reloadData)
{
    UiTheme::sectionHeading("Transactions", "Manual buys, sells, deposits, withdrawals, and other account activity.");

    UiTheme::pushButtonStyle(UiTheme::NeonMagenta);
    if (ImGui::Button("Add Transaction")) {
        openCreate();
        ImGui::OpenPopup(TransactionEditorPopup);
    }
    UiTheme::popButtonStyle();
    ImGui::SameLine();
    ImGui::SetNextItemWidth(300.0f);
    ImGui::InputTextWithHint("##TransactionSearch", "Search transactions", &searchText_);

    ImGui::Spacing();

    UiTheme::pushPanelStyle();
    ImGui::BeginChild("TransactionsLedgerPanel", ImVec2(0.0f, 0.0f), true);
    int visibleRows = 0;
    if (state.transactions.empty()) {
        UiTheme::emptyState("No transactions yet", "Add manual activity to build a useful account feed.");
    } else {
        UiTheme::pushTableStyle();
        if (ImGui::BeginTable("TransactionsTable", 12, ImGuiTableFlags_BordersInnerH | ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable | ImGuiTableFlags_Reorderable | ImGuiTableFlags_Hideable | ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_ScrollX)) {
        ImGui::TableSetupColumn("Date", ImGuiTableColumnFlags_WidthFixed, 100.0f);
        ImGui::TableSetupColumn("Type", ImGuiTableColumnFlags_WidthFixed, 105.0f);
        ImGui::TableSetupColumn("Ticker", ImGuiTableColumnFlags_WidthFixed, 76.0f);
        ImGui::TableSetupColumn("Asset", ImGuiTableColumnFlags_WidthStretch, 1.2f);
        ImGui::TableSetupColumn("Account", ImGuiTableColumnFlags_WidthStretch, 1.0f);
        ImGui::TableSetupColumn("Qty", ImGuiTableColumnFlags_WidthFixed, 88.0f);
        ImGui::TableSetupColumn("Price", ImGuiTableColumnFlags_WidthFixed, 92.0f);
        ImGui::TableSetupColumn("Fees", ImGuiTableColumnFlags_WidthFixed, 92.0f);
        ImGui::TableSetupColumn("Total", ImGuiTableColumnFlags_WidthFixed, 110.0f);
        ImGui::TableSetupColumn("Realized $", ImGuiTableColumnFlags_WidthFixed, 116.0f);
        ImGui::TableSetupColumn("Realized %", ImGuiTableColumnFlags_WidthFixed, 106.0f);
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
            DatePicker::drawTableDate(transaction.transactionDate);
            ImGui::TableNextColumn();
            UiTheme::badge(transaction.transactionType.c_str(), transactionBadgeColor(transaction.transactionType));
            ImGui::TableNextColumn();
            ImGui::Text("%s", transaction.ticker.c_str());
            ImGui::TableNextColumn();
            ImGui::Text("%s", transaction.assetName.c_str());
            ImGui::TableNextColumn();
            ImGui::TextColored(UiTheme::MutedText, "%s", accountName);
            ImGui::TableNextColumn();
            UiTheme::textRightAligned(Money::formatQuantity(transaction.quantity).c_str());
            ImGui::TableNextColumn();
            UiTheme::textRightAligned(Money::format(transaction.price).c_str());
            ImGui::TableNextColumn();
            UiTheme::textRightAligned(Money::format(transaction.fees).c_str());
            ImGui::TableNextColumn();
            UiTheme::textRightAligned(Money::format(transaction.totalAmount).c_str(), transactionAmountColor(transaction));
            ImGui::TableNextColumn();
            UiTheme::textRightAligned(isSell(transaction) ? Money::format(transaction.realizedGainDollar).c_str() : "-", UiTheme::moneyColor(transaction.realizedGainDollar));
            ImGui::TableNextColumn();
            UiTheme::textRightAligned(isSell(transaction) ? Money::formatPercent(transaction.realizedGainPercent, true).c_str() : "-", UiTheme::moneyColor(transaction.realizedGainDollar));
            ImGui::TableNextColumn();
            ImGui::PushID(transaction.id);
            UiTheme::pushButtonStyle(UiTheme::ElectricCyan);
            if (ImGui::SmallButton("Menu")) {
                ImGui::OpenPopup("TransactionRowMenu");
            }
            UiTheme::popButtonStyle();
            if (ImGui::BeginPopup("TransactionRowMenu")) {
                const bool sellTransaction = isSell(transaction);
                if (!sellTransaction) {
                    ImGui::BeginDisabled();
                }
                if (ImGui::MenuItem("View Gain Allocation")) {
                    allocationTransaction_ = transaction;
                    hasAllocationTransaction_ = true;
                    openAllocationPopup_ = true;
                }
                if (!sellTransaction) {
                    ImGui::EndDisabled();
                    ImGui::TextColored(UiTheme::MutedText, "Allocation is available for Sell transactions.");
                }

                ImGui::Separator();
                if (ImGui::MenuItem("Edit Transaction")) {
                    openEdit(transaction);
                    openEditorPopup_ = true;
                }
                if (ImGui::MenuItem("Delete Transaction")) {
                    deleteId_ = transaction.id;
                    deleteName_ = transaction.transactionDate + " " + transaction.transactionType + " " + transaction.ticker;
                    openDeletePopup_ = true;
                }
                ImGui::EndPopup();
            }
            ImGui::PopID();
        }

        ImGui::EndTable();
        if (visibleRows == 0) {
            ImGui::TextColored(UiTheme::MutedText, "No transactions match the current search.");
        }
        }
        UiTheme::popTableStyle();
    }
    ImGui::EndChild();
    UiTheme::popPanelStyle();

    if (openEditorPopup_) {
        ImGui::OpenPopup(TransactionEditorPopup);
        openEditorPopup_ = false;
    }
    if (openDeletePopup_) {
        ImGui::OpenPopup(DeleteTransactionPopup);
        openDeletePopup_ = false;
    }
    if (openAllocationPopup_) {
        ImGui::OpenPopup(AllocationPopup);
        openAllocationPopup_ = false;
    }

    drawAllocationPopup(state);
    drawEditor(state, service, reloadData);
    drawDeleteConfirmation(state, service, reloadData);
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

void TransactionsView::drawEditor(AppState& state, TransactionService& service, const std::function<void()>& reloadData)
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
        "Dividend",
        "Contribution",
        "Withdrawal",
        "Transfer",
        "Fee",
        "Interest",
        "Other",
    });
    DatePicker::draw("Date", draft_.transactionDate);
    ImGui::InputText("Ticker", &draft_.ticker, ImGuiInputTextFlags_CharsUppercase | ImGuiInputTextFlags_CharsNoBlank);
    ImGui::InputText("Asset name", &draft_.assetName);
    ImGui::InputDouble("Quantity", &draft_.quantity, 0.0, 0.0, "%.4f");
    ImGui::InputDouble("Price", &draft_.price, 0.0, 0.0, "%.4f");
    ImGui::InputDouble("Fees", &draft_.fees, 0.0, 0.0, "%.2f");
    ImGui::InputDouble("Total amount", &draft_.totalAmount, 0.0, 0.0, "%.2f");
    if (ImGui::SmallButton("Use quantity x price plus fees")) {
        draft_.totalAmount = draft_.quantity * draft_.price + draft_.fees;
    }

    if (draft_.transactionType == "Sell") {
        ImGui::Separator();
        ImGui::Text("Sell Details");
        ImGui::InputDouble("Sold quantity", &draft_.soldQuantity, 0.0, 0.0, "%.4f");
        ImGui::InputDouble("Sale price", &draft_.salePrice, 0.0, 0.0, "%.4f");
        ImGui::Checkbox("Adjustment entry", &draft_.isAdjustment);
        ImGui::TextColored(UiTheme::MutedText, "Adjustment entries can be saved even when the holding share count is not enough.");

        const Transaction preview = service.preview(draft_, state.holdings);
        ImGui::Text("Sale proceeds: %s", Money::format(preview.saleProceeds).c_str());
        ImGui::Text("Cost basis used: %s", Money::format(preview.costBasisUsed).c_str());
        ImGui::TextColored(UiTheme::moneyColor(preview.realizedGainDollar), "Realized gain/loss: %s (%s)",
            Money::format(preview.realizedGainDollar).c_str(),
            Money::formatPercent(preview.realizedGainPercent, true).c_str());
    }
    ImGui::InputTextMultiline("Notes", &draft_.notes, ImVec2(440.0f, 86.0f));

    UiTheme::formError(formError_);

    if (ImGui::Button(editing_ ? "Save Changes" : "Create Transaction", ImVec2(156.0f, 0.0f))) {
        std::string error;
        const bool saved = editing_ ? service.update(draft_, state.holdings, error) : service.create(draft_, state.holdings, error);
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

void TransactionsView::drawDeleteConfirmation(AppState& state, TransactionService& service, const std::function<void()>& reloadData)
{
    if (!ImGui::BeginPopupModal(DeleteTransactionPopup, nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        return;
    }

    ImGui::Text("Delete transaction?");
    ImGui::TextColored(UiTheme::MutedText, "%s", deleteName_.c_str());

    if (ImGui::Button("Delete", ImVec2(100.0f, 0.0f))) {
        std::string error;
        if (service.remove(deleteId_, error)) {
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

void TransactionsView::drawAllocationPopup(const AppState& state)
{
    if (!ImGui::BeginPopupModal(AllocationPopup, nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        return;
    }

    ImGui::Text("Capital Gains Allocation");
    ImGui::TextColored(UiTheme::MutedText, "Based on your saved allocation rules. This allocation helper does not move money or provide financial or tax advice.");
    ImGui::Separator();

    if (!hasAllocationTransaction_) {
        ImGui::TextColored(UiTheme::Loss, "No transaction selected.");
    } else if (!isSell(allocationTransaction_)) {
        ImGui::TextColored(UiTheme::MutedText, "Allocation is available for Sell transactions with realized gains.");
    } else {
        const CapitalGainAllocationResult result = CapitalGainAllocationService::calculate(allocationTransaction_, state.capitalGainAllocationRules, state.holdings);

        ImGui::TextColored(UiTheme::MutedText, "Transaction date");
        ImGui::SameLine(180.0f);
        ImGui::Text("%s", allocationTransaction_.transactionDate.c_str());
        ImGui::TextColored(UiTheme::MutedText, "Ticker");
        ImGui::SameLine(180.0f);
        ImGui::Text("%s", allocationTransaction_.ticker.empty() ? "-" : allocationTransaction_.ticker.c_str());
        ImGui::TextColored(UiTheme::MutedText, "Asset name");
        ImGui::SameLine(180.0f);
        ImGui::Text("%s", allocationTransaction_.assetName.empty() ? "-" : allocationTransaction_.assetName.c_str());
        ImGui::TextColored(UiTheme::MutedText, "Sale proceeds");
        ImGui::SameLine(180.0f);
        ImGui::Text("%s", Money::format(result.saleProceeds).c_str());
        ImGui::TextColored(UiTheme::MutedText, "Cost basis used");
        ImGui::SameLine(180.0f);
        ImGui::Text("%s", Money::format(result.costBasisUsed).c_str());
        ImGui::TextColored(UiTheme::MutedText, "Realized gain/loss");
        ImGui::SameLine(180.0f);
        ImGui::TextColored(UiTheme::moneyColor(result.realizedGain), "%s", Money::format(result.realizedGain).c_str());
        ImGui::TextColored(UiTheme::MutedText, "Allocation rules total");
        ImGui::SameLine(180.0f);
        ImGui::Text("%s", Money::formatPercent(result.totalPercentage).c_str());

        ImGui::Spacing();

        if (result.realizedGain <= 0.0) {
            ImGui::TextColored(UiTheme::Amber, "No positive realized gain to allocate.");
        } else if (result.lines.empty()) {
            ImGui::TextColored(UiTheme::Amber, "No active allocation rules are available. Add categories in Settings.");
        } else {
            UiTheme::pushTableStyle();
            if (ImGui::BeginTable("CapitalGainAllocationPopupTable", 3,
                    ImGuiTableFlags_BordersInnerH | ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingFixedFit,
                    ImVec2(520.0f, 0.0f))) {
                ImGui::TableSetupColumn("Category", ImGuiTableColumnFlags_WidthStretch, 1.0f);
                ImGui::TableSetupColumn("Percentage", ImGuiTableColumnFlags_WidthFixed, 120.0f);
                ImGui::TableSetupColumn("Amount", ImGuiTableColumnFlags_WidthFixed, 140.0f);
                ImGui::TableHeadersRow();

                for (const CapitalGainAllocationLine& line : result.lines) {
                    ImGui::TableNextRow();
                    ImGui::TableNextColumn();
                    ImGui::Text("%s", line.category.c_str());
                    ImGui::TableNextColumn();
                    UiTheme::textRightAligned(Money::formatPercent(line.percentage).c_str());
                    ImGui::TableNextColumn();
                    UiTheme::textRightAligned(Money::format(line.amount).c_str(), UiTheme::Gain);
                }

                ImGui::EndTable();
            }
            UiTheme::popTableStyle();

            if (result.totalPercentage < 99.99) {
                ImGui::Spacing();
                ImGui::TextColored(UiTheme::Amber, "Unallocated amount: %s", Money::format(result.unallocatedAmount).c_str());
            } else if (result.totalPercentage > 100.01) {
                ImGui::Spacing();
                ImGui::TextColored(UiTheme::Loss, "Overallocated by %s. Review the percentages in Settings.",
                    Money::format(result.allocatedAmount - result.realizedGain).c_str());
            }

            ImGui::Spacing();
            if (ImGui::Button("Copy Summary to Clipboard", ImVec2(190.0f, 0.0f))) {
                ImGui::SetClipboardText(allocationSummaryText(allocationTransaction_, result).c_str());
            }
            ImGui::SameLine();
        }
    }

    if (ImGui::Button("Close", ImVec2(100.0f, 0.0f))) {
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
