// SPDX-License-Identifier: MIT
#include "net/HttpClient.hpp"

#include <windows.h>
#include <winhttp.h>

#include <string>

namespace {

std::wstring widenAscii(const std::string& value)
{
    return std::wstring(value.begin(), value.end());
}

std::string windowsErrorMessage(DWORD errorCode)
{
    if (errorCode == 0) {
        return "Unknown WinHTTP error.";
    }

    LPWSTR buffer = nullptr;
    const DWORD size = FormatMessageW(
        FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
        nullptr,
        errorCode,
        MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
        reinterpret_cast<LPWSTR>(&buffer),
        0,
        nullptr);

    if (size == 0 || buffer == nullptr) {
        return "WinHTTP error " + std::to_string(errorCode) + ".";
    }

    const std::wstring wideMessage(buffer, size);
    LocalFree(buffer);

    int utf8Size = WideCharToMultiByte(CP_UTF8, 0, wideMessage.c_str(), static_cast<int>(wideMessage.size()), nullptr, 0, nullptr, nullptr);
    if (utf8Size <= 0) {
        return "WinHTTP error " + std::to_string(errorCode) + ".";
    }

    std::string message(static_cast<std::size_t>(utf8Size), '\0');
    WideCharToMultiByte(CP_UTF8, 0, wideMessage.c_str(), static_cast<int>(wideMessage.size()), message.data(), utf8Size, nullptr, nullptr);
    while (!message.empty() && (message.back() == '\r' || message.back() == '\n')) {
        message.pop_back();
    }
    return message;
}

void closeHandle(HINTERNET handle)
{
    if (handle != nullptr) {
        WinHttpCloseHandle(handle);
    }
}

} // namespace

HttpResponse HttpClient::getHttps(const std::string& host, const std::string& pathAndQuery, int timeoutMilliseconds) const
{
    HttpResponse response;

    HINTERNET session = WinHttpOpen(
        L"InvestorCommandCenter/0.3",
        WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
        WINHTTP_NO_PROXY_NAME,
        WINHTTP_NO_PROXY_BYPASS,
        0);
    if (session == nullptr) {
        response.error = windowsErrorMessage(GetLastError());
        return response;
    }

    WinHttpSetTimeouts(session, timeoutMilliseconds, timeoutMilliseconds, timeoutMilliseconds, timeoutMilliseconds);

    const std::wstring wideHost = widenAscii(host);
    HINTERNET connect = WinHttpConnect(session, wideHost.c_str(), INTERNET_DEFAULT_HTTPS_PORT, 0);
    if (connect == nullptr) {
        response.error = windowsErrorMessage(GetLastError());
        closeHandle(session);
        return response;
    }

    const std::wstring widePath = widenAscii(pathAndQuery);
    HINTERNET request = WinHttpOpenRequest(
        connect,
        L"GET",
        widePath.c_str(),
        nullptr,
        WINHTTP_NO_REFERER,
        WINHTTP_DEFAULT_ACCEPT_TYPES,
        WINHTTP_FLAG_SECURE);
    if (request == nullptr) {
        response.error = windowsErrorMessage(GetLastError());
        closeHandle(connect);
        closeHandle(session);
        return response;
    }

    constexpr wchar_t Headers[] =
        L"Accept: application/json\r\n"
        L"User-Agent: Mozilla/5.0 InvestorCommandCenter/0.3\r\n";

    if (!WinHttpSendRequest(request, Headers, static_cast<DWORD>(-1L), WINHTTP_NO_REQUEST_DATA, 0, 0, 0)) {
        response.error = windowsErrorMessage(GetLastError());
        closeHandle(request);
        closeHandle(connect);
        closeHandle(session);
        return response;
    }

    if (!WinHttpReceiveResponse(request, nullptr)) {
        response.error = windowsErrorMessage(GetLastError());
        closeHandle(request);
        closeHandle(connect);
        closeHandle(session);
        return response;
    }

    DWORD statusCode = 0;
    DWORD statusCodeSize = sizeof(statusCode);
    WinHttpQueryHeaders(
        request,
        WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
        WINHTTP_HEADER_NAME_BY_INDEX,
        &statusCode,
        &statusCodeSize,
        WINHTTP_NO_HEADER_INDEX);
    response.statusCode = static_cast<int>(statusCode);

    DWORD bytesAvailable = 0;
    do {
        bytesAvailable = 0;
        if (!WinHttpQueryDataAvailable(request, &bytesAvailable)) {
            response.error = windowsErrorMessage(GetLastError());
            break;
        }

        if (bytesAvailable == 0) {
            break;
        }

        std::string buffer(bytesAvailable, '\0');
        DWORD bytesRead = 0;
        if (!WinHttpReadData(request, buffer.data(), bytesAvailable, &bytesRead)) {
            response.error = windowsErrorMessage(GetLastError());
            break;
        }

        buffer.resize(bytesRead);
        response.body += buffer;
    } while (bytesAvailable > 0);

    closeHandle(request);
    closeHandle(connect);
    closeHandle(session);
    return response;
}
