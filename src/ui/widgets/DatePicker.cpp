// SPDX-License-Identifier: MIT
#include "ui/widgets/DatePicker.hpp"

#include "ui/UiTheme.hpp"
#include "util/Date.hpp"

#include <imgui.h>
#include <imgui_stdlib.h>

#include <array>
#include <ctime>
#include <map>
#include <string>

namespace {

struct PickerState {
    int year = 1970;
    int month = 1;
};

const std::array<const char*, 12> MonthNames = {
    "January",
    "February",
    "March",
    "April",
    "May",
    "June",
    "July",
    "August",
    "September",
    "October",
    "November",
    "December",
};

const std::array<const char*, 7> WeekdayNames = {
    "Sun",
    "Mon",
    "Tue",
    "Wed",
    "Thu",
    "Fri",
    "Sat",
};

PickerState stateForValue(const std::string& value)
{
    Date::DateParts parts;
    if (!value.empty() && Date::parseIsoDate(value, parts)) {
        return PickerState { parts.year, parts.month };
    }

    const Date::DateParts today = Date::todayParts();
    return PickerState { today.year, today.month };
}

int firstWeekdayOfMonth(int year, int month)
{
    std::tm monthStart {};
    monthStart.tm_year = year - 1900;
    monthStart.tm_mon = month - 1;
    monthStart.tm_mday = 1;
    std::mktime(&monthStart);
    return monthStart.tm_wday;
}

void moveMonth(PickerState& state, int direction)
{
    state.month += direction;
    while (state.month < 1) {
        state.month += 12;
        --state.year;
    }
    while (state.month > 12) {
        state.month -= 12;
        ++state.year;
    }
    if (state.year < 1) {
        state.year = 1;
        state.month = 1;
    }
}

}

namespace DatePicker {

bool draw(const char* label, std::string& value, bool optional)
{
    bool changed = false;

    ImGui::PushID(label);
    const ImGuiID pickerId = ImGui::GetID("DatePicker");
    static std::map<ImGuiID, PickerState> pickerStates;

    ImGui::SetNextItemWidth(160.0f);
    if (ImGui::InputText(label, &value)) {
        changed = true;
    }

    ImGui::SameLine();
    UiTheme::pushButtonStyle(UiTheme::ElectricCyan);
    if (ImGui::Button("Pick Date")) {
        pickerStates[pickerId] = stateForValue(value);
        ImGui::OpenPopup("DatePickerPopup");
    }
    UiTheme::popButtonStyle();

    PickerState& state = pickerStates[pickerId];
    if (state.year == 0) {
        state = stateForValue(value);
    }

    if (ImGui::BeginPopup("DatePickerPopup")) {
        UiTheme::pushButtonStyle(UiTheme::ElectricCyan);
        if (ImGui::Button("<")) {
            moveMonth(state, -1);
        }
        UiTheme::popButtonStyle();
        ImGui::SameLine();
        const std::string heading = std::string(MonthNames[static_cast<std::size_t>(state.month - 1)]) + " " + std::to_string(state.year);
        ImGui::TextColored(UiTheme::TextPrimary, "%s", heading.c_str());
        ImGui::SameLine();
        UiTheme::pushButtonStyle(UiTheme::ElectricCyan);
        if (ImGui::Button(">")) {
            moveMonth(state, 1);
        }
        UiTheme::popButtonStyle();

        UiTheme::pushButtonStyle(UiTheme::NeonMagenta);
        if (ImGui::Button("Today")) {
            const Date::DateParts today = Date::todayParts();
            state.year = today.year;
            state.month = today.month;
            value = Date::formatIsoDate(today.year, today.month, today.day);
            changed = true;
            ImGui::CloseCurrentPopup();
        }
        UiTheme::popButtonStyle();

        if (optional) {
            ImGui::SameLine();
            UiTheme::pushButtonStyle(UiTheme::ElectricCyan);
            if (ImGui::Button("Clear")) {
                value.clear();
                changed = true;
                ImGui::CloseCurrentPopup();
            }
            UiTheme::popButtonStyle();
        }

        ImGui::Separator();
        UiTheme::pushTableStyle();
        if (ImGui::BeginTable("DatePickerCalendar", 7, ImGuiTableFlags_SizingFixedFit)) {
            for (const char* weekday : WeekdayNames) {
                ImGui::TableSetupColumn(weekday, ImGuiTableColumnFlags_WidthFixed, 38.0f);
            }
            ImGui::TableHeadersRow();

            const int firstWeekday = firstWeekdayOfMonth(state.year, state.month);
            const int daysInMonth = Date::daysInMonth(state.year, state.month);
            int day = 1;
            for (int week = 0; week < 6 && day <= daysInMonth; ++week) {
                ImGui::TableNextRow();
                for (int weekday = 0; weekday < 7; ++weekday) {
                    ImGui::TableSetColumnIndex(weekday);
                    if ((week == 0 && weekday < firstWeekday) || day > daysInMonth) {
                        ImGui::TextUnformatted("");
                        continue;
                    }

                    const std::string dayLabel = std::to_string(day);
                    if (ImGui::SmallButton(dayLabel.c_str())) {
                        value = Date::formatIsoDate(state.year, state.month, day);
                        changed = true;
                        ImGui::CloseCurrentPopup();
                    }
                    ++day;
                }
            }

            ImGui::EndTable();
        }
        UiTheme::popTableStyle();

        ImGui::EndPopup();
    }

    const bool hasInvalidManualValue = !value.empty() && !Date::isIsoDate(value);
    if (hasInvalidManualValue) {
        ImGui::TextColored(UiTheme::TextDanger, "Date must be in YYYY-MM-DD format.");
    } else if (optional) {
        ImGui::TextColored(UiTheme::TextMuted, "Optional. Dates are stored as YYYY-MM-DD.");
    } else {
        ImGui::TextColored(UiTheme::TextMuted, "Dates are stored as YYYY-MM-DD.");
    }

    ImGui::PopID();
    return changed;
}

void drawTableDate(const std::string& value, bool optional)
{
    if (value.empty()) {
        ImGui::TextColored(UiTheme::TextMuted, "%s", optional ? "-" : "Missing");
        if (!optional && ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Date is required.");
        }
        return;
    }

    if (!Date::isIsoDate(value)) {
        ImGui::TextColored(UiTheme::TextDanger, "%s", value.c_str());
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Invalid date. Expected YYYY-MM-DD.");
        }
        return;
    }

    ImGui::Text("%s", value.c_str());
}

}
