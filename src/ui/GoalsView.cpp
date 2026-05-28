// SPDX-License-Identifier: MIT
#include "ui/GoalsView.hpp"

#include "app/AppState.hpp"
#include "repositories/GoalRepository.hpp"
#include "services/PortfolioCalculator.hpp"
#include "ui/UiTheme.hpp"
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

    if (ImGui::Button("Add Goal")) {
        openCreate();
        ImGui::OpenPopup(GoalEditorPopup);
    }
    ImGui::SameLine();
    ImGui::SetNextItemWidth(300.0f);
    ImGui::InputTextWithHint("##GoalSearch", "Search goals", &searchText_);

    ImGui::Spacing();

    int visibleRows = 0;
    if (state.goals.empty()) {
        UiTheme::emptyState("No goals yet", "Add a milestone to track progress over time.");
    } else if (ImGui::BeginTable("GoalsTable", 8, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable | ImGuiTableFlags_Reorderable | ImGuiTableFlags_Hideable | ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_ScrollX)) {
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

            const GoalMetrics metrics = PortfolioCalculator::calculateGoal(goal);
            ++visibleRows;

            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::Text("%s", goal.goalName.c_str());
            ImGui::TableNextColumn();
            ImGui::TextColored(UiTheme::MutedText, "%s", goal.category.c_str());
            ImGui::TableNextColumn();
            ImGui::Text("%s", Money::format(goal.targetAmount).c_str());
            ImGui::TableNextColumn();
            ImGui::Text("%s", Money::format(goal.currentAmount).c_str());
            ImGui::TableNextColumn();
            ImGui::TextColored(metrics.remainingAmount == 0.0 ? UiTheme::Gain : UiTheme::Amber, "%s", Money::format(metrics.remainingAmount).c_str());
            ImGui::TableNextColumn();
            ImGui::ProgressBar(static_cast<float>(metrics.progressPercent / 100.0), ImVec2(-1.0f, 0.0f), Money::formatPercent(metrics.progressPercent).c_str());
            ImGui::TableNextColumn();
            ImGui::TextColored(UiTheme::MutedText, "%s", goal.targetDate.c_str());
            ImGui::TableNextColumn();
            ImGui::PushID(goal.id);
            if (ImGui::SmallButton("Edit")) {
                openEdit(goal);
                ImGui::OpenPopup(GoalEditorPopup);
            }
            ImGui::SameLine();
            if (ImGui::SmallButton("Delete")) {
                deleteId_ = goal.id;
                deleteName_ = goal.goalName;
                ImGui::OpenPopup(DeleteGoalPopup);
            }
            ImGui::PopID();
        }

        ImGui::EndTable();
        if (visibleRows == 0) {
            ImGui::TextColored(UiTheme::MutedText, "No goals match the current search.");
        }
    }

    drawEditor(state, repository, reloadData);
    drawDeleteConfirmation(state, repository, reloadData);
}

void GoalsView::openCreate()
{
    draft_ = Goal {};
    draft_.category = "Portfolio";
    editing_ = false;
    formError_.clear();
}

void GoalsView::openEdit(const Goal& goal)
{
    draft_ = goal;
    editing_ = true;
    formError_.clear();
}

void GoalsView::drawEditor(AppState& state, GoalRepository& repository, const std::function<void()>& reloadData)
{
    if (!ImGui::BeginPopupModal(GoalEditorPopup, nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        return;
    }

    ImGui::Text("%s", editing_ ? "Edit goal" : "Add goal");
    ImGui::Separator();

    ImGui::InputText("Goal name", &draft_.goalName);
    drawStringCombo("Category", draft_.category, std::array {
        "Portfolio",
        "Retirement",
        "Income",
        "Cash",
        "Education",
        "Other",
    });
    ImGui::InputDouble("Target amount", &draft_.targetAmount, 0.0, 0.0, "%.2f");
    ImGui::InputDouble("Current amount", &draft_.currentAmount, 0.0, 0.0, "%.2f");
    ImGui::InputText("Target date", &draft_.targetDate);
    ImGui::TextColored(UiTheme::MutedText, "Optional. Use YYYY-MM-DD.");
    ImGui::InputTextMultiline("Notes", &draft_.notes, ImVec2(440.0f, 86.0f));

    const GoalMetrics metrics = PortfolioCalculator::calculateGoal(draft_);
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
    if (!ImGui::BeginPopupModal(DeleteGoalPopup, nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
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
