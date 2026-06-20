#include "app_runtime.h"

#include "text_codec.h"
#include "win_paths.h"

#include <algorithm>
#include <chrono>
#include <cwctype>

AppRuntime::AppRuntime(HINSTANCE dll, scs_log_t logger, std::wstring gameId, std::wstring gameName)
    : dll_(dll)
    , logger_(logger)
    , gameId_(std::move(gameId))
    , gameName_(std::move(gameName))
{
}

AppRuntime::~AppRuntime()
{
    Stop();
}

bool AppRuntime::Start()
{
    if (alive_) return true;
    alive_ = true;
    ui_ = std::thread(&AppRuntime::UiThread, this);
    return true;
}

void AppRuntime::Stop()
{
    if (!alive_ && !ui_.joinable()) return;
    alive_ = false;
    composeConfirmCv_.notify_all();

    if (tailer_) tailer_->Stop();
    if (translator_) translator_->Stop();
    if (panel_ && panel_->Window()) PostMessageW(panel_->Window(), WM_CLOSE, 0, 0);

    if (composeThread_.joinable()) composeThread_.join();
    if (ui_.joinable()) ui_.join();
}

void AppRuntime::UiThread()
{
    if (!Boot()) {
        Teardown();
        alive_ = false;
        return;
    }

    panel_->MessageLoop();
    Teardown();
    alive_ = false;
}

bool AppRuntime::Boot()
{
    pluginFolder_ = paths::ModuleFolder(dll_);
    configFile_ = pluginFolder_ + L"\\ets2_chat_translator_config.json";
    windowStateFile_ = pluginFolder_ + L"\\ets2_chat_translator_window.json";
    std::wstring lowerGame = gameId_ + L" " + gameName_;
    std::transform(lowerGame.begin(), lowerGame.end(), lowerGame.begin(), [](wchar_t ch) {
        return (wchar_t)towlower(ch);
    });
    bool ats = lowerGame.find(L"ats") != std::wstring::npos
        || lowerGame.find(L"american") != std::wstring::npos;
    logFolder_ = paths::DocumentsFolder() + (ats ? L"\\ATSMP\\logs" : L"\\ETS2MP\\logs");

    if (!paths::ExistsFile(configFile_)) settings::WriteDefaultFile(configFile_);
    settings_ = settings::Load(configFile_);
    configWriteTime_ = ConfigWriteTime();

    panel_ = std::make_unique<ChatPanel>();
    if (!panel_->Open(dll_, settings_.runtime, windowStateFile_)) {
        Log("[ChatTranslator] failed to create panel");
        return false;
    }
    panel_->SetComposeCallback([this](const std::wstring& text) {
        OnComposeSubmit(text);
    });
    Log("[ChatTranslator] panel created");

    bool translationOk = StartTranslator();

    std::wstring status = L"Logs: " + logFolder_;
    if (translationOk) {
        status += L" | providers: " + std::to_wstring(translator_->ProviderCount());
        status += L" | workers: " + std::to_wstring(translator_->WorkerCount());
        Log("[ChatTranslator] translation engine started");
    } else {
        status += L" | translation disabled: " + translator_->LastError();
        LogValue(L"[ChatTranslator] translation disabled: ", translator_->LastError());
    }
    panel_->Status(status);

    tailer_ = std::make_unique<ChatTailer>();
    tailer_->Start(logFolder_, [this](const ChatEntry& entry) {
        AcceptChat(entry);
    });

    Log("[ChatTranslator] runtime started");
    LogValue(L"[ChatTranslator] game: ", gameName_.empty() ? gameId_ : gameName_ + L" (" + gameId_ + L")");
    LogValue(L"[ChatTranslator] log folder: ", logFolder_);
    return true;
}

void AppRuntime::Teardown()
{
    if (composeThread_.joinable()) composeThread_.join();
    {
        std::lock_guard<std::mutex> g(translatorLock_);
        if (translator_) translator_->Stop();
    }
    if (tailer_) tailer_->Stop();
    {
        std::lock_guard<std::mutex> g(translatorLock_);
        translator_.reset();
    }
    tailer_.reset();
    panel_.reset();
}

