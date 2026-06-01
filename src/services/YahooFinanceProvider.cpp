// SPDX-License-Identifier: MIT
#include "services/YahooFinanceProvider.hpp"

#include "util/Date.hpp"

#include <algorithm>
#include <charconv>
#include <cctype>
#include <ctime>
#include <iomanip>
#include <optional>
#include <sstream>
#include <string>

namespace {

constexpr const char* YahooHost = "query1.finance.yahoo.com";

std::string trim(std::string value)
{
    auto isNotSpace = [](unsigned char character) {
        return std::isspace(character) == 0;
    };
    value.erase(value.begin(), std::find_if(value.begin(), value.end(), isNotSpace));
    value.erase(std::find_if(value.rbegin(), value.rend(), isNotSpace).base(), value.end());
    return value;
}

std::string normalizeSymbol(std::string symbol)
{
    symbol = trim(std::move(symbol));
    std::transform(symbol.begin(), symbol.end(), symbol.begin(), [](unsigned char character) {
        return static_cast<char>(std::toupper(character));
    });
    return symbol;
}

std::string urlEncode(const std::string& value)
{
    std::ostringstream stream;
    stream << std::uppercase << std::hex;
    for (unsigned char character : value) {
        if (std::isalnum(character) != 0 || character == '-' || character == '_' || character == '.' || character == '~') {
            stream << static_cast<char>(character);
        } else {
            stream << '%' << std::setw(2) << std::setfill('0') << static_cast<int>(character);
        }
    }
    return stream.str();
}

std::string findJsonObjectStartingAt(const std::string& json, std::size_t openBrace)
{
    if (openBrace == std::string::npos || openBrace >= json.size() || json[openBrace] != '{') {
        return {};
    }

    int depth = 0;
    bool inString = false;
    bool escaped = false;
    for (std::size_t index = openBrace; index < json.size(); ++index) {
        const char character = json[index];
        if (inString) {
            if (escaped) {
                escaped = false;
            } else if (character == '\\') {
                escaped = true;
            } else if (character == '"') {
                inString = false;
            }
            continue;
        }

        if (character == '"') {
            inString = true;
        } else if (character == '{') {
            ++depth;
        } else if (character == '}') {
            --depth;
            if (depth == 0) {
                return json.substr(openBrace, index - openBrace + 1);
            }
        }
    }

    return {};
}

std::string firstArrayObjectForKey(const std::string& json, const char* key)
{
    const std::string keyToken = "\"" + std::string(key) + "\"";
    const std::size_t keyPos = json.find(keyToken);
    if (keyPos == std::string::npos) {
        return {};
    }

    const std::size_t arrayStart = json.find('[', keyPos + keyToken.size());
    const std::size_t objectStart = json.find('{', arrayStart);
    return findJsonObjectStartingAt(json, objectStart);
}

std::string objectForKey(const std::string& json, const char* key)
{
    const std::string keyToken = "\"" + std::string(key) + "\"";
    const std::size_t keyPos = json.find(keyToken);
    if (keyPos == std::string::npos) {
        return {};
    }

    const std::size_t objectStart = json.find('{', keyPos + keyToken.size());
    return findJsonObjectStartingAt(json, objectStart);
}

std::optional<std::string> jsonString(const std::string& object, const char* key)
{
    const std::string keyToken = "\"" + std::string(key) + "\"";
    const std::size_t keyPos = object.find(keyToken);
    if (keyPos == std::string::npos) {
        return std::nullopt;
    }

    std::size_t valueStart = object.find(':', keyPos + keyToken.size());
    if (valueStart == std::string::npos) {
        return std::nullopt;
    }
    ++valueStart;
    while (valueStart < object.size() && std::isspace(static_cast<unsigned char>(object[valueStart])) != 0) {
        ++valueStart;
    }
    if (valueStart >= object.size() || object[valueStart] != '"') {
        return std::nullopt;
    }

    ++valueStart;
    std::string value;
    bool escaped = false;
    for (std::size_t index = valueStart; index < object.size(); ++index) {
        const char character = object[index];
        if (escaped) {
            switch (character) {
            case '"':
            case '\\':
            case '/':
                value.push_back(character);
                break;
            case 'n':
                value.push_back('\n');
                break;
            case 'r':
                value.push_back('\r');
                break;
            case 't':
                value.push_back('\t');
                break;
            default:
                value.push_back(character);
                break;
            }
            escaped = false;
            continue;
        }

        if (character == '\\') {
            escaped = true;
        } else if (character == '"') {
            return value;
        } else {
            value.push_back(character);
        }
    }

    return std::nullopt;
}

std::optional<double> jsonNumber(const std::string& object, const char* key)
{
    const std::string keyToken = "\"" + std::string(key) + "\"";
    const std::size_t keyPos = object.find(keyToken);
    if (keyPos == std::string::npos) {
        return std::nullopt;
    }

    std::size_t valueStart = object.find(':', keyPos + keyToken.size());
    if (valueStart == std::string::npos) {
        return std::nullopt;
    }
    ++valueStart;
    while (valueStart < object.size() && std::isspace(static_cast<unsigned char>(object[valueStart])) != 0) {
        ++valueStart;
    }
    if (valueStart >= object.size() || object.compare(valueStart, 4, "null") == 0) {
        return std::nullopt;
    }

    std::size_t valueEnd = valueStart;
    while (valueEnd < object.size()) {
        const char character = object[valueEnd];
        if (!(std::isdigit(static_cast<unsigned char>(character)) != 0 || character == '-' || character == '+' || character == '.' || character == 'e' || character == 'E')) {
            break;
        }
        ++valueEnd;
    }

    if (valueEnd == valueStart) {
        return std::nullopt;
    }

    double parsed = 0.0;
    const std::string token = object.substr(valueStart, valueEnd - valueStart);
    const auto result = std::from_chars(token.data(), token.data() + token.size(), parsed);
    if (result.ec != std::errc()) {
        return std::nullopt;
    }
    return parsed;
}

std::string firstAvailableString(const std::string& object, std::initializer_list<const char*> keys)
{
    for (const char* key : keys) {
        if (const auto value = jsonString(object, key); value.has_value() && !value->empty()) {
            return *value;
        }
    }
    return {};
}

std::optional<double> firstAvailableNumber(const std::string& object, std::initializer_list<const char*> keys)
{
    for (const char* key : keys) {
        if (const auto value = jsonNumber(object, key); value.has_value()) {
            return value;
        }
    }
    return std::nullopt;
}

std::string epochSecondsToLocalTime(const std::optional<double>& seconds)
{
    if (!seconds.has_value() || *seconds <= 0.0) {
        return {};
    }

    const std::time_t time = static_cast<std::time_t>(*seconds);
    std::tm localTime {};
    localtime_s(&localTime, &time);

    std::ostringstream stream;
    stream << std::put_time(&localTime, "%Y-%m-%d %H:%M:%S");
    return stream.str();
}

MarketQuote quoteFromQuoteObject(const std::string& object, const std::string& fallbackSymbol)
{
    MarketQuote quote;
    quote.provider = "Yahoo Finance";
    quote.symbol = normalizeSymbol(firstAvailableString(object, { "symbol" }));
    if (quote.symbol.empty()) {
        quote.symbol = fallbackSymbol;
    }
    quote.companyName = firstAvailableString(object, { "longName", "shortName", "displayName" });
    quote.currentPrice = firstAvailableNumber(object, { "regularMarketPrice" });
    quote.priceChangeDollar = firstAvailableNumber(object, { "regularMarketChange" });
    quote.priceChangePercent = firstAvailableNumber(object, { "regularMarketChangePercent" });
    quote.previousClose = firstAvailableNumber(object, { "regularMarketPreviousClose", "previousClose" });
    quote.openPrice = firstAvailableNumber(object, { "regularMarketOpen", "open" });
    quote.dayHigh = firstAvailableNumber(object, { "regularMarketDayHigh", "dayHigh" });
    quote.dayLow = firstAvailableNumber(object, { "regularMarketDayLow", "dayLow" });
    quote.fiftyTwoWeekHigh = firstAvailableNumber(object, { "fiftyTwoWeekHigh" });
    quote.fiftyTwoWeekLow = firstAvailableNumber(object, { "fiftyTwoWeekLow" });
    quote.marketCap = firstAvailableNumber(object, { "marketCap" });
    quote.volume = firstAvailableNumber(object, { "regularMarketVolume", "volume" });
    quote.averageVolume = firstAvailableNumber(object, { "averageDailyVolume3Month", "averageDailyVolume10Day" });
    quote.peRatio = firstAvailableNumber(object, { "trailingPE", "forwardPE" });
    quote.eps = firstAvailableNumber(object, { "epsTrailingTwelveMonths", "epsForward" });
    quote.dividendYield = firstAvailableNumber(object, { "dividendYield" });
    if (!quote.dividendYield.has_value()) {
        if (const auto decimalYield = firstAvailableNumber(object, { "trailingAnnualDividendYield" }); decimalYield.has_value()) {
            quote.dividendYield = *decimalYield * 100.0;
        }
    }
    quote.beta = firstAvailableNumber(object, { "beta" });
    quote.currency = firstAvailableString(object, { "currency", "financialCurrency" });
    quote.exchangeName = firstAvailableString(object, { "fullExchangeName", "exchange", "exchangeName" });
    quote.quoteTime = epochSecondsToLocalTime(firstAvailableNumber(object, { "regularMarketTime", "postMarketTime" }));
    quote.fetchedAt = Date::nowIso8601();
    quote.rawStatus = "Fetched";
    return quote;
}

std::string httpStatusError(const HttpResponse& response)
{
    if (!response.error.empty()) {
        return response.error;
    }
    if (response.statusCode == 401 || response.statusCode == 403) {
        return "Yahoo Finance rejected the quote request. The public endpoint may require browser cookies or may be temporarily unavailable.";
    }
    if (response.statusCode == 429) {
        return "Yahoo Finance rate-limited the quote request. Try again later.";
    }
    return "Yahoo Finance returned HTTP " + std::to_string(response.statusCode) + ".";
}

} // namespace

