#include "chat_panel.h"

#include <chrono>
#include <thread>
#include <windows.h>

namespace
{
ChatEntry Entry(const wchar_t* time, const wchar_t* author, const wchar_t* body, const wchar_t* translated)
{
    ChatEntry e;
    e.time = time;
    e.author = author;
    e.body = body;
    e.translated = translated;
    return e;
}

ChatEntry Service(const wchar_t* time, const wchar_t* body)
{
    ChatEntry e;
    e.time = time;
    e.body = body;
    e.translated = body;
    e.serviceLine = true;
    return e;
}

ChatEntry Info(const wchar_t* time, const wchar_t* body)
{
    ChatEntry e;
    e.time = time;
    e.body = body;
    e.translated = body;
    e.infoLine = true;
    e.searchOnly = true;
    return e;
}

DWORD WINAPI FeedThread(void* param)
{
    auto* panel = static_cast<ChatPanel*>(param);
    std::this_thread::sleep_for(std::chrono::milliseconds(180));
    panel->Status(L"预览模式：可直接点击搜索框输入，拖动右下角调整窗口大小");

    panel->Push(Service(L"14:34:10", L"[System] Player TAHA_27 (700) has been banned."));
    panel->Push(Entry(L"14:34:11", L"YGMK (729)", L"不要变卖", L"不要变卖"));
    panel->Push(Entry(L"14:34:12", L"Defne (274)", L"sry broooo", L"抱歉，兄弟"));
    panel->Push(Entry(L"14:34:13", L"User_6116755 (509)", L"？？？？、", L"？？？？、"));
    panel->Push(Entry(L"14:34:20", L"Prime Logistics Averka", L"ty mate", L"谢谢，伙计"));
    panel->Push(Info(L"14:34:39", L"日志信息：User_6116755  临时编号 509  TMPID 6116755  SteamID64 76561198757795883"));
    panel->Push(Info(L"14:35:31", L"日志信息：Turho3161  临时编号 457  TMPID 5094215  SteamID64 76561199326957802  Tag SOL_SERT REZERV"));
    return 0;
}
}

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int)
{
    RuntimeConfig runtime;
    runtime.fontSize = 16;
    runtime.overlayOpacity = 98;

    ChatPanel panel;
    if (!panel.Open(instance, runtime, L"")) return 1;
    panel.SetCloseButtonExits(true);

    HANDLE feeder = CreateThread(nullptr, 0, FeedThread, &panel, 0, nullptr);
    if (feeder) CloseHandle(feeder);

    panel.MessageLoop();
    return 0;
}
