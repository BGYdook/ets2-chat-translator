#include "chat_tailer.h"
#include "text_codec.h"

#include <sstream>

namespace
{
constexpr DWORD kMissingLogPollMs = 500;
constexpr DWORD kActiveLogPollMs = 80;
}

ChatTailer::ChatTailer()
{
    wake_ = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    offset_.QuadPart = -1;
}

ChatTailer::~ChatTailer()
{
    Stop();
    if (wake_) CloseHandle(wake_);
}

void ChatTailer::Start(std::wstring logFolder, Sink sink)
{
    if (live_) return;
    folder_ = std::move(logFolder);
    sink_ = std::move(sink);
    offset_.QuadPart = -1;
    activeFile_.clear();
    pendingText_.clear();
    if (wake_) ResetEvent(wake_);
    live_ = true;
    worker_ = std::thread(&ChatTailer::Run, this);
}

void ChatTailer::Stop()
{
    live_ = false;
    if (wake_) SetEvent(wake_);
    if (worker_.joinable()) worker_.join();
}

std::wstring ChatTailer::TodayFile() const
{
    SYSTEMTIME now{};
    GetLocalTime(&now);
    wchar_t name[80] = {};
    swprintf_s(name, L"chat_%04u_%02u_%02u_log.txt", now.wYear, now.wMonth, now.wDay);
    return folder_ + L"\\" + name;
}

void ChatTailer::Run()
{
    while (live_) {
        std::wstring file = TodayFile();
        if (file != activeFile_) {
            activeFile_ = file;
            offset_.QuadPart = -1; // 标记：需要跳到文件末尾
            pendingText_.clear();
        }

        HANDLE h = CreateFileW(file.c_str(), GENERIC_READ,
            FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
            nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);

        if (h == INVALID_HANDLE_VALUE) {
            if (wake_ && WaitForSingleObject(wake_, kMissingLogPollMs) == WAIT_OBJECT_0) break;
            continue;
        }

        LARGE_INTEGER size{};
        if (GetFileSizeEx(h, &size)) {
            // 首次打开：跳到文件末尾，不读历史内容
            if (offset_.QuadPart < 0) {
                offset_.QuadPart = size.QuadPart;
            }

            if (size.QuadPart > offset_.QuadPart) {
                LARGE_INTEGER seek = offset_;
                SetFilePointerEx(h, seek, nullptr, FILE_BEGIN);

                LONGLONG want = size.QuadPart - offset_.QuadPart;
                if (want > 1024 * 1024) want = 1024 * 1024;

                std::string bytes((size_t)want, '\0');
                DWORD got = 0;
                if (ReadFile(h, bytes.data(), (DWORD)want, &got, nullptr) && got > 0) {
                    bytes.resize(got);
                    EmitLines(text::DecodeLogBytes(bytes));
                    offset_.QuadPart += got;
                }
            }
        }

        CloseHandle(h);
        if (wake_ && WaitForSingleObject(wake_, kActiveLogPollMs) == WAIT_OBJECT_0) break;
    }
}

void ChatTailer::EmitLines(const std::wstring& textBlock)
{
    pendingText_ += textBlock;
    size_t start = 0;
    while (true) {
        size_t end = pendingText_.find(L'\n', start);
        if (end == std::wstring::npos) break;
        std::wstring line = pendingText_.substr(start, end - start);
        if (!line.empty() && line.back() == L'\r') line.pop_back();
        line = text::Trim(line);
        if (!line.empty() && sink_) sink_(Parse(line));
        start = end + 1;
    }
    pendingText_.erase(0, start);
}

ChatEntry ChatTailer::Parse(std::wstring line) const
{
    ChatEntry e;
    size_t pos = 0;

    if (!line.empty() && line[0] == L'[') {
        size_t end = line.find(L']', 1);
        if (end != std::wstring::npos) {
            e.channel = line.substr(1, end - 1);
            pos = end + 1;
        }
    } else if (line.rfind(L"VTC>", 0) == 0) {
        e.channel = L"VTC";
        pos = 4;
    }

    while (pos < line.size() && line[pos] == L' ') ++pos;
    if (pos < line.size() && line[pos] == L'[') {
        size_t end = line.find(L']', pos + 1);
        if (end != std::wstring::npos) {
            e.time = line.substr(pos + 1, end - pos - 1);
            pos = end + 1;
        }
    }

    while (pos < line.size() && line[pos] == L' ') ++pos;
    std::wstring rest = line.substr(pos);

    if (rest.rfind(L"[System]", 0) == 0 ||
        rest.rfind(L"[Job Tracker]", 0) == 0 ||
        rest.rfind(L"Connecting", 0) == 0 ||
        rest.rfind(L"Connection", 0) == 0) {
        e.serviceLine = true;
        e.body = rest;
        return e;
    }

    size_t cut = rest.find(L": ");
    if (cut != std::wstring::npos) {
        e.author = rest.substr(0, cut);
        e.body = rest.substr(cut + 2);
    } else {
        e.body = rest;
    }
    return e;
}
