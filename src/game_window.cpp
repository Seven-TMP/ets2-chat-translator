#include "game_window.h"

namespace
{
bool WaitForForegroundWindow(HWND target, DWORD timeoutMs)
{
    const DWORD deadline = GetTickCount() + timeoutMs;
    for (;;) {
        if (GetForegroundWindow() == target) return true;
        if ((LONG)(GetTickCount() - deadline) >= 0) return false;
        Sleep(8);
    }
}
}

namespace game_window
{
HWND Find(HWND exclude)
{
    struct Ctx
    {
        DWORD pid;
        HWND exclude;
        HWND result;
        LONG bestArea;
    };
    Ctx ctx{ GetCurrentProcessId(), exclude, nullptr, 0 };

    EnumWindows([](HWND hwnd, LPARAM lParam) -> BOOL {
        auto& ctx = *reinterpret_cast<Ctx*>(lParam);
        if (hwnd == ctx.exclude) return TRUE;

        DWORD wp = 0;
        GetWindowThreadProcessId(hwnd, &wp);
        if (wp != ctx.pid) return TRUE;
        if (!IsWindowVisible(hwnd)) return TRUE;
        if (GetWindow(hwnd, GW_OWNER) != nullptr) return TRUE;

        RECT rc{};
        GetClientRect(hwnd, &rc);
        const LONG area = (rc.right - rc.left) * (rc.bottom - rc.top);
        if (area > ctx.bestArea) {
            ctx.bestArea = area;
            ctx.result = hwnd;
        }
        return TRUE;
    }, reinterpret_cast<LPARAM>(&ctx));

    return ctx.result;
}

bool ClientSize(HWND window, int& width, int& height)
{
    width = 0;
    height = 0;
    if (!window || !IsWindow(window)) return false;
    RECT rc{};
    if (!GetClientRect(window, &rc)) return false;
    width = (int)(rc.right - rc.left);
    height = (int)(rc.bottom - rc.top);
    return width > 0 && height > 0;
}

bool ClientOrigin(HWND window, POINT& origin)
{
    origin.x = 0;
    origin.y = 0;
    if (!window || !IsWindow(window)) return false;
    POINT p{ 0, 0 };
    if (!ClientToScreen(window, &p)) return false;
    origin = p;
    return true;
}

bool Activate(HWND window)
{
    if (!window || !IsWindow(window)) return false;

    if (IsIconic(window)) ShowWindow(window, SW_RESTORE);
    else ShowWindow(window, SW_SHOW);

    const DWORD currentThread = GetCurrentThreadId();
    const DWORD gameThread = GetWindowThreadProcessId(window, nullptr);
    const HWND foreground = GetForegroundWindow();
    const DWORD foregroundThread = foreground ? GetWindowThreadProcessId(foreground, nullptr) : 0;

    if (gameThread && gameThread != currentThread) {
        AttachThreadInput(gameThread, currentThread, TRUE);
    }
    if (foregroundThread && foregroundThread != currentThread && foregroundThread != gameThread) {
        AttachThreadInput(foregroundThread, currentThread, TRUE);
    }

    SetForegroundWindow(window);
    SetFocus(window);
    SetActiveWindow(window);

    if (gameThread && gameThread != currentThread) {
        AttachThreadInput(gameThread, currentThread, FALSE);
    }
    if (foregroundThread && foregroundThread != currentThread) {
        AttachThreadInput(foregroundThread, currentThread, FALSE);
    }

    if (WaitForForegroundWindow(window, 250)) return true;

    SetForegroundWindow(window);
    BringWindowToTop(window);
    return WaitForForegroundWindow(window, 250);
}
}
