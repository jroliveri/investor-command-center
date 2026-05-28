// SPDX-License-Identifier: MIT
#include "util/Money.hpp"

#include <cmath>
#include <iomanip>
#include <sstream>
#include <string>

namespace {

std::string addThousandsSeparators(const std::string& digits)
{
    std::string result;
    int count = 0;
    for (auto it = digits.rbegin(); it != digits.rend(); ++it) {
        if (count == 3) {
            result.insert(result.begin(), ',');
            count = 0;
        }
        result.insert(result.begin(), *it);
        ++count;
    }
    return result;
}

}

namespace Money {

std::string formatNumber(double value, int decimals)
{
    const bool negative = value < 0.0;
    const double absoluteValue = std::fabs(value);

    std::ostringstream stream;
    stream << std::fixed << std::setprecision(decimals) << absoluteValue;
    std::string number = stream.str();

    const std::size_t decimalPos = number.find('.');
    const std::string whole = decimalPos == std::string::npos ? number : number.substr(0, decimalPos);
    const std::string fraction = decimalPos == std::string::npos ? std::string() : number.substr(decimalPos);

    return std::string(negative ? "-" : "") + addThousandsSeparators(whole) + fraction;
}

std::string format(double value)
{
    if (std::fabs(value) < 0.005) {
        value = 0.0;
    }

    const bool negative = value < 0.0;
    const double absoluteValue = std::fabs(value);

    std::ostringstream stream;
    stream << std::fixed << std::setprecision(2) << absoluteValue;
    std::string number = stream.str();

    const std::size_t decimalPos = number.find('.');
    const std::string whole = decimalPos == std::string::npos ? number : number.substr(0, decimalPos);
    const std::string cents = decimalPos == std::string::npos ? "00" : number.substr(decimalPos);

    return std::string(negative ? "-$" : "$") + addThousandsSeparators(whole) + cents;
}

std::string formatQuantity(double value)
{
    return formatNumber(value, 4);
}

std::string formatPercent(double value, bool includeSign)
{
    if (std::fabs(value) < 0.005) {
        value = 0.0;
    }

    std::ostringstream stream;
    if (includeSign && value > 0.0) {
        stream << '+';
    }
    stream << std::fixed << std::setprecision(2) << value << "%";
    return stream.str();
}

}
