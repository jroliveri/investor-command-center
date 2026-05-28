// SPDX-License-Identifier: MIT
#include "util/Date.hpp"

#include <chrono>
#include <ctime>
#include <cctype>
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

    return true;
}

}
