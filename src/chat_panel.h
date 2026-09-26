#pragma once

#include "core_types.h"
#include "overlay_host.h"

#include <atomic>
#include <functional>
#include <mutex>
#include <queue>
#include <string>
#include <vector>
#include <windows.h>
#include <imm.h>

class ChatPanel : public OverlayHost
{
public:
    ChatPanel();
    ~ChatPanel();

    bool Open(HINSTANCE instance, const RuntimeConfig& runtime, const std::wstring& overlayGeometryPath = L"");
    void Close();
    void MessageLoop();
    void ApplyRuntime(const RuntimeConfig& runtime);
    bool SetOverlayHotkey(const std::wstring& hotkey);
    void SetCloseButtonExits(bool value) { closeButtonExits_ = value; }

    bool OverlayGameSurfaceActive() const override;
    void OverlayUseGameSurface(HWND gameWindow) override;
    void OverlayReleaseGameSurface() override;
    bool OverlayVisible() const override;
    HWND OverlayWindowHandle() const override { return hwnd_; }
    void OverlaySetViewport(int width, int height) override;
    bool OverlayHitTest(int x, int y) const override;
    void OverlayMouseMove(int x, int y) override;
    void OverlayMouseButton(int button, bool down, int x, int y) override;
    void OverlayMouseWheel(int delta, int x, int y) override;
    bool OverlayWantsTextInput() const override;
    bool OverlayOwnsKeyboard() const override;
    void OverlayKeyDown(int virtualKey) override;
    void OverlayCharacter(unsigned int codepoint) override;
    void OverlaySetGameInputLocked(bool locked) override;
    bool OverlayWantsCursor() const override;
    void OverlaySetGameCursorVisible(bool available) override;
    bool OverlayAcquireFrame(OverlayFrame& out) const override;

    using ComposeCallback = std::function<void(const std::wstring& text)>;

    unsigned int Push(ChatEntry entry);
    void PatchTranslation(unsigned int id, const std::wstring& text);
    void Status(const std::wstring& text);
    void ToggleVisible();
    HWND Window() const { return hwnd_; }
    bool IsVisible() const;
    void SetComposeCallback(ComposeCallback cb) { composeCallback_ = std::move(cb); }
    void SetComposeStatus(const std::wstring& text);
    void PostComposeStatus(std::wstring text);

private:
    static LRESULT CALLBACK WindowProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);

    void Paint(HDC dc, RECT bounds);
    void RenderPanel();
    void RequestRender();
    void ReleaseRenderCache();
    void PublishFrame(int width, int height);
    void PublishFramePosition();
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
    void SaveOverlayGeometry() const;

    void ActivateGameSurface(HWND gameWindow);
    void DeactivateGameSurface();
    void ClampGameRect();
    void SetGameVisible(bool visible);
    void SetGameCursorAvailable(bool available);
    void SyncPointerBusy();
    bool PointerUsable() const;
    void HandleOverlayMouseMove(int x, int y);
    void HandleOverlayMouseButton(int button, bool down, int x, int y);
    void EnsureImeHost();
    void ReleaseImeHost();
    void FocusGameWindow();

    HWND hwnd_ = nullptr;
    HINSTANCE instance_ = nullptr;
    HFONT font_ = nullptr;
    HFONT smallFont_ = nullptr;
    HFONT titleFont_ = nullptr;

    HDC cacheDc_ = nullptr;
    HBITMAP cacheBmp_ = nullptr;
    HBITMAP cacheOldBmp_ = nullptr;
    int cacheWidth_ = 0;
    int cacheHeight_ = 0;
    std::atomic<bool> renderPosted_{ false };

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
    std::wstring overlayGeometryPath_;
    RECT searchBoxRect_{};

    std::atomic<bool> gameSurfaceActive_{ false };
    std::atomic<bool> gameVisible_{ true };
    std::atomic<int> viewportWidth_{ 0 };
    std::atomic<int> viewportHeight_{ 0 };
    HWND gameWindow_ = nullptr;
    int gameX_ = 0;
    int gameY_ = 0;
    bool imeHostActive_ = false;
    bool dragActive_ = false;
    bool resizeActive_ = false;
    bool gripHot_ = false;
    bool closeHot_ = false;
    int dragLastX_ = 0;
    int dragLastY_ = 0;

    mutable std::mutex frameLock_;
    std::shared_ptr<const std::vector<std::uint32_t>> framePixels_;
    int frameWidth_ = 0;
    int frameHeight_ = 0;
    int frameX_ = 0;
    int frameY_ = 0;
    std::uint64_t frameRevision_ = 0;

    std::atomic<int> pendingMouseX_{ 0 };
    std::atomic<int> pendingMouseY_{ 0 };
    std::atomic<bool> mouseMovePosted_{ false };
    struct OverlayButtonEvent
    {
        int button = 0;
        bool down = false;
        int x = 0;
        int y = 0;
    };
    mutable std::mutex buttonLock_;
    std::vector<OverlayButtonEvent> pendingButtons_;
    std::atomic<bool> buttonPosted_{ false };
    std::atomic<bool> textInputWanted_{ false };
    std::atomic<bool> pointerBusy_{ false };
    std::atomic<bool> pendingCursorAvailable_{ false };
    std::atomic<bool> cursorStatePosted_{ false };
    std::atomic<bool> gameCursorAvailable_{ false };
    std::atomic<bool> cursorStateKnown_{ false };

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
    void UpdateImeCompositionWindow();
    LRESULT HandleImeStartComposition();
    LRESULT HandleImeComposition(WPARAM wp, LPARAM lp);
    LRESULT HandleImeEndComposition();

    void DrainPending();
};
