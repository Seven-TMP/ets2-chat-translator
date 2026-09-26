#include "sdk_overlay.h"

#include "d3d_overlay.h"
#include "game_window.h"
#include "overlay_link.h"
#include "text_codec.h"

#include <string>

namespace
{
constexpr std::uint64_t kMouseFreshMs = 800;

bool Succeeded(TruckersMP::Result result)
{
    return result == TruckersMP::Result::Ok;
}
} // namespace

SdkOverlay::SdkOverlay() = default;

SdkOverlay::~SdkOverlay() = default;

bool SdkOverlay::Start(TruckersMP::Session* session)
{
    if (!session) return false;

    TruckersMP::Session* previous = session_.load();
    if (previous == session) {
        return true;
    }
    if (previous) Stop(previous);

    session_.store(session);
    lastError_.clear();
    rendererUnavailable_ = false;
    rendererAttempts_ = 0;
    surfaceRequested_ = false;

    if (!session->Render().IsAvailable()) {
        lastError_ = L"TruckersMP render API unavailable";
        Log("overlay: client render API unavailable; the panel cannot be displayed");
        return false;
    }

    RegisterListeners(session);
    Log("overlay: listening for client render and input events");
    return true;
}

void SdkOverlay::Stop(TruckersMP::Session* session)
{
    TruckersMP::Session* target = session;
    if (!target) target = session_.load();
    if (!target) return;

    RemoveListeners(target);
    ReleaseInputState(target);

    if (surfaceRequested_) {
        if (auto host = overlay_link::Current()) {
            host->OverlayReleaseGameSurface();
        }
        surfaceRequested_ = false;
    }

    if (renderer_) renderer_->Reset();
    renderer_.reset();
    rendererUnavailable_ = false;
    deviceHandle_ = 0;
    gameWindow_ = nullptr;
    viewportWidth_ = 0;
    viewportHeight_ = 0;
    eventMouseTick_.store(0);
    osMouseTick_.store(0);
    fedMouse_.store(false);
    mouseLogged_.store(false);
    eventMouseLogged_.store(false);
    clientCursorVisible_ = false;
    clientMouseLocked_ = false;
    inputSuppressed_.store(false);
    pointerAvailable_.store(false);
    panelVisible_.store(false);
    previousCursorVisible_.store(false);
    blockedButton_.store(-1);
    session_.store(nullptr);
}

bool SdkOverlay::RendererActive() const
{
    return renderer_ && renderer_->Ready();
}

void SdkOverlay::RegisterListeners(TruckersMP::Session* session)
{
    if (!renderRegistered_) {
        postRenderId_ = session->Render().OnPostRender.Register([this]() {
            OnPostRender();
        });
        renderRegistered_ = true;
    }

    if (inputRegistered_ || !session->Input().IsAvailable()) return;

    keyId_ = session->Input().OnKey.Register([this](TruckersMP::InputKeyEvent& event) {
        OnKey(event);
    });
    charId_ = session->Input().OnChar.Register([this](TruckersMP::InputCharEvent& event) {
        OnChar(event);
    });
    mouseMoveId_ = session->Input().OnMouseMove.Register([this](TruckersMP::InputMouseMoveEvent& event) {
        OnMouseMove(event);
    });
    mouseWheelId_ = session->Input().OnMouseWheel.Register([this](TruckersMP::InputMouseWheelEvent& event) {
        OnMouseWheel(event);
    });
    mouseButtonId_ = session->Input().OnMouseButton.Register([this](TruckersMP::InputMouseButtonEvent& event) {
        OnMouseButton(event);
    });
    inputRegistered_ = true;
}

void SdkOverlay::RemoveListeners(TruckersMP::Session* session)
{
    if (!session) return;

    if (renderRegistered_) {
        session->Render().OnPostRender.Unregister(postRenderId_);
        renderRegistered_ = false;
    }
    if (inputRegistered_) {
        session->Input().OnKey.Unregister(keyId_);
        session->Input().OnChar.Unregister(charId_);
        session->Input().OnMouseMove.Unregister(mouseMoveId_);
        session->Input().OnMouseWheel.Unregister(mouseWheelId_);
        session->Input().OnMouseButton.Unregister(mouseButtonId_);
        inputRegistered_ = false;
    }
}

