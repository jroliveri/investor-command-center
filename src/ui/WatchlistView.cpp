// SPDX-License-Identifier: MIT
#include "ui/WatchlistView.hpp"

#include "app/AppState.hpp"
#include "repositories/WatchlistRepository.hpp"
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

constexpr const char* WatchlistEditorPopup = "Watchlist Editor";
constexpr const char* DeleteWatchlistPopup = "Delete Watchlist Item";

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

bool matchesFilter(const WatchlistItem& item, const std::string& query)
{
    if (query.empty()) {
        return true;
    }

    return containsCaseInsensitive(item.ticker, query) ||
        containsCaseInsensitive(item.assetName, query) ||
        containsCaseInsensitive(item.assetType, query) ||
        containsCaseInsensitive(item.priority, query) ||
        containsCaseInsensitive(item.reasonWatching, query) ||
        containsCaseInsensitive(item.riskNotes, query);
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

double priceGap(const WatchlistItem& item)
{
    return item.currentPrice - item.targetBuyPrice;
}

}

void WatchlistView::render(AppState& state, WatchlistRepository& repository, const std::function<void()>& reloadData)
{
    UiTheme::sectionHeading("Watchlist", "Manual ideas under observation, without recommendations.");

    const WatchlistSummary summary = PortfolioCalculator::calculateWatchlist(state.watchlist);
    ImGui::TextColored(UiTheme::Loss, "High: %d", summary.highPriority);
    ImGui::SameLine();
    ImGui::TextColored(UiTheme::Amber, "Medium: %d", summary.mediumPriority);
    ImGui::SameLine();
    ImGui::TextColored(UiTheme::MutedText, "Low: %d", summary.lowPriority);

    if (ImGui::Button("Add Watchlist Item")) {
        openCreate();
        ImGui::OpenPopup(WatchlistEditorPopup);
    }
    ImGui::SameLine();
    ImGui::SetNextItemWidth(300.0f);
    ImGui::InputTextWithHint("##WatchlistSearch", "Search watchlist", &searchText_);

    ImGui::Spacing();

    int visibleRows = 0;
    if (state.watchlist.empty()) {
        UiTheme::emptyState("No watchlist items yet", "Add assets you want to monitor manually.");
    } else if (ImGui::BeginTable("WatchlistTable", 9, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable | ImGuiTableFlags_Reorderable | ImGuiTableFlags_Hideable | ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_ScrollX)) {
        ImGui::TableSetupColumn("Priority", ImGuiTableColumnFlags_WidthFixed, 90.0f);
        ImGui::TableSetupColumn("Ticker", ImGuiTableColumnFlags_WidthFixed, 76.0f);
        ImGui::TableSetupColumn("Asset", ImGuiTableColumnFlags_WidthStretch, 1.2f);
        ImGui::TableSetupColumn("Type", ImGuiTableColumnFlags_WidthFixed, 110.0f);
        ImGui::TableSetupColumn("Target", ImGuiTableColumnFlags_WidthFixed, 100.0f);
        ImGui::TableSetupColumn("Current", ImGuiTableColumnFlags_WidthFixed, 100.0f);
        ImGui::TableSetupColumn("Gap", ImGuiTableColumnFlags_WidthFixed, 100.0f);
        ImGui::TableSetupColumn("Reason", ImGuiTableColumnFlags_WidthStretch, 1.0f);
        ImGui::TableSetupColumn("Actions", ImGuiTableColumnFlags_WidthFixed, 128.0f);
        ImGui::TableHeadersRow();

        for (const WatchlistItem& item : state.watchlist) {
            if (!matchesFilter(item, searchText_)) {
                continue;
            }

            ++visibleRows;
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            drawPriorityBadge(item.priority);
            ImGui::TableNextColumn();
            ImGui::Text("%s", item.ticker.c_str());
            ImGui::TableNextColumn();
            ImGui::Text("%s", item.assetName.c_str());
            ImGui::TableNextColumn();
            ImGui::TextColored(UiTheme::MutedText, "%s", item.assetType.c_str());
            ImGui::TableNextColumn();
            ImGui::Text("%s", Money::format(item.targetBuyPrice).c_str());
            ImGui::TableNextColumn();
            ImGui::Text("%s", Money::format(item.currentPrice).c_str());
            ImGui::TableNextColumn();
            ImGui::TextColored(UiTheme::moneyColor(-priceGap(item)), "%s", Money::format(priceGap(item)).c_str());
            ImGui::TableNextColumn();
            ImGui::TextColored(UiTheme::MutedText, "%s", item.reasonWatching.c_str());
            ImGui::TableNextColumn();
            ImGui::PushID(item.id);
            if (ImGui::SmallButton("Edit")) {
                openEdit(item);
                ImGui::OpenPopup(WatchlistEditorPopup);
            }
            ImGui::SameLine();
            if (ImGui::SmallButton("Delete")) {
                deleteId_ = item.id;
                deleteName_ = item.ticker + " - " + item.assetName;
                ImGui::OpenPopup(DeleteWatchlistPopup);
            }
            ImGui::PopID();
        }

        ImGui::EndTable();
        if (visibleRows == 0) {
            ImGui::TextColored(UiTheme::MutedText, "No watchlist items match the current search.");
        }
    }

    drawEditor(state, repository, reloadData);
    drawDeleteConfirmation(state, repository, reloadData);
}

