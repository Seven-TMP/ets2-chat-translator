#pragma once

#include <TruckersMP/TruckersMP.hxx>

#include <windows.h>

#include <atomic>
#include <cstdint>
#include <memory>
#include <string>

class D3dOverlayRenderer;

class SdkOverlay
{
public:
    using PostRenderEvent = TruckersMP::Event<>;
    using KeyEvent = TruckersMP::Event<TruckersMP::InputKeyEvent&>;
    using CharEvent = TruckersMP::Event<TruckersMP::InputCharEvent&>;
    using MouseMoveEvent = TruckersMP::Event<TruckersMP::InputMouseMoveEvent&>;
    using MouseWheelEvent = TruckersMP::Event<TruckersMP::InputMouseWheelEvent&>;
    using MouseButtonEvent = TruckersMP::Event<TruckersMP::InputMouseButtonEvent&>;

    SdkOverlay();
    ~SdkOverlay();

    SdkOverlay(const SdkOverlay&) = delete;
    SdkOverlay& operator=(const SdkOverlay&) = delete;

    bool Start(TruckersMP::Session* session);

    void Stop(TruckersMP::Session* session);

    bool RendererActive() const;

    const std::wstring& LastError() const { return lastError_; }

private:
    void OnPostRender();
    void OnKey(TruckersMP::InputKeyEvent& event);
    void OnChar(TruckersMP::InputCharEvent& event);
    void OnMouseMove(TruckersMP::InputMouseMoveEvent& event);
    void OnMouseWheel(TruckersMP::InputMouseWheelEvent& event);
    void OnMouseButton(TruckersMP::InputMouseButtonEvent& event);

    void RegisterListeners(TruckersMP::Session* session);
    void RemoveListeners(TruckersMP::Session* session);
    bool EnsureRenderer(TruckersMP::Session* session);
    void UpdateInputState(TruckersMP::Session* session, bool wantCursor, bool wantKeyboard);
    void ReleaseInputState(TruckersMP::Session* session);

    bool ResolveMouse(int& x, int& y);
    void SampleOsCursor();
    void FeedMouseMove(int x, int y);

    void Log(const char* text) const;
    void LogValue(const wchar_t* prefix, const std::wstring& value) const;

    std::atomic<TruckersMP::Session*> session_{ nullptr };

    std::unique_ptr<D3dOverlayRenderer> renderer_;
    bool rendererUnavailable_ = false;
    int rendererAttempts_ = 0;
    std::uint64_t deviceHandle_ = 0;

    bool renderRegistered_ = false;
    bool inputRegistered_ = false;
    PostRenderEvent::EventID postRenderId_ = 0;
    KeyEvent::EventID keyId_ = 0;
    CharEvent::EventID charId_ = 0;
    MouseMoveEvent::EventID mouseMoveId_ = 0;
    MouseWheelEvent::EventID mouseWheelId_ = 0;
    MouseButtonEvent::EventID mouseButtonId_ = 0;

    HWND gameWindow_ = nullptr;
    bool surfaceRequested_ = false;
    int viewportWidth_ = 0;
    int viewportHeight_ = 0;

    std::atomic<int> eventMouseX_{ 0 };
    std::atomic<int> eventMouseY_{ 0 };
    std::atomic<std::uint64_t> eventMouseTick_{ 0 };
    std::atomic<int> osMouseX_{ 0 };
    std::atomic<int> osMouseY_{ 0 };
    std::atomic<std::uint64_t> osMouseTick_{ 0 };
    std::atomic<int> fedMouseX_{ 0 };
    std::atomic<int> fedMouseY_{ 0 };
    std::atomic<bool> fedMouse_{ false };
    std::atomic<bool> mouseLogged_{ false };
    std::atomic<bool> eventMouseLogged_{ false };
    bool clientCursorVisible_ = false;
    bool clientMouseLocked_ = false;

    std::atomic<bool> inputSuppressed_{ false };
    std::atomic<bool> pointerAvailable_{ false };
    std::atomic<bool> panelVisible_{ false };
    std::atomic<bool> previousCursorVisible_{ false };
    std::atomic<int> blockedButton_{ -1 };

    bool cursorRequested_ = false;
    bool cursorOwned_ = false;
    bool keyboardLocked_ = false;
    bool keyboardLockOwned_ = false;

    std::wstring lastError_;
};