bool SdkOverlay::EnsureRenderer(TruckersMP::Session* session)
{
    const auto handle = session->Render().GetDeviceHandle();

    if (renderer_ && renderer_->Ready()) {
        if (handle && *handle == deviceHandle_) return true;
        renderer_->Reset();
        renderer_.reset();
        deviceHandle_ = 0;
        rendererUnavailable_ = false;
        rendererAttempts_ = 0;
        Log("overlay: client device changed, rebuilding the overlay pipeline");
    }

    if (rendererUnavailable_) return false;

    const auto rendererId = session->Render().GetRendererID();
    if (rendererId && *rendererId != TruckersMP::RendererID::DirectX11) {
        rendererUnavailable_ = true;
        Log("overlay: client is not on DirectX 11; the panel cannot be displayed");
        return false;
    }

    if (!handle || *handle == 0) return false;   // device not ready yet, retry later

    auto device = reinterpret_cast<ID3D11Device*>(static_cast<UINT_PTR>(*handle));
    if (!renderer_) renderer_ = std::make_unique<D3dOverlayRenderer>();

    ++rendererAttempts_;
    if (renderer_->Initialize(device)) {
        deviceHandle_ = *handle;
        Log("overlay: bound to the client DirectX 11 device");
        return true;
    }

    lastError_ = renderer_->LastError();
    LogValue(L"overlay: renderer init failed: ", lastError_);
    renderer_.reset();
    if (rendererAttempts_ >= 3) rendererUnavailable_ = true;
    return false;
}

void SdkOverlay::OnPostRender()
{
    TruckersMP::Session* session = session_.load();
    if (!session) return;

    const bool rendererReady = EnsureRenderer(session);

    auto host = overlay_link::Current();
    if (!host) {
        ReleaseInputState(session);
        return;
    }

    if (!surfaceRequested_ && rendererReady) {
        gameWindow_ = game_window::Find(nullptr);
        if (gameWindow_) {
            host->OverlayUseGameSurface(gameWindow_);
            surfaceRequested_ = true;
            Log("overlay: asking the panel to render inside the client");
        }
    }

    if (!surfaceRequested_) {
        ReleaseInputState(session);
        return;
    }

    int clientWidth = 0;
    int clientHeight = 0;
    if (!gameWindow_ || !IsWindow(gameWindow_)) {
        gameWindow_ = game_window::Find(nullptr);
        viewportWidth_ = 0;
        viewportHeight_ = 0;
    }
    if (!game_window::ClientSize(gameWindow_, clientWidth, clientHeight)) {
        ReleaseInputState(session);
        return;
    }
    if (clientWidth != viewportWidth_ || clientHeight != viewportHeight_) {
        viewportWidth_ = clientWidth;
        viewportHeight_ = clientHeight;
        host->OverlaySetViewport(clientWidth, clientHeight);
    }

    const bool active = host->OverlayGameSurfaceActive();
    const bool visible = active && host->OverlayVisible();

    if (visible != panelVisible_.load()) {
        panelVisible_.store(visible);
        inputSuppressed_.store(false);
        blockedButton_.store(-1);
    }

    const bool cursorShown = session->Input().IsMouseVisible().value_or(false);
    if (inputSuppressed_.load() && cursorShown && !previousCursorVisible_.load()) {
        inputSuppressed_.store(false);
    }
    previousCursorVisible_.store(cursorShown);

    UpdateInputState(session,
        visible && !inputSuppressed_.load() && host->OverlayWantsCursor(),
        visible && !inputSuppressed_.load() && host->OverlayWantsTextInput());

    pointerAvailable_.store(clientCursorVisible_ || (cursorRequested_ && cursorOwned_));
    host->OverlaySetGameCursorVisible(pointerAvailable_.load());

    if (visible) {
        SampleOsCursor();
        int mouseX = 0;
        int mouseY = 0;
        if (ResolveMouse(mouseX, mouseY)) FeedMouseMove(mouseX, mouseY);
    }

    if (!visible || !rendererReady) return;

    OverlayFrame frame;
    if (!host->OverlayAcquireFrame(frame)) return;
    if (!renderer_->Draw(frame, clientWidth, clientHeight)) {
        lastError_ = renderer_->LastError();
    }
}