void AppRuntime::AcceptChat(const ChatEntry& entry)
{
    if (!alive_ || !panel_) return;
    bool confirmedCompose = NoteComposeLogEntry(entry);
    CheckConfigReload();

    ChatEntry displayEntry = entry;
    if (displayEntry.infoLine) {
        panel_->Push(displayEntry);
        return;
    }

    if (displayEntry.serviceLine) {
        panel_->Push(displayEntry);
        LogValue(L"[ChatTranslator] skip service line: ", displayEntry.body);
        return;
    }

    if (!panel_->IsVisible()) {
        displayEntry.translated = displayEntry.body;
        panel_->Push(displayEntry);
        LogValue(L"[ChatTranslator] skip translation while overlay hidden: ", displayEntry.body);
        return;
    }

    if (!TranslateEngine::ShouldTranslate(displayEntry.body)) {
        displayEntry.translated = displayEntry.body;
        panel_->Push(displayEntry);
        LogValue(confirmedCompose ? L"[ChatTranslator] compose confirmed by chat log: " : L"[ChatTranslator] skip non-translatable text: ", displayEntry.body);
        return;
    }

    unsigned int id = panel_->Push(displayEntry);
    LogValue(L"[ChatTranslator] submit translation: ", displayEntry.body);
    std::lock_guard<std::mutex> g(translatorLock_);
    if (translator_ && translator_->ProviderCount() > 0) {
        translator_->Submit(id, displayEntry.body);
    } else {
        panel_->PatchTranslation(id, displayEntry.body);
        Log("[ChatTranslator] skip translation: translator not ready");
    }
}

void AppRuntime::CheckConfigReload()
{
    FILETIME current = ConfigWriteTime();
    if (CompareFileTime(&current, &configWriteTime_) == 0) return;

    configWriteTime_ = current;
    AppSettings reloaded = settings::Load(configFile_);
    settings_ = std::move(reloaded);
    if (panel_) panel_->ApplyRuntime(settings_.runtime);

    Log("[ChatTranslator] config changed, reloading translation engine");
    bool ok = StartTranslator();
    if (panel_) {
        std::wstring status = L"Logs: " + logFolder_;
        if (ok && translator_) {
            status += L" | providers: " + std::to_wstring(translator_->ProviderCount());
            status += L" | workers: " + std::to_wstring(translator_->WorkerCount());
            status += L" | config reloaded";
        } else if (translator_) {
            status += L" | translation disabled: " + translator_->LastError();
        } else {
            status += L" | translation disabled";
        }
        panel_->Status(status);
    }
}

bool AppRuntime::StartTranslator()
{
    auto next = std::make_unique<TranslateEngine>();
    next->SetLogger([this](const std::wstring& line) {
        LogValue(L"", line);
    });

    bool ok = next->Start(settings_.runtime, settings_.providers,
        [this](unsigned int id, const std::wstring& translated) {
            if (alive_ && panel_) panel_->PatchTranslation(id, translated);
        });

    std::lock_guard<std::mutex> g(translatorLock_);
    if (translator_) translator_->Stop();
    translator_ = std::move(next);

    if (ok) {
        Log("[ChatTranslator] translation engine started");
    } else if (translator_) {
        LogValue(L"[ChatTranslator] translation disabled: ", translator_->LastError());
    }
    return ok;
}

FILETIME AppRuntime::ConfigWriteTime() const
{
    WIN32_FILE_ATTRIBUTE_DATA data = {};
    if (GetFileAttributesExW(configFile_.c_str(), GetFileExInfoStandard, &data)) {
        return data.ftLastWriteTime;
    }
    FILETIME empty = {};
    return empty;
}

void AppRuntime::Log(const char* message) const
{
    if (logger_) logger_(SCS_LOG_TYPE_message, message);
}

void AppRuntime::LogValue(const std::wstring& prefix, const std::wstring& value) const
{
    if (!logger_) return;
    std::string msg = text::ToUtf8(prefix + value);
    logger_(SCS_LOG_TYPE_message, msg.c_str());
}

static bool SendTextToGameChat(const std::wstring& text);
static std::wstring NormalizeComposeConfirmationText(const std::wstring& text);

void AppRuntime::OnComposeSubmit(const std::wstring& text)
{
    if (!alive_ || !panel_) return;
    if (composeBusy_.exchange(true)) {
        panel_->SetComposeStatus(L"Previous compose is still running...");
        return;
    }

    LogValue(L"[ChatTranslator] compose input: ", text);
    panel_->SetComposeStatus(L"Translating and preparing game chat...");

    if (composeThread_.joinable()) composeThread_.join();
    composeThread_ = std::thread([this, text]() {
        std::wstring translated = text;
        {
            std::lock_guard<std::mutex> g(translatorLock_);
            if (translator_ && translator_->ProviderCount() > 0) {
                translated = translator_->TranslateCompose(text);
            }
        }
        FinishComposeSend(text, translated);
        composeBusy_ = false;
    });
}

