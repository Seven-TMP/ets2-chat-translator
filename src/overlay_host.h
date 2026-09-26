#pragma once

#include <windows.h>

#include <cstdint>
#include <memory>
#include <vector>

struct OverlayFrame
{
    std::shared_ptr<const std::vector<std::uint32_t>> pixels;
    int width = 0;
    int height = 0;
    int x = 0;
    int y = 0;
    std::uint64_t revision = 0;
};

class OverlayHost
{
public:
    virtual ~OverlayHost() = default;

    virtual bool OverlayGameSurfaceActive() const = 0;
    virtual void OverlayUseGameSurface(HWND gameWindow) = 0;
    virtual void OverlayReleaseGameSurface() = 0;

    virtual bool OverlayVisible() const = 0;
    virtual HWND OverlayWindowHandle() const = 0;

    virtual void OverlaySetViewport(int width, int height) = 0;
    virtual bool OverlayHitTest(int x, int y) const = 0;
    virtual void OverlayMouseMove(int x, int y) = 0;
    virtual void OverlayMouseButton(int button, bool down, int x, int y) = 0;
    virtual void OverlayMouseWheel(int delta, int x, int y) = 0;
    virtual bool OverlayWantsTextInput() const = 0;
    virtual bool OverlayOwnsKeyboard() const = 0;
    virtual void OverlayKeyDown(int virtualKey) = 0;
    virtual void OverlayCharacter(unsigned int codepoint) = 0;
    virtual void OverlaySetGameInputLocked(bool locked) = 0;
    virtual bool OverlayWantsCursor() const = 0;
    virtual void OverlaySetGameCursorVisible(bool available) = 0;

    virtual bool OverlayAcquireFrame(OverlayFrame& out) const = 0;
};
