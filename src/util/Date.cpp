// SPDX-License-Identifier: MIT
#include "util/Date.hpp"

#include <chrono>
#include <cctype>
#include <ctime>
#include <iomanip>
#include <sstream>

namespace Date {

namespace {

std::tm localNow()
{
    const auto now = std::chrono::system_clock::now();
    const std::time_t nowTime = std::chrono::system_clock::to_time_t(now);

    std::tm localTime {};
    localtime_s(&localTime, &nowTime);
    return localTime;
}

int parseInt(const std::string& value, std::size_t offset, std::size_t length)
{
    int result = 0;
    for (std::size_t index = offset; index < offset + length; ++index) {
        result = result * 10 + (value[index] - '0');
    }
    return result;
}

}

std::string nowIso8601()
{
    const std::tm localTime = localNow();

    std::ostringstream stream;
    stream << std::put_time(&localTime, "%Y-%m-%d %H:%M:%S");
    return stream.str();
}

std::string todayIso8601()
{
    const std::tm localTime = localNow();

    std::ostringstream stream;
    stream << std::put_time(&localTime, "%Y-%m-%d");
    return stream.str();
}

DateParts todayParts()
{
    const std::tm localTime = localNow();
    return DateParts {
        localTime.tm_year + 1900,
        localTime.tm_mon + 1,
        localTime.tm_mday,
    };
}

std::string currentMonthPrefix()
{
    const std::tm localTime = localNow();

    std::ostringstream stream;
    stream << std::put_time(&localTime, "%Y-%m");
    return stream.str();
}

std::string currentYearPrefix()
{
    const std::tm localTime = localNow();

    std::ostringstream stream;
    stream << std::put_time(&localTime, "%Y");
    return stream.str();
}

bool isIsoDate(const std::string& value)
{
    DateParts parts;
    return parseIsoDate(value, parts);
}

bool parseIsoDate(const std::string& value, DateParts& parts)
{
    if (value.size() != 10) {
        return false;
    }

    if (value[4] != '-' || value[7] != '-') {
        return false;
    }

    for (std::size_t index = 0; index < value.size(); ++index) {
        if (index == 4 || index == 7) {
            continue;
        }

        if (std::isdigit(static_cast<unsigned char>(value[index])) == 0) {
            return false;
        }
    }

    const int year = parseInt(value, 0, 4);
    const int month = parseInt(value, 5, 2);
    const int day = parseInt(value, 8, 2);

    if (year < 1 || month < 1 || month > 12) {
        return false;
    }

    const int maxDay = daysInMonth(year, month);
    if (day < 1 || day > maxDay) {
        return false;
    }

    parts = DateParts { year, month, day };
    return true;
}

std::string formatIsoDate(int year, int month, int day)
{
    const DateParts parts = clampDate(year, month, day);

    std::ostringstream stream;
    stream << std::setfill('0')
           << std::setw(4) << parts.year
           << '-'
           << std::setw(2) << parts.month
           << '-'
           << std::setw(2) << parts.day;
    return stream.str();
}

bool isLeapYear(int year)
{
    return (year % 4 == 0 && year % 100 != 0) || year % 400 == 0;
}

int daysInMonth(int year, int month)
{
    if (month < 1 || month > 12) {
        return 31;
    }

    static constexpr int DaysByMonth[] = {
        31, 28, 31, 30, 31, 30,
        31, 31, 30, 31, 30, 31,
    };

    if (month == 2 && isLeapYear(year)) {
        return 29;
    }

    return DaysByMonth[month - 1];
}

DateParts clampDate(int year, int month, int day)
{
    if (year < 1) {
        year = 1;
    }

    if (month < 1) {
        month = 1;
    } else if (month > 12) {
        month = 12;
    }

    const int maxDay = daysInMonth(year, month);
    if (day < 1) {
        day = 1;
    } else if (day > maxDay) {
        day = maxDay;
    }

    return DateParts { year, month, day };
}

}