void AppRuntime::FinishComposeSend(const std::wstring& original, const std::wstring& translated)
{
    if (!alive_ || !panel_) return;

    std::wstring originalTrimmed = text::Trim(original);
    std::wstring translatedTrimmed = text::Trim(translated);
    if (translatedTrimmed.empty() || translatedTrimmed == originalTrimmed || text::MostlyChinese(translatedTrimmed)) {
        LogValue(L"[ChatTranslator] compose translation failed, not sending: ", originalTrimmed);
        panel_->PostComposeStatus(L"Translation failed; not sent.");
        return;
    }

    if (panel_->IsVisible()) {
        ShowWindow(panel_->Window(), SW_HIDE);
    }

    ArmComposeConfirmation(translatedTrimmed);
    bool ok = SendTextToGameChat(translatedTrimmed);
    bool confirmed = ok && WaitForComposeConfirmation(2500);
    ClearComposeConfirmation();

    if (panel_->Window() && !panel_->IsVisible()) {
        ShowWindow(panel_->Window(), SW_SHOWNA);
        SetWindowPos(panel_->Window(), HWND_TOPMOST, 0, 0, 0, 0,
            SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE | SWP_SHOWWINDOW);
    }

    if (!ok) {
        LogValue(L"[ChatTranslator] compose send failed: ", translatedTrimmed);
        panel_->PostComposeStatus(L"Send failed: game window or clipboard unavailable.");
    } else if (confirmed) {
        LogValue(L"[ChatTranslator] compose send confirmed: ", translatedTrimmed);
        panel_->PostComposeStatus(L"Sent and confirmed by chat log.");
    } else {
        LogValue(L"[ChatTranslator] compose sent but not confirmed by chat log: ", translatedTrimmed);
        panel_->PostComposeStatus(L"Sent, waiting for chat log confirmation timed out.");
    }
}

void AppRuntime::ArmComposeConfirmation(const std::wstring& text)
{
    std::lock_guard<std::mutex> g(composeConfirmLock_);
    pendingComposeText_ = NormalizeComposeConfirmationText(text);
    pendingComposeActive_ = !pendingComposeText_.empty();
    pendingComposeConfirmed_ = false;
}

void AppRuntime::ClearComposeConfirmation()
{
    std::lock_guard<std::mutex> g(composeConfirmLock_);
    pendingComposeText_.clear();
    pendingComposeActive_ = false;
    pendingComposeConfirmed_ = false;
}

static std::wstring NormalizeComposeConfirmationText(const std::wstring& text)
{
    std::wstring trimmed = text::Trim(text);
    std::wstring out;
    out.reserve(trimmed.size());
    bool spacing = false;
    for (wchar_t ch : trimmed) {
        if (iswspace(ch)) {
            spacing = !out.empty();
            continue;
        }
        if (spacing) {
            out.push_back(L' ');
            spacing = false;
        }
        out.push_back(ch);
    }
    return out;
}

bool AppRuntime::NoteComposeLogEntry(const ChatEntry& entry)
{
    if (entry.infoLine || entry.serviceLine) return false;
    std::wstring body = NormalizeComposeConfirmationText(entry.body);
    if (body.empty()) return false;

    std::lock_guard<std::mutex> g(composeConfirmLock_);
    if (!pendingComposeActive_ || pendingComposeConfirmed_) return false;
    if (body != pendingComposeText_) return false;

    pendingComposeConfirmed_ = true;
    composeConfirmCv_.notify_all();
    return true;
}

bool AppRuntime::WaitForComposeConfirmation(DWORD timeoutMs)
{
    std::unique_lock<std::mutex> g(composeConfirmLock_);
    if (!pendingComposeActive_) return false;
    return composeConfirmCv_.wait_for(g, std::chrono::milliseconds(timeoutMs), [this] {
        return pendingComposeConfirmed_ || !pendingComposeActive_ || !alive_;
    }) && pendingComposeConfirmed_;
}

static HWND FindGameWindow()
{
    struct Ctx { DWORD pid; HWND result; };
    Ctx ctx{ GetCurrentProcessId(), nullptr };

    EnumWindows([](HWND hwnd, LPARAM lParam) -> BOOL {
        auto& ctx = *reinterpret_cast<Ctx*>(lParam);
        DWORD wp = 0;
        GetWindowThreadProcessId(hwnd, &wp);
        if (wp != ctx.pid) return TRUE;
        if (!IsWindowVisible(hwnd)) return TRUE;
        if (GetWindow(hwnd, GW_OWNER) != nullptr) return TRUE;
        ctx.result = hwnd;
        return FALSE;
    }, reinterpret_cast<LPARAM>(&ctx));

    return ctx.result;
}

