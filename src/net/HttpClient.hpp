// SPDX-License-Identifier: MIT
#pragma once

#include <string>

struct HttpResponse {
    int statusCode = 0;
    std::string body;
    std::string error;

    bool ok() const { return statusCode >= 200 && statusCode < 300 && error.empty(); }
};

class HttpClient {
public:
    HttpResponse getHttps(const std::string& host, const std::string& pathAndQuery, int timeoutMilliseconds = 8000) const;
};