void SdkOverlay::OnKey(TruckersMP::InputKeyEvent& event)
{
    auto host = overlay_link::Current();
    if (!host) return;

    const int key = (int)event.GetKey();
    if (event.GetDown() && (key == VK_TAB || key == VK_ESCAPE)) {
        if (cursorRequested_ || keyboardLocked_) {
            if (TruckersMP::Session* session = session_.load()) ReleaseInputState(session);
            Log(key == VK_TAB ? "overlay: Tab pressed, gave the pointer and keyboard back to the game"
                              : "overlay: Esc pressed, gave the pointer and keyboard back to the game");
        }
        inputSuppressed_.store(true);
        blockedButton_.store(-1);
    }

    if (!host->OverlayOwnsKeyboard()) return;
    host->OverlayKeyDown(key);
}

void SdkOverlay::OnChar(TruckersMP::InputCharEvent& event)
{
    auto host = overlay_link::Current();
    if (!host || !host->OverlayOwnsKeyboard() || inputSuppressed_.load()) return;
    host->OverlayCharacter((unsigned int)event.GetCharacter());
}

void SdkOverlay::OnMouseMove(TruckersMP::InputMouseMoveEvent& event)
{
    eventMouseX_.store((int)event.GetX());
    eventMouseY_.store((int)event.GetY());
    eventMouseTick_.store(GetTickCount64());
    if (!eventMouseLogged_.exchange(true)) {
        Log("overlay: client mouse move events received");
    }

    auto host = overlay_link::Current();
    if (!host || !host->OverlayGameSurfaceActive()) return;
    FeedMouseMove(eventMouseX_.load(), eventMouseY_.load());
}

void SdkOverlay::OnMouseWheel(TruckersMP::InputMouseWheelEvent& event)
{
    auto host = overlay_link::Current();
    if (!host) return;
    if (!pointerAvailable_.load()) return;

    int x = 0;
    int y = 0;
    if (!ResolveMouse(x, y)) return;
    if (!host->OverlayHitTest(x, y)) return;
    host->OverlayMouseWheel((int)event.GetDelta(), x, y);
}

void SdkOverlay::OnMouseButton(TruckersMP::InputMouseButtonEvent& event)
{
    auto host = overlay_link::Current();
    if (!host) return;

    const int button = (int)event.GetButton();
    const bool down = event.GetDown();

    int x = 0;
    int y = 0;
    const bool resolved = ResolveMouse(x, y);
    const bool overPanel = resolved && pointerAvailable_.load() && host->OverlayHitTest(x, y);

    if (!down) {
        const bool paired = blockedButton_.load() == button;
        if (paired) blockedButton_.store(-1);
        if (!paired && !overPanel) return;
        event.SetBlock(true);
        if (overPanel) host->OverlayMouseButton(button, false, x, y);
        return;
    }

    if (!overPanel) {
        if (pointerAvailable_.load() && host->OverlayWantsTextInput()) host->OverlayKeyDown(VK_TAB);
        return;
    }

    inputSuppressed_.store(false);
    event.SetBlock(true);
    blockedButton_.store(button);
    host->OverlayMouseButton(button, true, x, y);
}

bool SdkOverlay::ResolveMouse(int& x, int& y)
{
    const std::uint64_t now = GetTickCount64();

    const std::uint64_t eventTick = eventMouseTick_.load();
    if (eventTick != 0 && now - eventTick <= kMouseFreshMs) {
        x = eventMouseX_.load();
        y = eventMouseY_.load();
        return true;
    }

    const std::uint64_t osTick = osMouseTick_.load();
    if (osTick != 0 && now - osTick <= kMouseFreshMs) {
        x = osMouseX_.load();
        y = osMouseY_.load();
        return true;
    }

    if (eventTick == 0) return false;
    x = eventMouseX_.load();
    y = eventMouseY_.load();
    return true;
}

