// SPDX-License-Identifier: MIT
#pragma once

#include <string>

namespace Date {
struct DateParts {
    int year = 1970;
    int month = 1;
    int day = 1;
};

std::string nowIso8601();
std::string todayIso8601();
DateParts todayParts();
std::string currentMonthPrefix();
std::string currentYearPrefix();
bool isIsoDate(const std::string& value);
bool parseIsoDate(const std::string& value, DateParts& parts);
std::string formatIsoDate(int year, int month, int day);
bool isLeapYear(int year);
int daysInMonth(int year, int month);
DateParts clampDate(int year, int month, int day);
}
