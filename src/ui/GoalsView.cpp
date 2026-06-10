// SPDX-License-Identifier: MIT
#include "ui/GoalsView.hpp"

#include "app/AppState.hpp"
#include "repositories/GoalRepository.hpp"
#include "services/PortfolioCalculator.hpp"
#include "ui/UiTheme.hpp"
#include "ui/widgets/DatePicker.hpp"
#include "util/Money.hpp"

#include <imgui.h>
#include <imgui_stdlib.h>

#include <algorithm>
#include <array>
#include <cctype>
#include <string>

namespace {

constexpr const char* GoalEditorPopup = "Goal Editor";
constexpr const char* DeleteGoalPopup = "Delete Goal";
constexpr const char* NewGoalEditorPopup = "Goal Editor###goal_edit_popup_new";

std::string goalEditorPopupId(int goalId)
{
    return "Goal Editor###goal_edit_popup_" + std::to_string(goalId);
}

std::string goalDeletePopupId(int goalId)
{
    return "Delete Goal###goal_delete_popup_" + std::to_string(goalId);
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

bool matchesFilter(const Goal& goal, const std::string& query)
{
    if (query.empty()) {
        return true;
    }

    return containsCaseInsensitive(goal.goalName, query) ||
        containsCaseInsensitive(goal.category, query) ||
        containsCaseInsensitive(goal.targetDate, query) ||
        containsCaseInsensitive(goal.notes, query);
}

const Account* accountFor(const AppState& state, int accountId)
{
    for (const Account& account : state.accounts) {
        if (account.id == accountId) {
            return &account;
        }
    }

    return nullptr;
}

const char* accountNameFor(const AppState& state, int accountId)
{
    const Account* account = accountFor(state, accountId);
    return account == nullptr ? "Missing account" : account->accountName.c_str();
}

bool hasLinkedAccount(const AppState& state, const Goal& goal)
{
    return goal.linkedAccountId > 0 && accountFor(state, goal.linkedAccountId) != nullptr;
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

void GoalsView::render(AppState& state, GoalRepository& repository, const std::function<void()>& reloadData)
{
    UiTheme::sectionHeading("Goals", "Milestones for long-term progress.");

    UiTheme::pushButtonStyle(UiTheme::NeonMagenta);
    if (ImGui::Button("Add Goal")) {
        openCreate();
        openEditorPopup_ = true;
    }
    UiTheme::popButtonStyle();
    ImGui::SameLine();
    ImGui::SetNextItemWidth(300.0f);
    ImGui::InputTextWithHint("##GoalSearch", "Search goals", &searchText_);

    ImGui::Spacing();

    UiTheme::pushPanelStyle();
    ImGui::BeginChild("GoalsTablePanel", ImVec2(0.0f, 0.0f), true);
    int visibleRows = 0;
    if (state.goals.empty()) {
        UiTheme::emptyState("No goals yet", "Add a milestone to track progress over time.");
    } else {
        UiTheme::pushTableStyle();
        if (ImGui::BeginTable("GoalsTable", 8, ImGuiTableFlags_BordersInnerH | ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable | ImGuiTableFlags_Reorderable | ImGuiTableFlags_Hideable | ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_ScrollX)) {
        ImGui::TableSetupColumn("Goal", ImGuiTableColumnFlags_WidthStretch, 1.3f);
        ImGui::TableSetupColumn("Category", ImGuiTableColumnFlags_WidthFixed, 120.0f);
        ImGui::TableSetupColumn("Target", ImGuiTableColumnFlags_WidthFixed, 112.0f);
        ImGui::TableSetupColumn("Current", ImGuiTableColumnFlags_WidthFixed, 112.0f);
        ImGui::TableSetupColumn("Remaining", ImGuiTableColumnFlags_WidthFixed, 112.0f);
        ImGui::TableSetupColumn("Progress", ImGuiTableColumnFlags_WidthFixed, 170.0f);
        ImGui::TableSetupColumn("Target Date", ImGuiTableColumnFlags_WidthFixed, 110.0f);
        ImGui::TableSetupColumn("Actions", ImGuiTableColumnFlags_WidthFixed, 128.0f);
        ImGui::TableHeadersRow();

        for (const Goal& goal : state.goals) {
            if (!matchesFilter(goal, searchText_)) {
                continue;
            }

            const GoalMetrics metrics = PortfolioCalculator::calculateGoal(goal, state.accounts, state.holdings);
            ++visibleRows;

            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::Text("%s", goal.goalName.c_str());
            ImGui::TableNextColumn();
            ImGui::TextColored(UiTheme::MutedText, "%s", goal.category.c_str());
            ImGui::TableNextColumn();
            UiTheme::textRightAligned(Money::format(goal.targetAmount).c_str());
            ImGui::TableNextColumn();
            UiTheme::textRightAligned(Money::format(metrics.effectiveCurrentAmount).c_str(), metrics.missingLinkedAccount ? UiTheme::Loss : UiTheme::TextSecondary);
            if (goal.useAccountValue && ImGui::IsItemHovered()) {
                ImGui::SetTooltip("%s", metrics.missingLinkedAccount
                    ? "Linked account is missing; using 0.00."
                    : accountNameFor(state, goal.linkedAccountId));
            }
            ImGui::TableNextColumn();
            UiTheme::textRightAligned(Money::format(metrics.remainingAmount).c_str(), metrics.remainingAmount == 0.0 ? UiTheme::Gain : UiTheme::Amber);
            ImGui::TableNextColumn();
            ImGui::PushStyleColor(ImGuiCol_PlotHistogram, UiTheme::withAlpha(UiTheme::ElectricCyan, 0.72f));
            ImGui::PushStyleColor(ImGuiCol_FrameBg, UiTheme::withAlpha(UiTheme::SurfaceMain, 0.80f));
            ImGui::ProgressBar(static_cast<float>(metrics.progressPercent / 100.0), ImVec2(-1.0f, 0.0f), Money::formatPercent(metrics.progressPercent).c_str());
            ImGui::PopStyleColor(2);
            ImGui::TableNextColumn();
            DatePicker::drawTableDate(goal.targetDate, true);
            ImGui::TableNextColumn();
            const std::string editButtonId = "Edit##edit_button_" + std::to_string(goal.id);
            UiTheme::pushButtonStyle(UiTheme::ElectricCyan);
            if (ImGui::SmallButton(editButtonId.c_str())) {
                openEdit(goal);
                openEditorPopup_ = true;
            }
            UiTheme::popButtonStyle();
            ImGui::SameLine();
            const std::string deleteButtonId = "Delete##delete_button_" + std::to_string(goal.id);
            UiTheme::pushButtonStyle(UiTheme::Loss);
            if (ImGui::SmallButton(deleteButtonId.c_str())) {
                deleteId_ = goal.id;
                deleteName_ = goal.goalName;
                deletePopupId_ = goalDeletePopupId(goal.id);
                openDeletePopup_ = true;
            }
            UiTheme::popButtonStyle();
        }

        ImGui::EndTable();
        if (visibleRows == 0) {
            ImGui::TextColored(UiTheme::MutedText, "No goals match the current search.");
        }
        }
        UiTheme::popTableStyle();
    }
    ImGui::EndChild();
    UiTheme::popPanelStyle();

    if (openEditorPopup_) {
        ImGui::OpenPopup(editorPopupId_.empty() ? GoalEditorPopup : editorPopupId_.c_str());
        openEditorPopup_ = false;
    }
    if (openDeletePopup_) {
        ImGui::OpenPopup(deletePopupId_.empty() ? DeleteGoalPopup : deletePopupId_.c_str());
        openDeletePopup_ = false;
    }

    drawEditor(state, repository, reloadData);
    drawDeleteConfirmation(state, repository, reloadData);
}

void GoalsView::openCreate()
{
    draft_ = Goal {};
    draft_.category = "Portfolio";
    editing_ = false;
    editorPopupId_ = NewGoalEditorPopup;
    formError_.clear();
}

void GoalsView::openEdit(const Goal& goal)
{
    draft_ = goal;
    editing_ = true;
    editorPopupId_ = goalEditorPopupId(goal.id);
    formError_.clear();
}

void GoalsView::drawEditor(AppState& state, GoalRepository& repository, const std::function<void()>& reloadData)
{
    const char* popupId = editorPopupId_.empty() ? GoalEditorPopup : editorPopupId_.c_str();
    if (!ImGui::BeginPopupModal(popupId, nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        return;
    }

    ImGui::Text("%s", editing_ ? "Edit goal" : "Add goal");
    ImGui::Separator();

    ImGui::InputText("Goal name", &draft_.goalName);
    ImGui::InputDouble("Target amount", &draft_.targetAmount, 0.0, 0.0, "%.2f");

    bool useAccountValue = draft_.useAccountValue;
    if (state.accounts.empty() && !draft_.useAccountValue) {
        ImGui::BeginDisabled();
    }
    if (ImGui::Checkbox("Use account value", &useAccountValue)) {
        draft_.useAccountValue = useAccountValue;
        if (draft_.useAccountValue && !state.accounts.empty() && !hasLinkedAccount(state, draft_)) {
            draft_.linkedAccountId = state.accounts.front().id;
        }
    }
    if (state.accounts.empty() && !draft_.useAccountValue) {
        ImGui::EndDisabled();
    }
    ImGui::TextWrapped("Use account value links this goal to a calculated account balance instead of a manually entered amount.");

    if (state.accounts.empty()) {
        ImGui::TextColored(UiTheme::Amber, "Create an account before linking a goal to account value.");
    }

    if (draft_.useAccountValue) {
        const char* accountPreview = accountNameFor(state, draft_.linkedAccountId);
        if (ImGui::BeginCombo("Account", accountPreview)) {
            for (const Account& account : state.accounts) {
                const bool selected = draft_.linkedAccountId == account.id;
                if (ImGui::Selectable(account.accountName.c_str(), selected)) {
                    draft_.linkedAccountId = account.id;
                }
                if (selected) {
                    ImGui::SetItemDefaultFocus();
                }
            }
            ImGui::EndCombo();
        }

        const GoalMetrics linkedMetrics = PortfolioCalculator::calculateGoal(draft_, state.accounts, state.holdings);
        double calculatedCurrentAmount = linkedMetrics.effectiveCurrentAmount;
        ImGui::BeginDisabled();
        ImGui::InputDouble("Current amount", &calculatedCurrentAmount, 0.0, 0.0, "%.2f");
        ImGui::EndDisabled();
        ImGui::TextColored(UiTheme::MutedText, "Calculated as active holdings market value plus account cash balance.");
        if (linkedMetrics.missingLinkedAccount) {
            ImGui::TextColored(UiTheme::Loss, "Linked account is missing or invalid; current amount is treated as 0.00.");
        }
    } else {
        ImGui::InputDouble("Current amount", &draft_.currentAmount, 0.0, 0.0, "%.2f");
    }

    DatePicker::draw("Target date", draft_.targetDate, true);
    drawStringCombo("Category", draft_.category, std::array {
        "Portfolio",
        "Retirement",
        "Income",
        "Cash",
        "Education",
        "Other",
    });
    ImGui::InputTextMultiline("Notes", &draft_.notes, ImVec2(440.0f, 86.0f));

    const GoalMetrics metrics = PortfolioCalculator::calculateGoal(draft_, state.accounts, state.holdings);
    ImGui::Separator();
    ImGui::TextColored(UiTheme::MutedText, "Remaining: %s", Money::format(metrics.remainingAmount).c_str());
    ImGui::ProgressBar(static_cast<float>(metrics.progressPercent / 100.0), ImVec2(420.0f, 0.0f), Money::formatPercent(metrics.progressPercent).c_str());

    UiTheme::formError(formError_);

    if (ImGui::Button(editing_ ? "Save Changes" : "Create Goal", ImVec2(140.0f, 0.0f))) {
        std::string error;
        const bool saved = editing_ ? repository.update(draft_, error) : repository.create(draft_, error);
        if (saved) {
            reloadData();
            state.setStatus(editing_ ? "Goal updated." : "Goal created.");
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

void GoalsView::drawDeleteConfirmation(AppState& state, GoalRepository& repository, const std::function<void()>& reloadData)
{
    const char* popupId = deletePopupId_.empty() ? DeleteGoalPopup : deletePopupId_.c_str();
    if (!ImGui::BeginPopupModal(popupId, nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        return;
    }

    ImGui::Text("Delete goal?");
    ImGui::TextColored(UiTheme::MutedText, "%s", deleteName_.c_str());

    if (ImGui::Button("Delete", ImVec2(100.0f, 0.0f))) {
        std::string error;
        if (repository.remove(deleteId_, error)) {
            reloadData();
            state.setStatus("Goal deleted.");
            ImGui::CloseCurrentPopup();
        } else {
            state.setStatus("Could not delete goal: " + error, true);
            ImGui::CloseCurrentPopup();
        }
    }

    ImGui::SameLine();
    if (ImGui::Button("Cancel", ImVec2(100.0f, 0.0f))) {
        ImGui::CloseCurrentPopup();
    }

    ImGui::EndPopup();
}
