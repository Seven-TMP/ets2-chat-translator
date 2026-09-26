#include "chat_panel.h"

#include <chrono>
#include <cstdint>
#include <cstring>
#include <string>
#include <thread>
#include <windows.h>
#include <windowsx.h>

namespace
{
const wchar_t* kPreviewClass = L"ETS2TranslatorPreviewHost";
constexpr UINT_PTR kRepaintTimerId = 1;
constexpr int kRepaintIntervalMs = 33;
constexpr int kPreviewWidth = 1100;
constexpr int kPreviewHeight = 720;
constexpr int kGeometryWidth = 600;
constexpr int kGeometryHeight = 540;

std::wstring PreviewGeometryPath()
{
    wchar_t buffer[MAX_PATH]{};
    DWORD length = GetTempPathW(MAX_PATH, buffer);
    std::wstring path(buffer, length);
    path += L"ets2_chat_translator_preview_geometry.json";
    return path;
}

void ResetPreviewGeometry(const std::wstring& path)
{
    HANDLE file = CreateFileW(path.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS,
        FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) return;
    const std::string json = "{\n  \"x\": 12,\n  \"y\": 12,\n  \"width\": "
        + std::to_string(kGeometryWidth) + ",\n  \"height\": "
        + std::to_string(kGeometryHeight) + "\n}\n";
    DWORD written = 0;
    WriteFile(file, json.data(), (DWORD)json.size(), &written, nullptr);
    CloseHandle(file);
}

struct PreviewSurface
{
    HDC dc = nullptr;
    HBITMAP bitmap = nullptr;
    HGDIOBJ oldBitmap = nullptr;
    void* bits = nullptr;
    int width = 0;
    int height = 0;

    ~PreviewSurface() { Release(); }

    void Release()
    {
        if (dc && oldBitmap) SelectObject(dc, oldBitmap);
        oldBitmap = nullptr;
        bits = nullptr;
        if (bitmap) DeleteObject(bitmap);
        bitmap = nullptr;
        if (dc) DeleteDC(dc);
        dc = nullptr;
        width = 0;
        height = 0;
    }

    bool Ensure(int w, int h)
    {
        if (dc && bitmap && width == w && height == h) return true;
        Release();
        if (w <= 0 || h <= 0) return false;

        BITMAPINFO info{};
        info.bmiHeader.biSize = sizeof(info.bmiHeader);
        info.bmiHeader.biWidth = w;
        info.bmiHeader.biHeight = -h;   // top-down: same layout as OverlayFrame
        info.bmiHeader.biPlanes = 1;
        info.bmiHeader.biBitCount = 32;
        info.bmiHeader.biCompression = BI_RGB;

        HDC screen = GetDC(nullptr);
        bitmap = CreateDIBSection(screen, &info, DIB_RGB_COLORS, &bits, nullptr, 0);
        dc = CreateCompatibleDC(screen);
        ReleaseDC(nullptr, screen);
        if (!bitmap || !dc || !bits) {
            Release();
            return false;
        }
        oldBitmap = SelectObject(dc, bitmap);
        width = w;
        height = h;
        return true;
    }