void WatchlistView::openCreate()
{
    draft_ = WatchlistItem {};
    draft_.assetType = "Stock";
    draft_.priority = "Medium";
    editing_ = false;
    formError_.clear();
}

void WatchlistView::openEdit(const WatchlistItem& item)
{
    draft_ = item;
    editing_ = true;
    formError_.clear();
}

void WatchlistView::drawEditor(AppState& state, WatchlistRepository& repository, const std::function<void()>& reloadData)
{
    if (!ImGui::BeginPopupModal(WatchlistEditorPopup, nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        return;
    }

    ImGui::Text("%s", editing_ ? "Edit watchlist item" : "Add watchlist item");
    ImGui::Separator();

    ImGui::InputText("Ticker", &draft_.ticker, ImGuiInputTextFlags_CharsUppercase | ImGuiInputTextFlags_CharsNoBlank);
    ImGui::InputText("Asset name", &draft_.assetName);
    drawStringCombo("Asset type", draft_.assetType, std::array {
        "Stock",
        "ETF",
        "Mutual Fund",
        "Bond",
        "Crypto",
        "Other",
    });
    drawStringCombo("Priority", draft_.priority, std::array {
        "High",
        "Medium",
        "Low",
    });
    ImGui::InputDouble("Target buy price", &draft_.targetBuyPrice, 0.0, 0.0, "%.4f");
    ImGui::InputDouble("Current price", &draft_.currentPrice, 0.0, 0.0, "%.4f");
    ImGui::InputTextMultiline("Reason watching", &draft_.reasonWatching, ImVec2(440.0f, 70.0f));
    ImGui::InputTextMultiline("Risk notes", &draft_.riskNotes, ImVec2(440.0f, 70.0f));

    ImGui::Separator();
    ImGui::TextColored(UiTheme::MutedText, "Current minus target: %s", Money::format(priceGap(draft_)).c_str());

    UiTheme::formError(formError_);

    if (ImGui::Button(editing_ ? "Save Changes" : "Create Item", ImVec2(140.0f, 0.0f))) {
        std::string error;
        const bool saved = editing_ ? repository.update(draft_, error) : repository.create(draft_, error);
        if (saved) {
            reloadData();
            state.setStatus(editing_ ? "Watchlist item updated." : "Watchlist item created.");
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

void WatchlistView::drawDeleteConfirmation(AppState& state, WatchlistRepository& repository, const std::function<void()>& reloadData)
{
    if (!ImGui::BeginPopupModal(DeleteWatchlistPopup, nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        return;
    }

    ImGui::Text("Delete watchlist item?");
    ImGui::TextColored(UiTheme::MutedText, "%s", deleteName_.c_str());

    if (ImGui::Button("Delete", ImVec2(100.0f, 0.0f))) {
        std::string error;
        if (repository.remove(deleteId_, error)) {
            reloadData();
            state.setStatus("Watchlist item deleted.");
            ImGui::CloseCurrentPopup();
        } else {
            state.setStatus("Could not delete watchlist item: " + error, true);
            ImGui::CloseCurrentPopup();
        }
    }

    ImGui::SameLine();
    if (ImGui::Button("Cancel", ImVec2(100.0f, 0.0f))) {
        ImGui::CloseCurrentPopup();
    }

    ImGui::EndPopup();
}

void WatchlistView::drawPriorityBadge(const std::string& priority) const
{
    ImVec4 color = UiTheme::Amber;
    if (priority == "High") {
        color = UiTheme::Loss;
    } else if (priority == "Low") {
        color = UiTheme::MutedText;
    }

    ImGui::TextColored(color, "%s", priority.c_str());
}
