#include "http_agent.h"

#include <algorithm>

HttpAgent::HttpAgent(int timeoutMs)
    : timeoutMs_((std::max)(1500, (std::min)(30000, timeoutMs)))
{
    session_ = WinHttpOpen(L"ETS2ChatTranslator/rewrite-20260611", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
        WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
}

HttpAgent::~HttpAgent()
{
    for (auto& conn : connections_) {
        if (conn.handle) WinHttpCloseHandle(conn.handle);
    }
    connections_.clear();
    if (session_) WinHttpCloseHandle(session_);
}

HINTERNET HttpAgent::ConnectionFor(const std::wstring& host, INTERNET_PORT port)
{
    for (const auto& conn : connections_) {
        if (conn.host == host && conn.port == port) return conn.handle;
    }

    HINTERNET handle = WinHttpConnect(session_, host.c_str(), port, 0);
    if (!handle) return nullptr;
    connections_.push_back({ host, port, handle });
    return handle;
}

NetReply HttpAgent::Get(const std::wstring& host, INTERNET_PORT port, const std::wstring& target, bool tls,
                        const std::vector<HeaderPair>& headers)
{
    return Send(L"GET", host, port, target, tls, "", headers);
}

NetReply HttpAgent::Post(const std::wstring& host, INTERNET_PORT port, const std::wstring& target, bool tls,
                         const std::string& body, const std::vector<HeaderPair>& headers)
{
    return Send(L"POST", host, port, target, tls, body, headers);
}

NetReply HttpAgent::Send(const wchar_t* verb, const std::wstring& host, INTERNET_PORT port,
                         const std::wstring& target, bool tls, const std::string& body,
                         const std::vector<HeaderPair>& headers)
{
    NetReply reply;
    if (!session_) {
        reply.error = L"network session unavailable";
        return reply;
    }

    HINTERNET conn = ConnectionFor(host, port);
    if (!conn) {
        reply.error = L"connect failed";
        return reply;
    }

    HINTERNET req = WinHttpOpenRequest(conn, verb, target.c_str(), nullptr,
        WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, tls ? WINHTTP_FLAG_SECURE : 0);
    if (!req) {
        reply.error = L"request creation failed";
        return reply;
    }

    int connectTimeout = (std::min)(5000, timeoutMs_);
    int sendTimeout = (std::min)(5000, timeoutMs_);
    WinHttpSetTimeouts(req, 800, connectTimeout, sendTimeout, timeoutMs_);

    std::wstring rawHeaders;
    for (const auto& h : headers) rawHeaders += h.name + L": " + h.value + L"\r\n";

    void* data = body.empty() ? WINHTTP_NO_REQUEST_DATA : (void*)body.data();
    DWORD dataSize = (DWORD)body.size();
    BOOL ok = WinHttpSendRequest(req,
        rawHeaders.empty() ? WINHTTP_NO_ADDITIONAL_HEADERS : rawHeaders.c_str(),
        rawHeaders.empty() ? 0 : (DWORD)-1L,
        data, dataSize, dataSize, 0);
    if (!ok || !WinHttpReceiveResponse(req, nullptr)) {
        reply.error = L"request failed";
        WinHttpCloseHandle(req);
        return reply;
    }

    DWORD statusBytes = sizeof(reply.status);
    WinHttpQueryHeaders(req, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
        WINHTTP_HEADER_NAME_BY_INDEX, &reply.status, &statusBytes, WINHTTP_NO_HEADER_INDEX);

    char chunk[4096];
    DWORD got = 0;
    while (WinHttpReadData(req, chunk, sizeof(chunk), &got) && got > 0) {
        reply.payload.append(chunk, got);
        got = 0;
    }

    if (reply.status < 200 || reply.status >= 300) {
        wchar_t buf[64] = {};
        swprintf_s(buf, L"HTTP %lu", reply.status);
        reply.error = buf;
    }

    WinHttpCloseHandle(req);
    return reply;
}