    void Draw(HDC target, const OverlayFrame& frame)
    {
        if (!frame.pixels) return;
        if (!Ensure(frame.width, frame.height)) return;
        memcpy(bits, frame.pixels->data(), (size_t)frame.width * (size_t)frame.height * sizeof(std::uint32_t));

        BLENDFUNCTION blend{ AC_SRC_OVER, 0, 255, AC_SRC_ALPHA };
        GdiAlphaBlend(target, frame.x, frame.y, frame.width, frame.height,
            dc, 0, 0, frame.width, frame.height, blend);
    }
};

struct PreviewHost
{
    HWND hwnd = nullptr;
    ChatPanel* panel = nullptr;
    PreviewSurface surface;
    std::uint64_t paintedRevision = 0;
    bool surfaceRequested = false;
    bool pointerFree = true;
};

PreviewHost g_host;

void SyncViewport()
{
    if (!g_host.hwnd || !g_host.panel) return;
    RECT rc{};
    if (!GetClientRect(g_host.hwnd, &rc)) return;
    const int width = (int)(rc.right - rc.left);
    const int height = (int)(rc.bottom - rc.top);
    if (width <= 0 || height <= 0) return;

    g_host.panel->OverlaySetViewport(width, height);
    if (!g_host.surfaceRequested) {
        g_host.panel->OverlayUseGameSurface(g_host.hwnd);
        g_host.panel->OverlaySetGameCursorVisible(true);
        g_host.surfaceRequested = true;
    }
}

void ForwardButton(int button, bool down, LPARAM lp)
{
    if (!g_host.panel) return;
    g_host.panel->OverlayMouseButton(button, down, GET_X_LPARAM(lp), GET_Y_LPARAM(lp));
}

LRESULT CALLBACK PreviewProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    switch (msg) {
    case WM_CREATE: {
        auto cs = reinterpret_cast<CREATESTRUCTW*>(lp);
        g_host.hwnd = hwnd;
        g_host.panel = static_cast<ChatPanel*>(cs->lpCreateParams);
        SetTimer(hwnd, kRepaintTimerId, kRepaintIntervalMs, nullptr);
        SyncViewport();
        return 0;
    }
    case WM_SIZE:
        SyncViewport();
        InvalidateRect(hwnd, nullptr, FALSE);
        return 0;
    case WM_PAINT: {
        PAINTSTRUCT ps{};
        HDC dc = BeginPaint(hwnd, &ps);
        RECT rc{};
        GetClientRect(hwnd, &rc);

        HBRUSH background = CreateSolidBrush(RGB(28, 34, 48));
        FillRect(dc, &rc, background);
        DeleteObject(background);

        if (g_host.panel && g_host.panel->OverlayVisible()) {
            OverlayFrame frame;
            if (g_host.panel->OverlayAcquireFrame(frame)) {
                g_host.surface.Draw(dc, frame);
                g_host.paintedRevision = frame.revision;
            }
        }
        EndPaint(hwnd, &ps);
        return 0;
    }
    case WM_TIMER:
        if (wp == kRepaintTimerId && g_host.panel) {
            OverlayFrame frame;
            const bool ready = g_host.panel->OverlayAcquireFrame(frame);
            const std::uint64_t revision = (ready && g_host.panel->OverlayVisible()) ? frame.revision : 0;
            if (revision != g_host.paintedRevision) InvalidateRect(hwnd, nullptr, FALSE);
        }
        return 0;
    case WM_MOUSEMOVE:
        if (g_host.panel) g_host.panel->OverlayMouseMove(GET_X_LPARAM(lp), GET_Y_LPARAM(lp));
        return 0;
    case WM_LBUTTONDOWN:
        SetCapture(hwnd);
        ForwardButton(0, true, lp);
        return 0;
    case WM_LBUTTONUP:
        ReleaseCapture();
        ForwardButton(0, false, lp);
        return 0;
    case WM_RBUTTONDOWN:
        ForwardButton(1, true, lp);
        return 0;
    case WM_RBUTTONUP:
        ForwardButton(1, false, lp);
        return 0;
    case WM_MBUTTONDOWN:
        ForwardButton(2, true, lp);
        return 0;
    case WM_MBUTTONUP:
        ForwardButton(2, false, lp);
        return 0;
    case WM_MOUSEWHEEL: {
        if (!g_host.panel) return 0;
        POINT p{ GET_X_LPARAM(lp), GET_Y_LPARAM(lp) };
        ScreenToClient(hwnd, &p);
        g_host.panel->OverlayMouseWheel(GET_WHEEL_DELTA_WPARAM(wp), p.x, p.y);
        return 0;
    }
    case WM_KEYDOWN:
        if (wp == VK_F8) {
            g_host.pointerFree = !g_host.pointerFree;
            if (g_host.panel) g_host.panel->OverlaySetGameCursorVisible(g_host.pointerFree);
            InvalidateRect(hwnd, nullptr, FALSE);
            return 0;
        }
        if (g_host.panel) g_host.panel->OverlayKeyDown((int)wp);
        return 0;
    case WM_CHAR:
        if (g_host.panel) g_host.panel->OverlayCharacter((unsigned int)wp);
        return 0;
    case WM_ERASEBKGND:
        return 1;
    case WM_DESTROY:
        KillTimer(hwnd, kRepaintTimerId);
        g_host.hwnd = nullptr;
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

ChatEntry Entry(const wchar_t* time, const wchar_t* author, const wchar_t* body,
    const wchar_t* translated, PlayerRole role = PlayerRole::None)
{
    ChatEntry e;
    e.time = time;
    e.author = author;
    e.body = body;
    e.translated = translated;
    e.role = role;
    return e;
}

ChatEntry Service(const wchar_t* time, const wchar_t* body)
{
    ChatEntry e;
    e.time = time;
    e.body = body;
    e.translated = body;
    e.serviceLine = true;
    return e;
}

ChatEntry Info(const wchar_t* time, const wchar_t* body)
{
    ChatEntry e;
    e.time = time;
    e.body = body;
    e.translated = body;
    e.infoLine = true;
    e.searchOnly = true;
    return e;
}

DWORD WINAPI FeedThread(void* param)
{
    auto* panel = static_cast<ChatPanel*>(param);
    std::this_thread::sleep_for(std::chrono::milliseconds(180));
    panel->Status(L"预览模式：点击搜索框/输入框可直接输入，拖动标题栏移动，右下角斜纹缩放，右上角 ✕ 隐藏");

    panel->Push(Service(L"14:34:10", L"[System] Player TAHA_27 (700) has been banned."));
    panel->Push(Entry(L"14:34:11", L"YGMK (729)", L"不要变卖", L"不要变卖"));
    panel->Push(Entry(L"14:34:12", L"Defne (274)", L"sry broooo", L"抱歉，兄弟"));
    panel->Push(Entry(L"14:34:14", L"Wezy", L"please slow down in the city",
        L"在城里请减速", PlayerRole::GameModerator));
    panel->Push(Entry(L"14:34:16", L"TruckersMP Management", L"convoy starts in 10 minutes",
        L"车队 10 分钟后出发", PlayerRole::Manager));
    panel->Push(Entry(L"14:34:18", L"Support Team", L"welcome to TruckersMP!",
        L"欢迎来到 TruckersMP！", PlayerRole::TeamMember));
    panel->Push(Entry(L"14:34:19", L"BigSupporter", L"nice paint job!",
        L"涂装不错！", PlayerRole::Patron));
    panel->Push(Entry(L"14:34:13", L"User_6116755 (509)", L"？？？？、", L"？？？？、"));
    panel->Push(Entry(L"14:34:20", L"Prime Logistics Averka", L"ty mate", L"谢谢，伙计"));
    panel->Push(Info(L"14:34:39", L"日志信息：User_6116755  临时编号 509  TMPID 6116755  SteamID64 76561198757795883"));
    panel->Push(Info(L"14:35:31", L"日志信息：Turho3161  临时编号 457  TMPID 5094215  SteamID64 76561199326957802  Tag SOL_SERT REZERV"));
    return 0;
}
}

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int)
{
    RuntimeConfig runtime;
    runtime.fontSize = 16;
    runtime.overlayOpacity = 98;
    runtime.overlayHotkey = L"F9";

    const std::wstring geometryPath = PreviewGeometryPath();
    ResetPreviewGeometry(geometryPath);

    ChatPanel panel;
    if (!panel.Open(instance, runtime, geometryPath)) return 1;
    panel.SetCloseButtonExits(true);

    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = PreviewProc;
    wc.hInstance = instance;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.lpszClassName = kPreviewClass;
    wc.hbrBackground = nullptr;
    RegisterClassExW(&wc);

    HWND host = CreateWindowExW(0, kPreviewClass, L"ETS2 Chat Translator · 游戏内悬浮窗预览（F8 模拟游戏锁定鼠标）",
        WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, kPreviewWidth, kPreviewHeight,
        nullptr, nullptr, instance, &panel);
    if (!host) return 1;

    ShowWindow(host, SW_SHOW);
    UpdateWindow(host);

    HANDLE feeder = CreateThread(nullptr, 0, FeedThread, &panel, 0, nullptr);
    if (feeder) CloseHandle(feeder);

    panel.MessageLoop();

    if (host && IsWindow(host)) DestroyWindow(host);
    panel.Close();
    return 0;
}
