#pragma once

#include <windows.h>

namespace game_window
{
    HWND Find(HWND exclude = nullptr);

    bool ClientSize(HWND window, int& width, int& height);

    bool ClientOrigin(HWND window, POINT& origin);

    bool Activate(HWND window);
}
