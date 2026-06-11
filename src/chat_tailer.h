#pragma once

#include "core_types.h"

#include <atomic>
#include <functional>
#include <string>
#include <thread>
#include <windows.h>

class ChatTailer
{
public:
    using Sink = std::function<void(const ChatEntry&)>;

    ChatTailer();
    ~ChatTailer();

    void Start(std::wstring logFolder, Sink sink);
    void Stop();

private:
    void Run();
    std::wstring TodayFile() const;
    ChatEntry Parse(std::wstring line) const;
    void EmitLines(const std::wstring& text);

    std::wstring folder_;
    std::wstring activeFile_;
    LARGE_INTEGER offset_{};
    Sink sink_;
    std::thread worker_;
    std::atomic<bool> live_{ false };
    HANDLE wake_ = nullptr;
    std::wstring pendingText_;
};
