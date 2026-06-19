#include "app_runtime.h"

#include "text_codec.h"
#include "win_paths.h"

#include <algorithm>
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

    if (tailer_) tailer_->Stop();
    if (translator_) translator_->Stop();
    if (panel_ && panel_->Window()) PostMessageW(panel_->Window(), WM_CLOSE, 0, 0);

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
        LogValue(L"[ChatTranslator] skip non-translatable text: ", displayEntry.body);
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

static void SendKeysToGame(const std::wstring& text);

void AppRuntime::OnComposeSubmit(const std::wstring& text)
{
    if (!alive_ || !panel_) return;
    LogValue(L"[ChatTranslator] compose input: ", text);

    // Hide overlay so the game becomes the foreground window.
    // This is essential: fullscreen DirectX games won't receive
    // SendInput unless they are in foreground.
    if (panel_->IsVisible()) {
        ShowWindow(panel_->Window(), SW_HIDE);
    }

    std::lock_guard<std::mutex> g(translatorLock_);
    if (translator_ && translator_->ProviderCount() > 0) {
        auto panel = panel_.get();
        translator_->SubmitCompose(text, [panel](const std::wstring& translated) {
            // Overlay is hidden, game has focus — send keys
            SendKeysToGame(translated);
            // Restore overlay
            if (panel->Window() && !panel->IsVisible()) {
                ShowWindow(panel->Window(), SW_SHOWNA);
                SetWindowPos(panel->Window(), HWND_TOPMOST, 0, 0, 0, 0,
                    SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE | SWP_SHOWWINDOW);
            }
            panel->SetComposeStatus(L"");
        });
    } else {
        SendKeysToGame(text);
        if (panel_->Window() && !panel_->IsVisible()) {
            ShowWindow(panel_->Window(), SW_SHOWNA);
            SetWindowPos(panel_->Window(), HWND_TOPMOST, 0, 0, 0, 0,
                SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE | SWP_SHOWWINDOW);
        }
        panel_->SetComposeStatus(L"");
    }
}

// Find the game's main window by enumerating top-level windows in this process.
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

static void SendKeysToGame(const std::wstring& text)
{
    if (text.empty()) return;

    // Copy text to clipboard
    bool ok = false;
    if (OpenClipboard(nullptr)) {
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
    if (!ok) return;

    // Wait for game to process the hidden overlay
    Sleep(250);

    // Explicitly bring the game window to foreground.
    // SendInput only works for DirectX fullscreen games when
    // the target window is the foreground window.
    HWND gameWnd = FindGameWindow();
    if (gameWnd) {
        SetForegroundWindow(gameWnd);
        Sleep(50);
    }

    auto keyDown = [](WORD vk) {
        INPUT input = {};
        input.type = INPUT_KEYBOARD;
        input.ki.wVk = vk;
        SendInput(1, &input, sizeof(INPUT));
    };

    auto keyUp = [](WORD vk) {
        INPUT input = {};
        input.type = INPUT_KEYBOARD;
        input.ki.wVk = vk;
        input.ki.dwFlags = KEYEVENTF_KEYUP;
        SendInput(1, &input, sizeof(INPUT));
    };
    auto press = [&](WORD vk) { keyDown(vk); Sleep(25); keyUp(vk); };

    // Y to open chat
    press('Y');
    Sleep(120);

    // Ctrl+V to paste
    keyDown(VK_CONTROL);
    Sleep(25);
    press('V');
    Sleep(25);
    keyUp(VK_CONTROL);
    Sleep(80);

    // Enter to send
    press(VK_RETURN);
}