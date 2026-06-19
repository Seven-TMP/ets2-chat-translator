#pragma once

#include "../include/scs_telemetry.h"
#include "chat_panel.h"
#include "chat_tailer.h"
#include "settings_store.h"
#include "translate_engine.h"

#include <atomic>
#include <mutex>
#include <memory>
#include <thread>
#include <windows.h>

class AppRuntime
{
public:
    AppRuntime(HINSTANCE dll, scs_log_t logger, std::wstring gameId = L"", std::wstring gameName = L"");
    ~AppRuntime();

    bool Start();
    void Stop();

private:
    void UiThread();
    bool Boot();
    void Teardown();
    void AcceptChat(const ChatEntry& entry);
    void CheckConfigReload();
    bool StartTranslator();
    FILETIME ConfigWriteTime() const;
    void Log(const char* message) const;
    void LogValue(const std::wstring& prefix, const std::wstring& value) const;
    void OnComposeSubmit(const std::wstring& text);

    HINSTANCE dll_ = nullptr;
    scs_log_t logger_ = nullptr;
    std::wstring gameId_;
    std::wstring gameName_;
    std::atomic<bool> alive_{ false };
    std::thread ui_;

    std::unique_ptr<ChatPanel> panel_;
    std::unique_ptr<ChatTailer> tailer_;
    std::unique_ptr<TranslateEngine> translator_;

    AppSettings settings_;
    std::wstring pluginFolder_;
    std::wstring configFile_;
    std::wstring windowStateFile_;
    std::wstring logFolder_;
    FILETIME configWriteTime_{};
    std::mutex translatorLock_;
};
