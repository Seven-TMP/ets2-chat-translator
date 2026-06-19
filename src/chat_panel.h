#pragma once

#include "core_types.h"

#include <functional>
#include <mutex>
#include <queue>
#include <string>
#include <vector>
#include <windows.h>
#include <imm.h>

class ChatPanel
{
public:
    ChatPanel();
    ~ChatPanel();

    bool Open(HINSTANCE instance, const RuntimeConfig& runtime, const std::wstring& windowStatePath = L"");
    void Close();
    void MessageLoop();
    void ApplyRuntime(const RuntimeConfig& runtime);
    bool SetOverlayHotkey(const std::wstring& hotkey);
    void SetCloseButtonExits(bool value) { closeButtonExits_ = value; }

    using ComposeCallback = std::function<void(const std::wstring& text)>;

    unsigned int Push(ChatEntry entry);
    void PatchTranslation(unsigned int id, const std::wstring& text);
    void Status(const std::wstring& text);
    void ToggleVisible();
    HWND Window() const { return hwnd_; }
    bool IsVisible() const { return hwnd_ && IsWindowVisible(hwnd_) != FALSE; }
    void SetComposeCallback(ComposeCallback cb) { composeCallback_ = std::move(cb); }
    void SetComposeStatus(const std::wstring& text);

private:
    static LRESULT CALLBACK WindowProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);

    void Paint(HDC dc, RECT bounds);
    void RenderLayered();
    void LayoutSearchBox(RECT bounds);
    void LayoutComposeBox(RECT bounds);
    void SetSearchText(std::wstring text);
    void SetSearchFocus(bool focused);
    void SetComposeFocus(bool focused);
    bool SearchBoxHit(int x, int y) const;
    bool ComposeBoxHit(int x, int y) const;
    bool HandleSearchKey(UINT msg, WPARAM wp);
    bool HandleComposeKey(UINT msg, WPARAM wp);
    bool EntryMatches(const ChatEntry& entry) const;
    int MatchCountUnlocked() const;
    void UpdateContentWidth(int clientWidth);
    int EntryHeight(HDC dc, const ChatEntry& entry) const;
    void ScrollToEnd();
    int ContentHeight(HDC dc) const;
    int ContentHeightUnlocked(HDC dc) const;
    void ResizeScroll();
    void OnWheel(int delta);
    void OnClick(int x, int y);
    void SaveWindowState() const;

    HWND hwnd_ = nullptr;
    HINSTANCE instance_ = nullptr;
    HFONT font_ = nullptr;
    HFONT smallFont_ = nullptr;
    HFONT titleFont_ = nullptr;

    mutable std::mutex lock_;
    std::vector<ChatEntry> entries_;
    std::queue<ChatEntry> pendingEntries_;
    std::queue<std::pair<unsigned int, std::wstring>> pendingTranslations_;
    unsigned int nextId_ = 1;
    std::wstring status_;
    std::wstring searchText_;
    std::wstring searchDisplayText_;
    std::wstring searchInputText_;

    int topBand_ = 46;
    int statusBand_ = 32;
    int rowH_ = 28;
    int subRowH_ = 24;
    int fontSize_ = 18;
    int overlayOpacity_ = 98;
    int contentWidth_ = 420;
    int scroll_ = 0;
    DWORD uiThreadId_ = 0;
    bool follow_ = true;
    bool closing_ = false;
    bool closeButtonExits_ = false;
    int hotkeyId_ = 0x4554;
    bool hotkeyRegistered_ = false;
    bool searchFocused_ = false;
    bool searchCaretVisible_ = false;
    std::wstring overlayHotkey_ = L"Ctrl+Shift+T";
    std::wstring windowStatePath_;
    RECT searchBoxRect_{};

    ComposeCallback composeCallback_;
    std::wstring composeInputText_;
    std::wstring composeStatus_;
    bool composeFocused_ = false;
    bool composeCaretVisible_ = false;
    RECT composeBoxRect_{};
    int composeCursorPos_ = 0;           // cursor position within composeInputText_
    std::wstring composeImeComp_ = L"";  // IME composition string
    UINT_PTR composeTimerId_ = 0;        // cursor blink timer
    bool composeCaretOn_ = false;        // current blink state

    void StartComposeCaret();
    void StopComposeCaret();
    RECT ComposeCaretRect(HDC dc) const;
    LRESULT HandleImeStartComposition();
    LRESULT HandleImeComposition(WPARAM wp, LPARAM lp);
    LRESULT HandleImeEndComposition();

    void DrainPending();
};