const char* YahooFinanceProvider::providerName() const
{
    return "Yahoo Finance";
}

MarketQuoteResult YahooFinanceProvider::fetchQuote(const std::string& symbol)
{
    const std::string normalizedSymbol = normalizeSymbol(symbol);
    if (normalizedSymbol.empty()) {
        MarketQuoteResult result;
        result.error = "Ticker is required.";
        return result;
    }

    MarketQuoteResult quoteResult = fetchQuoteEndpoint(normalizedSymbol);
    if (quoteResult.success) {
        return quoteResult;
    }

    MarketQuoteResult fallbackResult = fetchChartFallback(normalizedSymbol);
    if (fallbackResult.success) {
        fallbackResult.quote.rawStatus = "Fetched from Yahoo chart fallback after quote endpoint error: " + quoteResult.error;
        return fallbackResult;
    }

    if (!fallbackResult.error.empty()) {
        quoteResult.error += " Fallback also failed: " + fallbackResult.error;
    }
    return quoteResult;
}

MarketQuoteResult YahooFinanceProvider::fetchQuoteEndpoint(const std::string& symbol)
{
    MarketQuoteResult result;
    result.quote.symbol = symbol;

    const std::string path = "/v7/finance/quote?symbols=" + urlEncode(symbol);
    const HttpResponse response = httpClient_.getHttps(YahooHost, path);
    if (!response.ok()) {
        result.error = httpStatusError(response);
        return result;
    }

    const std::string quoteObject = firstArrayObjectForKey(response.body, "result");
    if (quoteObject.empty()) {
        result.error = "Yahoo Finance returned no quote result for " + symbol + ".";
        return result;
    }

    result.quote = quoteFromQuoteObject(quoteObject, symbol);
    result.success = !result.quote.symbol.empty();
    if (!result.success) {
        result.error = "Yahoo Finance returned an incomplete quote for " + symbol + ".";
    }
    return result;
}

MarketQuoteResult YahooFinanceProvider::fetchChartFallback(const std::string& symbol)
{
    MarketQuoteResult result;
    result.quote.symbol = symbol;

    const std::string path = "/v8/finance/chart/" + urlEncode(symbol) + "?range=1d&interval=1m";
    const HttpResponse response = httpClient_.getHttps(YahooHost, path);
    if (!response.ok()) {
        result.error = httpStatusError(response);
        return result;
    }

    const std::string resultObject = firstArrayObjectForKey(response.body, "result");
    const std::string metaObject = objectForKey(resultObject, "meta");
    if (metaObject.empty()) {
        result.error = "Yahoo Finance returned no chart metadata for " + symbol + ".";
        return result;
    }

    MarketQuote quote = quoteFromQuoteObject(metaObject, symbol);
    if (quote.companyName.empty()) {
        quote.companyName = symbol;
    }
    quote.rawStatus = "Fetched from chart metadata";

    result.quote = quote;
    result.success = !quote.symbol.empty();
    if (!result.success) {
        result.error = "Yahoo Finance returned incomplete chart metadata for " + symbol + ".";
    }
    return result;
}
