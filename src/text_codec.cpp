#include "text_codec.h"

#include <windows.h>
#include <algorithm>
#include <cwctype>

namespace text
{
std::string ToUtf8(const std::wstring& value)
{
    if (value.empty()) return "";
    int needed = WideCharToMultiByte(CP_UTF8, 0, value.data(), (int)value.size(), nullptr, 0, nullptr, nullptr);
    if (needed <= 0) return "";
    std::string out(needed, '\0');
    WideCharToMultiByte(CP_UTF8, 0, value.data(), (int)value.size(), out.data(), needed, nullptr, nullptr);
    return out;
}

std::wstring FromUtf8(const std::string& value)
{
    if (value.empty()) return L"";
    int needed = MultiByteToWideChar(CP_UTF8, 0, value.data(), (int)value.size(), nullptr, 0);
    if (needed <= 0) return L"";
    std::wstring out(needed, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, value.data(), (int)value.size(), out.data(), needed);
    return out;
}

std::wstring DecodeLogBytes(const std::string& bytes)
{
    if (bytes.empty()) return L"";

    const char* data = bytes.data();
    int len = (int)bytes.size();
    if (len >= 3 && (unsigned char)data[0] == 0xEF && (unsigned char)data[1] == 0xBB && (unsigned char)data[2] == 0xBF) {
        data += 3;
        len -= 3;
    }

    int needed = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, data, len, nullptr, 0);
    if (needed > 0) {
        std::wstring out(needed, L'\0');
        MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, data, len, out.data(), needed);
        return out;
    }

    needed = MultiByteToWideChar(CP_ACP, 0, data, len, nullptr, 0);
    if (needed <= 0) return L"";
    std::wstring out(needed, L'\0');
    MultiByteToWideChar(CP_ACP, 0, data, len, out.data(), needed);
    return out;
}

std::wstring PercentEncode(const std::wstring& value)
{
    std::string utf8 = ToUtf8(value);
    std::wstring out;
    out.reserve(utf8.size() * 3);

    for (unsigned char ch : utf8) {
        if (isalnum(ch) || ch == '-' || ch == '_' || ch == '.' || ch == '~') {
            out.push_back((wchar_t)ch);
        } else if (ch == ' ') {
            out.push_back(L'+');
        } else {
            wchar_t buf[4] = {};
            swprintf_s(buf, L"%%%02X", ch);
            out += buf;
        }
    }
    return out;
}

std::string EscapeJson(const std::wstring& value)
{
    std::string src = ToUtf8(value);
    std::string out;
    out.reserve(src.size() + 16);
    for (unsigned char ch : src) {
        switch (ch) {
        case '"': out += "\\\""; break;
        case '\\': out += "\\\\"; break;
        case '\n': out += "\\n"; break;
        case '\r': out += "\\r"; break;
        case '\t': out += "\\t"; break;
        default:
            if (ch < 0x20) {
                char buf[7] = {};
                sprintf_s(buf, "\\u%04X", ch);
                out += buf;
            } else {
                out.push_back((char)ch);
            }
            break;
        }
    }
    return out;
}

static void AppendCodepoint(std::string& out, unsigned int cp)
{
    if (cp <= 0x7F) {
        out.push_back((char)cp);
    } else if (cp <= 0x7FF) {
        out.push_back((char)(0xC0 | (cp >> 6)));
        out.push_back((char)(0x80 | (cp & 0x3F)));
    } else if (cp <= 0xFFFF) {
        out.push_back((char)(0xE0 | (cp >> 12)));
        out.push_back((char)(0x80 | ((cp >> 6) & 0x3F)));
        out.push_back((char)(0x80 | (cp & 0x3F)));
    } else {
        out.push_back((char)(0xF0 | (cp >> 18)));
        out.push_back((char)(0x80 | ((cp >> 12) & 0x3F)));
        out.push_back((char)(0x80 | ((cp >> 6) & 0x3F)));
        out.push_back((char)(0x80 | (cp & 0x3F)));
    }
}

std::wstring JsonString(const std::string& json, const std::string& key)
{
    std::string needle = "\"" + key + "\"";
    size_t searchFrom = 0;
    while (true) {
        size_t p = json.find(needle, searchFrom);
        if (p == std::string::npos) return L"";
        searchFrom = p + needle.size();
        p = json.find(':', searchFrom);
        if (p == std::string::npos) return L"";
        ++p;
        while (p < json.size() && (unsigned char)json[p] <= ' ') ++p;
        if (p >= json.size()) return L"";
        if (json[p] != '"') continue;
        ++p;

        std::string raw;
        bool closed = false;
        while (p < json.size()) {
            char ch = json[p++];
            if (ch == '"') {
                closed = true;
                break;
            }
            if (ch != '\\' || p >= json.size()) {
                raw.push_back(ch);
                continue;
            }
            char esc = json[p++];
            switch (esc) {
            case '"': raw.push_back('"'); break;
            case '\\': raw.push_back('\\'); break;
            case '/': raw.push_back('/'); break;
            case 'b': raw.push_back('\b'); break;
            case 'f': raw.push_back('\f'); break;
            case 'n': raw.push_back('\n'); break;
            case 'r': raw.push_back('\r'); break;
            case 't': raw.push_back('\t'); break;
            case 'u':
                if (p + 4 <= json.size()) {
                    unsigned int cp = 0;
                    try { cp = std::stoul(json.substr(p, 4), nullptr, 16); } catch (...) { cp = 0; }
                    p += 4;
                    if (cp >= 0xD800 && cp <= 0xDBFF && p + 6 <= json.size()
                        && json[p] == '\\' && json[p + 1] == 'u') {
                        unsigned int low = 0;
                        try { low = std::stoul(json.substr(p + 2, 4), nullptr, 16); } catch (...) { low = 0; }
                        if (low >= 0xDC00 && low <= 0xDFFF) {
                            cp = 0x10000 + ((cp - 0xD800) << 10) + (low - 0xDC00);
                            p += 6;
                        }
                    }
                    AppendCodepoint(raw, cp);
                }
                break;
            default:
                raw.push_back(esc);
                break;
            }
        }
        if (!closed) return L"";
        return FromUtf8(raw);
    }
}

std::wstring Trim(std::wstring value)
{
    while (!value.empty() && iswspace(value.front())) value.erase(value.begin());
    while (!value.empty() && iswspace(value.back())) value.pop_back();
    return value;
}

static bool IsChinese(wchar_t ch)
{
    return (ch >= 0x4E00 && ch <= 0x9FFF)
        || (ch >= 0x3400 && ch <= 0x4DBF)
        || (ch >= 0x3000 && ch <= 0x303F)
        || (ch >= 0xFF00 && ch <= 0xFFEF);
}

bool MostlyChinese(const std::wstring& value)
{
    int letters = 0;
    int chinese = 0;
    for (wchar_t ch : value) {
        if (IsChinese(ch)) {
            ++letters;
            ++chinese;
        } else if (iswalpha(ch)) {
            ++letters;
        }
    }
    if (letters == 0) return true;
    return ((float)chinese / (float)letters) >= 0.7f;
}
}
