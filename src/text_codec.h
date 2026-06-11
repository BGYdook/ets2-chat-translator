#pragma once

#include <string>

namespace text
{
    std::string ToUtf8(const std::wstring& value);
    std::wstring FromUtf8(const std::string& value);
    std::wstring DecodeLogBytes(const std::string& bytes);
    std::wstring PercentEncode(const std::wstring& value);
    std::string EscapeJson(const std::wstring& value);
    std::wstring JsonString(const std::string& json, const std::string& key);
    std::wstring Trim(std::wstring value);
    bool MostlyChinese(const std::wstring& value);
}
