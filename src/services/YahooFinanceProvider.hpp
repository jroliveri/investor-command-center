// SPDX-License-Identifier: MIT
#pragma once

#include "net/HttpClient.hpp"
#include "services/MarketDataProvider.hpp"

class YahooFinanceProvider final : public MarketDataProvider {
public:
    const char* providerName() const override;
    MarketQuoteResult fetchQuote(const std::string& symbol) override;
    HistoricalPriceResult fetchHistoricalPrices(const std::string& symbol, const std::string& range, const std::string& interval) override;

private:
    MarketQuoteResult fetchQuoteEndpoint(const std::string& symbol);
    MarketQuoteResult fetchChartFallback(const std::string& symbol);

    HttpClient httpClient_;
};
