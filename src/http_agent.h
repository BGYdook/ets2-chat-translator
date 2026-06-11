#pragma once

#include <string>
#include <vector>
#include <windows.h>
#include <winhttp.h>

struct HeaderPair
{
    std::wstring name;
    std::wstring value;
};

struct NetReply
{
    DWORD status = 0;
    std::string payload;
    std::wstring error;
};

class HttpAgent
{
public:
    explicit HttpAgent(int timeoutMs);
    ~HttpAgent();

    NetReply Get(const std::wstring& host, INTERNET_PORT port, const std::wstring& target, bool tls,
                 const std::vector<HeaderPair>& headers = {});
    NetReply Post(const std::wstring& host, INTERNET_PORT port, const std::wstring& target, bool tls,
                  const std::string& body, const std::vector<HeaderPair>& headers = {});

private:
    NetReply Send(const wchar_t* verb, const std::wstring& host, INTERNET_PORT port, const std::wstring& target,
                  bool tls, const std::string& body, const std::vector<HeaderPair>& headers);

    HINTERNET session_ = nullptr;
    int timeoutMs_ = 10000;
};