static void PressKey(WORD vk, DWORD holdMs = 25)
{
    INPUT down = {};
    down.type = INPUT_KEYBOARD;
    down.ki.wVk = vk;
    SendInput(1, &down, sizeof(INPUT));
    Sleep(holdMs);

    INPUT up = {};
    up.type = INPUT_KEYBOARD;
    up.ki.wVk = vk;
    up.ki.dwFlags = KEYEVENTF_KEYUP;
    SendInput(1, &up, sizeof(INPUT));
}

static void KeyDown(WORD vk)
{
    INPUT input = {};
    input.type = INPUT_KEYBOARD;
    input.ki.wVk = vk;
    SendInput(1, &input, sizeof(INPUT));
}

static void KeyUp(WORD vk)
{
    INPUT input = {};
    input.type = INPUT_KEYBOARD;
    input.ki.wVk = vk;
    input.ki.dwFlags = KEYEVENTF_KEYUP;
    SendInput(1, &input, sizeof(INPUT));
}

static bool ActivateGameWindow(HWND gameWnd)
{
    if (!gameWnd) return false;

    if (IsIconic(gameWnd)) ShowWindow(gameWnd, SW_RESTORE);
    else ShowWindow(gameWnd, SW_SHOW);

    DWORD currentThread = GetCurrentThreadId();
    DWORD gameThread = GetWindowThreadProcessId(gameWnd, nullptr);
    DWORD foregroundThread = GetWindowThreadProcessId(GetForegroundWindow(), nullptr);

    if (foregroundThread && foregroundThread != currentThread) {
        AttachThreadInput(foregroundThread, currentThread, TRUE);
    }
    if (gameThread && gameThread != currentThread) {
        AttachThreadInput(gameThread, currentThread, TRUE);
    }

    BringWindowToTop(gameWnd);
    SetForegroundWindow(gameWnd);
    SetFocus(gameWnd);
    SetActiveWindow(gameWnd);

    Sleep(35);
    if (GetForegroundWindow() != gameWnd) {
        SetForegroundWindow(gameWnd);
        BringWindowToTop(gameWnd);
    }

    if (gameThread && gameThread != currentThread) {
        AttachThreadInput(gameThread, currentThread, FALSE);
    }
    if (foregroundThread && foregroundThread != currentThread) {
        AttachThreadInput(foregroundThread, currentThread, FALSE);
    }

    Sleep(55);
    return GetForegroundWindow() == gameWnd;
}

static void RestoreClipboardText(const std::wstring& previousText, bool hadText)
{
    if (!OpenClipboard(nullptr)) return;
    EmptyClipboard();
    if (hadText) {
        size_t cb = (previousText.size() + 1) * sizeof(wchar_t);
        HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE, cb);
        if (hMem) {
            wchar_t* p = static_cast<wchar_t*>(GlobalLock(hMem));
            if (p) {
                memcpy(p, previousText.c_str(), cb);
                GlobalUnlock(hMem);
                if (!SetClipboardData(CF_UNICODETEXT, hMem)) GlobalFree(hMem);
            } else {
                GlobalFree(hMem);
            }
        }
    }
    CloseClipboard();
}

static bool SendTextToGameChat(const std::wstring& text)
{
    if (text.empty()) return false;
    HWND gameWnd = FindGameWindow();
    if (!gameWnd) return false;

    std::wstring previousText;
    bool hadText = false;
    bool ok = false;

    if (OpenClipboard(nullptr)) {
        HANDLE old = GetClipboardData(CF_UNICODETEXT);
        if (old) {
            const wchar_t* oldText = static_cast<const wchar_t*>(GlobalLock(old));
            if (oldText) {
                previousText = oldText;
                hadText = true;
                GlobalUnlock(old);
            }
        }

        if (EmptyClipboard()) {
            size_t cb = (text.size() + 1) * sizeof(wchar_t);
            HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE, cb);
            if (hMem) {
                wchar_t* p = static_cast<wchar_t*>(GlobalLock(hMem));
                if (p) {
                    memcpy(p, text.c_str(), cb);
                    GlobalUnlock(hMem);
                    ok = SetClipboardData(CF_UNICODETEXT, hMem) != nullptr;
                }
                if (!ok) GlobalFree(hMem);
            }
        }
        CloseClipboard();
    }
    if (!ok) {
        RestoreClipboardText(previousText, hadText);
        return false;
    }

    Sleep(60);
    if (!ActivateGameWindow(gameWnd)) {
        RestoreClipboardText(previousText, hadText);
        return false;
    }

    PressKey('Y', 18);
    Sleep(70);

    KeyDown(VK_CONTROL);
    Sleep(10);
    PressKey('V', 18);
    Sleep(12);
    KeyUp(VK_CONTROL);
    Sleep(70);

    PressKey(VK_RETURN, 18);
    Sleep(30);

    RestoreClipboardText(previousText, hadText);
    return true;
}