void SdkOverlay::SampleOsCursor()
{
    if (!cursorRequested_ || !gameWindow_ || !IsWindow(gameWindow_)) return;

    POINT cursor{};
    if (!GetCursorPos(&cursor)) return;

    POINT origin{};
    if (!game_window::ClientOrigin(gameWindow_, origin)) return;

    int width = 0;
    int height = 0;
    if (!game_window::ClientSize(gameWindow_, width, height)) return;

    const int x = (int)cursor.x - (int)origin.x;
    const int y = (int)cursor.y - (int)origin.y;
    if (x < 0 || y < 0 || x >= width || y >= height) return;

    if (x == osMouseX_.load() && y == osMouseY_.load()) return;

    osMouseX_.store(x);
    osMouseY_.store(y);
    osMouseTick_.store(GetTickCount64());

    if (!mouseLogged_.exchange(true)) {
        std::string line = "overlay: cursor tracked at ";
        line += std::to_string(x);
        line += ",";
        line += std::to_string(y);
        line += ", cursor visible=";
        line += (clientCursorVisible_) ? "yes" : "no";
        line += ", game mouse locked=";
        line += (clientMouseLocked_) ? "yes" : "no";
        Log(line.c_str());
    }
}

void SdkOverlay::FeedMouseMove(int x, int y)
{
    if (fedMouse_.load() && x == fedMouseX_.load() && y == fedMouseY_.load()) return;
    if (!pointerAvailable_.load()) return;

    auto host = overlay_link::Current();
    if (!host || !host->OverlayGameSurfaceActive()) return;

    fedMouse_.store(true);
    fedMouseX_.store(x);
    fedMouseY_.store(y);
    host->OverlayMouseMove(x, y);
}

void SdkOverlay::UpdateInputState(TruckersMP::Session* session, bool wantCursor, bool wantKeyboard)
{
    if (!session || !session->Input().IsAvailable()) return;

    clientCursorVisible_ = session->Input().IsMouseVisible().value_or(false);
    clientMouseLocked_ = session->Input().IsGameMouseLocked().value_or(false);

    if (wantCursor && !cursorRequested_) {
        if (clientCursorVisible_) {
            cursorRequested_ = true;
            cursorOwned_ = false;
        } else if (Succeeded(session->Input().IncreaseMouseRef())) {
            cursorRequested_ = true;
            cursorOwned_ = true;
            Log("overlay: borrowed the client mouse reference for the panel pointer");
        }
    } else if (!wantCursor && cursorRequested_) {
        if (cursorOwned_) {
            session->Input().DecreaseMouseRef();
            Log("overlay: gave the client mouse reference back");
        }
        cursorRequested_ = false;
        cursorOwned_ = false;
    }

    if (wantKeyboard && !keyboardLocked_) {
        if (session->Input().IsGameKeyboardLocked().value_or(false)) {
            keyboardLocked_ = true;
            keyboardLockOwned_ = false;
        } else if (Succeeded(session->Input().SetGameKeyboardLocked(true))) {
            keyboardLocked_ = true;
            keyboardLockOwned_ = true;
            Log("overlay: locked the game keyboard while a panel text field is focused");
        }
    } else if (!wantKeyboard && keyboardLocked_) {
        if (keyboardLockOwned_) {
            session->Input().SetGameKeyboardLocked(false);
            Log("overlay: game keyboard unlocked");
        }
        keyboardLocked_ = false;
        keyboardLockOwned_ = false;
    }
}

void SdkOverlay::ReleaseInputState(TruckersMP::Session* session)
{
    if (session && session->Input().IsAvailable()) {
        UpdateInputState(session, false, false);
        return;
    }
    cursorRequested_ = false;
    cursorOwned_ = false;
    keyboardLocked_ = false;
    keyboardLockOwned_ = false;
}

void SdkOverlay::Log(const char* text) const
{
    TruckersMP::Session* session = session_.load();
    if (!session || !session->Core().IsAvailable()) return;
    session->Core().LogMessage(TruckersMP::LogLevel::Info, text);
}

void SdkOverlay::LogValue(const wchar_t* prefix, const std::wstring& value) const
{
    if (value.empty()) return;
    const std::string line = text::ToUtf8(std::wstring(prefix) + value);
    Log(line.c_str());
}
