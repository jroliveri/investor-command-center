// SPDX-License-Identifier: MIT
#pragma once

#include "net/HttpClient.hpp"
#include "services/MarketDataProvider.hpp"

class YahooFinanceProvider final : public MarketDataProvider {
public:
    const char* providerName() const override;
    MarketQuoteResult fetchQuote(const std::string& symbol) override;

private:
    MarketQuoteResult fetchQuoteEndpoint(const std::string& symbol);
    MarketQuoteResult fetchChartFallback(const std::string& symbol);

    HttpClient httpClient_;
};
