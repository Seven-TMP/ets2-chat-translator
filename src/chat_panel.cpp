#include "chat_panel.h"
#include "text_codec.h"

#include <algorithm>
#include <commctrl.h>
#include <cwctype>
#include <cstdint>
#include <fstream>
#include <sstream>
#include <windowsx.h>

#pragma comment(lib, "imm32")

namespace
{
const wchar_t* kClass = L"ETS2TranslatorPanelV4Simple";
const COLORREF cBack = RGB(9, 12, 18);
const COLORREF cPanel = RGB(18, 23, 34);
const COLORREF cCard = RGB(24, 30, 44);
const COLORREF cCardAlt = RGB(28, 35, 50);
const COLORREF cLine = RGB(52, 60, 78);
const COLORREF cText = RGB(243, 244, 246);
const COLORREF cDim = RGB(145, 158, 180);
const COLORREF cName = RGB(16, 185, 129);
const COLORREF cTime = RGB(125, 140, 165);
const COLORREF cTrans = RGB(255, 215, 100);
const COLORREF cWarn = RGB(239, 68, 68);
const COLORREF cBlue = RGB(59, 130, 246);
const COLORREF cCyan = RGB(34, 211, 238);
constexpr int kTimeColumnW = 68;

BYTE BackgroundAlpha(int opacity)
{
    int clamped = (std::max)(0, (std::min)(100, opacity));
    return (BYTE)((clamped * 255 + 50) / 100);
}

int JsonInt(const std::string& json, const std::string& key, int fallback)
{
    std::string needle = "\"" + key + "\"";
    size_t p = json.find(needle);
    if (p == std::string::npos) return fallback;
    p = json.find(':', p + needle.size());
    if (p == std::string::npos) return fallback;
    try { return std::stoi(json.substr(p + 1)); } catch (...) { return fallback; }
}

RECT WorkAreaForRect(const RECT& rect)
{
    RECT area{};
    HMONITOR monitor = MonitorFromRect(&rect, MONITOR_DEFAULTTONEAREST);
    MONITORINFO info{};
    info.cbSize = sizeof(info);
    if (monitor && GetMonitorInfoW(monitor, &info)) return info.rcWork;
    SystemParametersInfoW(SPI_GETWORKAREA, 0, &area, 0);
    return area;
}

RECT ClampWindowRect(RECT rect)
{
    constexpr int minW = 420;
    constexpr int minH = 260;
    int w = (std::max)(minW, (int)(rect.right - rect.left));
    int h = (std::max)(minH, (int)(rect.bottom - rect.top));
    RECT probe{ rect.left, rect.top, rect.left + w, rect.top + h };
    RECT area = WorkAreaForRect(probe);
    int areaW = (int)(area.right - area.left);
    int areaH = (int)(area.bottom - area.top);
    int maxW = (std::max)(minW, areaW);
    int maxH = (std::max)(minH, areaH);
    w = (std::min)(w, maxW);
    h = (std::min)(h, maxH);
    int x = rect.left;
    int y = rect.top;
    if (x + w > area.right) x = area.right - w;
    if (y + h > area.bottom) y = area.bottom - h;
    if (x < area.left) x = area.left;
    if (y < area.top) y = area.top;
    return RECT{ x, y, x + w, y + h };
}

RECT DefaultWindowRect()
{
    int w = 600;
    int h = 540;
    RECT area{};
    SystemParametersInfoW(SPI_GETWORKAREA, 0, &area, 0);
    int x = area.right - w - 24;
    int y = area.top + 72;
    return ClampWindowRect(RECT{ x, y, x + w, y + h });
}

RECT LoadWindowRect(const std::wstring& path)
{
    RECT fallback = DefaultWindowRect();
    if (path.empty()) return fallback;
    std::ifstream f(path, std::ios::binary);
    if (!f) return fallback;
    std::ostringstream buf;
    buf << f.rdbuf();
    std::string json = buf.str();
    if (json.empty()) return fallback;
    int x = JsonInt(json, "x", fallback.left);
    int y = JsonInt(json, "y", fallback.top);
    int w = JsonInt(json, "width", fallback.right - fallback.left);
    int h = JsonInt(json, "height", fallback.bottom - fallback.top);
    return ClampWindowRect(RECT{ x, y, x + w, y + h });
}

void Fill(HDC dc, RECT r, COLORREF color)
{
    HBRUSH b = CreateSolidBrush(color);
    FillRect(dc, &r, b);
    DeleteObject(b);
}

void DrawTextLine(HDC dc, HFONT font, COLORREF color, const std::wstring& text, RECT r, UINT flags)
{
    HFONT old = (HFONT)SelectObject(dc, font);
    SetTextColor(dc, color);
    DrawTextW(dc, text.c_str(), -1, &r, flags | DT_NOPREFIX);
    SelectObject(dc, old);
}

void RoundFill(HDC dc, RECT r, int radius, COLORREF color)
{
    HBRUSH brush = CreateSolidBrush(color);
    HPEN pen = CreatePen(PS_SOLID, 1, color);
    HGDIOBJ oldBrush = SelectObject(dc, brush);
    HGDIOBJ oldPen = SelectObject(dc, pen);
    RoundRect(dc, r.left, r.top, r.right, r.bottom, radius, radius);
    SelectObject(dc, oldPen);
    SelectObject(dc, oldBrush);
    DeleteObject(pen);
    DeleteObject(brush);
}

void PrepareLayeredBitmap(HBITMAP bmp, int width, int height, BYTE backgroundAlpha)
{
    BITMAP bitmap{};
    if (!GetObjectW(bmp, sizeof(bitmap), &bitmap) || !bitmap.bmBits) return;
    const int stride = bitmap.bmWidthBytes;
    auto* bits = static_cast<std::uint8_t*>(bitmap.bmBits);
    for (int y = 0; y < height; ++y) {
        auto* row = reinterpret_cast<std::uint32_t*>(bits + y * stride);
        for (int x = 0; x < width; ++x) {
            std::uint32_t pixel = row[x];
            BYTE b = (BYTE)(pixel & 0xFF);
            BYTE g = (BYTE)((pixel >> 8) & 0xFF);
            BYTE r = (BYTE)((pixel >> 16) & 0xFF);
            BYTE maxChannel = (std::max)(r, (std::max)(g, b));
            BYTE a = maxChannel >= 95 ? 255 : backgroundAlpha;
            if (a != 255) {
                r = (BYTE)((r * a + 127) / 255);
                g = (BYTE)((g * a + 127) / 255);
                b = (BYTE)((b * a + 127) / 255);
            }
            row[x] = ((std::uint32_t)a << 24) | ((std::uint32_t)r << 16) | ((std::uint32_t)g << 8) | b;
        }
    }
}

HBITMAP CreateLayerBitmap(HDC dc, int width, int height, void** bits)
{
    BITMAPINFO info{};
    info.bmiHeader.biSize = sizeof(info.bmiHeader);
    info.bmiHeader.biWidth = width;
    info.bmiHeader.biHeight = -height;
    info.bmiHeader.biPlanes = 1;
    info.bmiHeader.biBitCount = 32;
    info.bmiHeader.biCompression = BI_RGB;
    return CreateDIBSection(dc, &info, DIB_RGB_COLORS, bits, nullptr, 0);
}

void StrokeRound(HDC dc, RECT r, int radius, COLORREF color)
{
    HBRUSH hollow = (HBRUSH)GetStockObject(HOLLOW_BRUSH);
    HPEN pen = CreatePen(PS_SOLID, 1, color);
    HGDIOBJ oldBrush = SelectObject(dc, hollow);
    HGDIOBJ oldPen = SelectObject(dc, pen);
    RoundRect(dc, r.left, r.top, r.right, r.bottom, radius, radius);
    SelectObject(dc, oldPen);
    SelectObject(dc, oldBrush);
    DeleteObject(pen);
}

std::wstring CompactStatus(const std::wstring& status)
{
    if (status.empty()) return L"翻译引擎准备就绪";
    return status;
}

std::wstring LowerCopy(std::wstring value)
{
    std::transform(value.begin(), value.end(), value.begin(),
        [](wchar_t ch) { return (wchar_t)towlower(ch); });
    return value;
}

bool ParseHotkey(const std::wstring& hotkey, UINT& modifiers, UINT& vk)
{
    modifiers = 0;
    vk = 0;
    std::wstring token;
    std::vector<std::wstring> parts;
    for (wchar_t ch : hotkey) {
        if (ch == L'+' || ch == L' ' || ch == L'\t' || ch == L'-') {
            token = text::Trim(token);
            if (!token.empty()) parts.push_back(token);
            token.clear();
        } else {
            token.push_back((wchar_t)towupper(ch));
        }
    }
    token = text::Trim(token);
    if (!token.empty()) parts.push_back(token);

    for (const auto& part : parts) {
        if (part == L"CTRL" || part == L"CONTROL") modifiers |= MOD_CONTROL;
        else if (part == L"SHIFT") modifiers |= MOD_SHIFT;
        else if (part == L"ALT") modifiers |= MOD_ALT;
        else if (part == L"WIN" || part == L"META") modifiers |= MOD_WIN;
        else if (part.size() == 1 && part[0] >= L'A' && part[0] <= L'Z') vk = (UINT)part[0];
        else if (part.size() == 1 && part[0] >= L'0' && part[0] <= L'9') vk = (UINT)part[0];
        else if (part.size() >= 2 && part[0] == L'F') {
            try {
                int n = std::stoi(part.substr(1));
                if (n >= 1 && n <= 24) vk = VK_F1 + (UINT)n - 1;
            } catch (...) {
                return false;
            }
        } else if (part == L"INSERT" || part == L"INS") vk = VK_INSERT;
        else if (part == L"DELETE" || part == L"DEL") vk = VK_DELETE;
        else if (part == L"HOME") vk = VK_HOME;
        else if (part == L"END") vk = VK_END;
        else if (part == L"PAGEUP" || part == L"PGUP") vk = VK_PRIOR;
        else if (part == L"PAGEDOWN" || part == L"PGDN") vk = VK_NEXT;
        else if (part == L"UP") vk = VK_UP;
        else if (part == L"DOWN") vk = VK_DOWN;
        else if (part == L"LEFT") vk = VK_LEFT;
        else if (part == L"RIGHT") vk = VK_RIGHT;
        else if (part == L"SPACE") vk = VK_SPACE;
        else if (part == L"TAB") vk = VK_TAB;
        else if (part == L"ESC" || part == L"ESCAPE") vk = VK_ESCAPE;
        else return false;
    }

    return modifiers != 0 && vk != 0;
}

int TextWidth(HDC dc, HFONT font, const std::wstring& text)
{
    if (text.empty()) return 0;
    HFONT old = (HFONT)SelectObject(dc, font);
    SIZE size{};
    GetTextExtentPoint32W(dc, text.c_str(), (int)text.size(), &size);
    SelectObject(dc, old);
    return size.cx;
}

int AverageCharWidth(HDC dc, HFONT font)
{
    HFONT old = (HFONT)SelectObject(dc, font);
    TEXTMETRICW tm{};
    GetTextMetricsW(dc, &tm);
    SelectObject(dc, old);
    return (std::max)(6, (int)tm.tmAveCharWidth);
}

std::wstring BreakLongRuns(const std::wstring& text, int maxRun)
{
    if (text.empty()) return text;
    std::wstring out;
    out.reserve(text.size() + text.size() / 24);
    int run = 0;
    for (wchar_t ch : text) {
        out.push_back(ch);
        bool asciiToken = (ch >= L'a' && ch <= L'z') || (ch >= L'A' && ch <= L'Z') ||
            (ch >= L'0' && ch <= L'9');
        if (!asciiToken) {
            run = 0;
            continue;
        }
        ++run;
        if (run >= maxRun) {
            out.push_back(L' ');
            run = 0;
        }
    }
    return out;
}

std::wstring LayoutText(HDC dc, HFONT font, const std::wstring& text, int width)
{
    int maxRun = (std::max)(10, width / AverageCharWidth(dc, font) - 2);
    return BreakLongRuns(text, maxRun);
}

int WrappedTextHeight(HDC dc, HFONT font, const std::wstring& text, int width, int lineHeight, int maxLines)
{
    if (width <= 0 || text.empty()) return lineHeight;
    std::wstring display = LayoutText(dc, font, text, width);
    RECT measure{ 0, 0, width, 0 };
    HFONT old = (HFONT)SelectObject(dc, font);
    DrawTextW(dc, display.c_str(), -1, &measure,
        DT_CALCRECT | DT_WORDBREAK | DT_EDITCONTROL | DT_NOPREFIX);
    SelectObject(dc, old);

    int measured = measure.bottom - measure.top;
    int maxHeight = lineHeight * (std::max)(1, maxLines);
    return (std::max)(lineHeight, (std::min)(measured, maxHeight));
}

void DrawWrappedText(HDC dc, HFONT font, COLORREF color, const std::wstring& text,
    RECT r, int maxLines, int lineHeight)
{
    std::wstring display = LayoutText(dc, font, text, r.right - r.left);
    int maxHeight = lineHeight * (std::max)(1, maxLines);
    r.bottom = (std::min)(r.bottom, r.top + maxHeight);

    HFONT old = (HFONT)SelectObject(dc, font);
    SetTextColor(dc, color);
    DrawTextW(dc, display.c_str(), -1, &r,
        DT_LEFT | DT_TOP | DT_WORDBREAK | DT_EDITCONTROL | DT_END_ELLIPSIS | DT_NOPREFIX);
    SelectObject(dc, old);
}

void DrawSearchIcon(HDC dc, RECT r, COLORREF color)
{
    HPEN pen = CreatePen(PS_SOLID, 2, color);
    HGDIOBJ oldPen = SelectObject(dc, pen);
    HGDIOBJ oldBrush = SelectObject(dc, GetStockObject(HOLLOW_BRUSH));

    int cx = r.left + (r.right - r.left) / 2 - 1;
    int cy = r.top + (r.bottom - r.top) / 2 - 1;
    int iconW = (int)(r.right - r.left);
    int iconH = (int)(r.bottom - r.top);
    int radius = (std::max)(4, (std::min)(iconW, iconH) / 4);
    Ellipse(dc, cx - radius, cy - radius, cx + radius, cy + radius);
    MoveToEx(dc, cx + radius - 1, cy + radius - 1, nullptr);
    LineTo(dc, cx + radius + 5, cy + radius + 5);

    SelectObject(dc, oldBrush);
    SelectObject(dc, oldPen);
    DeleteObject(pen);
}
}

ChatPanel::ChatPanel()
{
}

ChatPanel::~ChatPanel()
{
    Close();
}

bool ChatPanel::Open(HINSTANCE instance, const RuntimeConfig& runtime, const std::wstring& windowStatePath)
{
    instance_ = instance;
    uiThreadId_ = GetCurrentThreadId();
    windowStatePath_ = windowStatePath;

    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = ChatPanel::WindowProc;
    wc.hInstance = instance_;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.lpszClassName = kClass;
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.hbrBackground = nullptr;
    RegisterClassExW(&wc);

    ApplyRuntime(runtime);

    RECT startRect = LoadWindowRect(windowStatePath_);
    int w = startRect.right - startRect.left;
    int h = startRect.bottom - startRect.top;

    hwnd_ = CreateWindowExW(WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_LAYERED,
        kClass, L"ETS2 Chat Translator", WS_POPUP,
        startRect.left, startRect.top, w, h, nullptr, nullptr, instance_, this);
    if (!hwnd_) return false;

    overlayOpacity_ = (std::max)(0, (std::min)(100, runtime.overlayOpacity));
    SetOverlayHotkey(runtime.overlayHotkey);

    ShowWindow(hwnd_, SW_SHOW);
    RenderLayered();
    return true;
}

void ChatPanel::ApplyRuntime(const RuntimeConfig& runtime)
{
    int nextFontSize = (std::max)(12, (std::min)(28, runtime.fontSize));

    HFONT nextFont = CreateFontW(nextFontSize, 0, 0, 0, FW_MEDIUM, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
        OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Microsoft YaHei UI");
    HFONT nextSmallFont = CreateFontW((std::max)(11, nextFontSize - 2), 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
        OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Microsoft YaHei UI");
    HFONT nextTitleFont = CreateFontW(nextFontSize + 2, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
        OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Microsoft YaHei UI");

    if (!nextFont || !nextSmallFont || !nextTitleFont) {
        if (nextFont) DeleteObject(nextFont);
        if (nextSmallFont) DeleteObject(nextSmallFont);
        if (nextTitleFont) DeleteObject(nextTitleFont);
        return;
    }

    if (font_) DeleteObject(font_);
    if (smallFont_) DeleteObject(smallFont_);
    if (titleFont_) DeleteObject(titleFont_);
    font_ = nextFont;
    smallFont_ = nextSmallFont;
    titleFont_ = nextTitleFont;

    fontSize_ = nextFontSize;
    overlayOpacity_ = (std::max)(0, (std::min)(100, runtime.overlayOpacity));
    rowH_ = (std::max)(18, fontSize_ + 7);
    subRowH_ = (std::max)(16, fontSize_ + 5);
    topBand_ = (std::max)(44, fontSize_ + 31);
    statusBand_ = (std::max)(26, fontSize_ + 13);

    SetOverlayHotkey(runtime.overlayHotkey);

    if (hwnd_) {
        RECT rc{};
        GetClientRect(hwnd_, &rc);
        LayoutSearchBox(rc);
        ScrollToEnd();
        RenderLayered();
    }
}

void ChatPanel::Close()
{
    if (hwnd_) {
        SaveWindowState();
        if (hotkeyRegistered_) {
            UnregisterHotKey(hwnd_, hotkeyId_);
            hotkeyRegistered_ = false;
        }
        DestroyWindow(hwnd_);
        hwnd_ = nullptr;
    }
    if (font_) DeleteObject(font_);
    if (smallFont_) DeleteObject(smallFont_);
    if (titleFont_) DeleteObject(titleFont_);
    font_ = smallFont_ = titleFont_ = nullptr;
}

void ChatPanel::MessageLoop()
{
    MSG msg{};
    while (GetMessageW(&msg, nullptr, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
}

bool ChatPanel::SetOverlayHotkey(const std::wstring& hotkey)
{
    std::wstring value = text::Trim(hotkey.empty() ? L"Ctrl+Shift+T" : hotkey);
    {
        std::lock_guard<std::mutex> guard(lock_);
        overlayHotkey_ = value;
    }
    if (!hwnd_) return false;

    if (uiThreadId_ != 0 && GetCurrentThreadId() != uiThreadId_) {
        PostMessageW(hwnd_, WM_APP + 4, 0, 0);
        return true;
    }

    if (hotkeyRegistered_) {
        UnregisterHotKey(hwnd_, hotkeyId_);
        hotkeyRegistered_ = false;
    }

    UINT modifiers = 0;
    UINT vk = 0;
    if (!ParseHotkey(value, modifiers, vk)) return false;

    hotkeyRegistered_ = RegisterHotKey(hwnd_, hotkeyId_, modifiers | MOD_NOREPEAT, vk) != FALSE;
    return hotkeyRegistered_;
}

unsigned int ChatPanel::Push(ChatEntry entry)
{
    unsigned int id;
    {
        std::lock_guard<std::mutex> guard(lock_);
        id = nextId_++;
        entry.id = id;
        pendingEntries_.push(std::move(entry));
    }

    if (hwnd_) {
        PostMessageW(hwnd_, WM_APP + 3, 0, 0);
    }
    return id;
}

void ChatPanel::PatchTranslation(unsigned int id, const std::wstring& text)
{
    {
        std::lock_guard<std::mutex> guard(lock_);
        pendingTranslations_.push({ id, text });
    }
    if (hwnd_) {
        PostMessageW(hwnd_, WM_APP + 3, 0, 0);
    }
}

void ChatPanel::Status(const std::wstring& text)
{
    {
        std::lock_guard<std::mutex> guard(lock_);
        status_ = text;
    }
    if (hwnd_) RenderLayered();
}

void ChatPanel::ToggleVisible()
{
    if (!hwnd_) return;
    if (IsWindowVisible(hwnd_)) {
        ShowWindow(hwnd_, SW_HIDE);
        return;
    }

    ShowWindow(hwnd_, SW_SHOWNA);
    SetWindowPos(hwnd_, HWND_TOPMOST, 0, 0, 0, 0,
        SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE | SWP_SHOWWINDOW);
    RenderLayered();
}

void ChatPanel::SaveWindowState() const
{
    if (!hwnd_ || windowStatePath_.empty()) return;
    RECT rect{};
    if (!GetWindowRect(hwnd_, &rect)) return;
    rect = ClampWindowRect(rect);
    std::ofstream f(windowStatePath_, std::ios::binary);
    if (!f) return;
    f << "{\n"
      << "  \"x\": " << rect.left << ",\n"
      << "  \"y\": " << rect.top << ",\n"
      << "  \"width\": " << (rect.right - rect.left) << ",\n"
      << "  \"height\": " << (rect.bottom - rect.top) << "\n"
      << "}\n";
}

LRESULT CALLBACK ChatPanel::WindowProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    ChatPanel* self = nullptr;
    if (msg == WM_NCCREATE) {
        auto cs = reinterpret_cast<CREATESTRUCTW*>(lp);
        self = reinterpret_cast<ChatPanel*>(cs->lpCreateParams);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, (LONG_PTR)self);
        if (self) self->hwnd_ = hwnd;
    } else {
        self = reinterpret_cast<ChatPanel*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    }

    if (!self) return DefWindowProcW(hwnd, msg, wp, lp);

    switch (msg) {
    case WM_NCCREATE:
        return TRUE;

    case WM_CREATE: {
        RECT rc{};
        GetClientRect(hwnd, &rc);
        self->LayoutSearchBox(rc);
        self->LayoutComposeBox(rc);
        return 0;
    }

    case WM_PAINT: {
        PAINTSTRUCT ps{};
        BeginPaint(hwnd, &ps);
        EndPaint(hwnd, &ps);
        self->RenderLayered();
        return 0;
    }
    case WM_NCHITTEST: {
        POINT p{ GET_X_LPARAM(lp), GET_Y_LPARAM(lp) };
        ScreenToClient(hwnd, &p);
        RECT rc{};
        GetClientRect(hwnd, &rc);
        if (p.y >= rc.bottom - 12 && p.x >= rc.right - 12) return HTBOTTOMRIGHT;
        if (p.y >= rc.bottom - 8) return HTBOTTOM;
        if (p.x >= rc.right - 8) return HTRIGHT;
        if (self->SearchBoxHit(p.x, p.y)) return HTCLIENT;
        if (p.y < self->topBand_ && p.x < rc.right - 40) return HTCAPTION;
        return HTCLIENT;
    }
    case WM_SIZE: {
        self->ResizeScroll();
        int cx = LOWORD(lp);
        int cy = HIWORD(lp);
        HRGN rgn = CreateRoundRectRgn(0, 0, cx, cy, 16, 16);
        SetWindowRgn(hwnd, rgn, TRUE);
        RECT rc{ 0, 0, cx, cy };
        self->LayoutSearchBox(rc);
        self->LayoutComposeBox(rc);
        self->RenderLayered();
        return 0;
    }
    case WM_EXITSIZEMOVE:
        self->SaveWindowState();
        return 0;
    case WM_CHAR:
    case WM_KEYDOWN:
        if (self->HandleSearchKey(msg, wp)) return 0;
        if (self->HandleComposeKey(msg, wp)) return 0;
        break;
    case WM_SETFOCUS:
        if (self->searchFocused_) self->searchCaretVisible_ = true;
        if (self->composeFocused_) self->composeCaretVisible_ = true;
        self->RenderLayered();
        return 0;
    case WM_KILLFOCUS:
        self->SetSearchFocus(false);
        self->SetComposeFocus(false);
        return 0;
    case WM_MOUSEWHEEL:
        self->OnWheel(GET_WHEEL_DELTA_WPARAM(wp));
        return 0;
    case WM_HOTKEY:
        if ((int)wp == self->hotkeyId_) self->ToggleVisible();
        return 0;
    case WM_LBUTTONDOWN:
        self->OnClick(GET_X_LPARAM(lp), GET_Y_LPARAM(lp));
        return 0;
    case WM_IME_STARTCOMPOSITION:
        if (self->composeFocused_) return self->HandleImeStartComposition();
        break;
    case WM_IME_COMPOSITION:
        if (self->composeFocused_) return self->HandleImeComposition(wp, lp);
        break;
    case WM_IME_ENDCOMPOSITION:
        if (self->composeFocused_) return self->HandleImeEndComposition();
        break;
    case WM_TIMER:
        if (wp == (WPARAM)self->composeTimerId_ && self->composeFocused_) {
            self->composeCaretOn_ = !self->composeCaretOn_;
            self->RenderLayered();
            return 0;
        }
        break;
    case WM_APP + 1:
        self->ScrollToEnd();
        return 0;
    case WM_APP + 3:
        self->DrainPending();
        return 0;
    case WM_APP + 4: {
        std::wstring hotkey;
        {
            std::lock_guard<std::mutex> guard(self->lock_);
            hotkey = self->overlayHotkey_;
        }
        self->SetOverlayHotkey(hotkey);
        return 0;
    }
    case WM_ERASEBKGND:
        return 1;
    case WM_CLOSE:
        self->closing_ = true;
        DestroyWindow(hwnd);
        return 0;
    case WM_DESTROY:
        if (self->hotkeyRegistered_) {
            UnregisterHotKey(hwnd, self->hotkeyId_);
            self->hotkeyRegistered_ = false;
        }
        self->hwnd_ = nullptr;
        if (self->closing_) PostQuitMessage(0);
        return 0;
    case WM_NCDESTROY:
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, 0);
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

void ChatPanel::DrainPending()
{
    {
        std::lock_guard<std::mutex> guard(lock_);
        while (!pendingEntries_.empty()) {
            entries_.push_back(std::move(pendingEntries_.front()));
            pendingEntries_.pop();
        }
        while (!pendingTranslations_.empty()) {
            auto item = std::move(pendingTranslations_.front());
            pendingTranslations_.pop();
            for (auto it = entries_.rbegin(); it != entries_.rend(); ++it) {
                if (it->id == item.first) {
                    it->translated = item.second;
                    break;
                }
            }
        }
        if (entries_.size() > 1800) {
            int removeSearchOnly = 400;
            entries_.erase(std::remove_if(entries_.begin(), entries_.end(), [&](const ChatEntry& entry) {
                if (removeSearchOnly <= 0 || !entry.searchOnly) return false;
                --removeSearchOnly;
                return true;
            }), entries_.end());
        }
        if (entries_.size() > 900) entries_.erase(entries_.begin(), entries_.begin() + 180);
    }
    RenderLayered();
    if (follow_) PostMessageW(hwnd_, WM_APP + 1, 0, 0);
}

void ChatPanel::RenderLayered()
{
    if (!hwnd_) return;
    RECT rc{};
    if (!GetClientRect(hwnd_, &rc)) return;
    int width = rc.right - rc.left;
    int height = rc.bottom - rc.top;
    if (width <= 0 || height <= 0) return;

    HDC screen = GetDC(nullptr);
    HDC mem = CreateCompatibleDC(screen);
    void* bits = nullptr;
    HBITMAP bmp = CreateLayerBitmap(screen, width, height, &bits);
    if (!bmp || !bits) {
        if (bmp) DeleteObject(bmp);
        DeleteDC(mem);
        ReleaseDC(nullptr, screen);
        return;
    }

    HBITMAP old = (HBITMAP)SelectObject(mem, bmp);
    Paint(mem, RECT{ 0, 0, width, height });
    PrepareLayeredBitmap(bmp, width, height, BackgroundAlpha(overlayOpacity_));

    POINT src{ 0, 0 };
    POINT pos{};
    RECT wr{};
    GetWindowRect(hwnd_, &wr);
    pos.x = wr.left;
    pos.y = wr.top;
    SIZE size{ width, height };
    BLENDFUNCTION blend{ AC_SRC_OVER, 0, 255, AC_SRC_ALPHA };
    UpdateLayeredWindow(hwnd_, screen, &pos, &size, mem, &src, 0, &blend, ULW_ALPHA);

    SelectObject(mem, old);
    DeleteObject(bmp);
    DeleteDC(mem);
    ReleaseDC(nullptr, screen);
}

void ChatPanel::Paint(HDC dc, RECT bounds)
{
    SetBkMode(dc, TRANSPARENT);
    UpdateContentWidth(bounds.right);
    Fill(dc, bounds, RGB(0, 0, 0));

    RECT outer{ 0, 0, bounds.right, bounds.bottom };
    RoundFill(dc, outer, 18, cPanel);
    StrokeRound(dc, { 0, 0, bounds.right - 1, bounds.bottom - 1 }, 18, RGB(45, 55, 75));

    RECT accent{ 16, 12, 20, topBand_ - 10 };
    RoundFill(dc, accent, 4, cBlue);

    bool hasSearchBox = searchBoxRect_.right > searchBoxRect_.left && searchBoxRect_.bottom > searchBoxRect_.top;
    RECT title{ 30, 0, hasSearchBox ? searchBoxRect_.left - 10 : bounds.right - 120, topBand_ };
    if (title.right < title.left + 92) title.right = title.left + 92;
    DrawTextLine(dc, titleFont_, cText, bounds.right < 520 ? L"TruckersMP" : L"TruckersMP Chat",
        title, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);

    if (hasSearchBox) {
        RoundFill(dc, searchBoxRect_, 7, searchFocused_ ? RGB(13, 22, 34) : RGB(11, 16, 24));
        StrokeRound(dc, searchBoxRect_, 7, searchFocused_ ? cCyan : RGB(52, 64, 86));

        RECT icon{ searchBoxRect_.left + 8, searchBoxRect_.top + 4,
            searchBoxRect_.left + 28, searchBoxRect_.bottom - 4 };
        DrawSearchIcon(dc, icon, searchFocused_ ? cCyan : cDim);

        std::wstring value;
        bool focused = false;
        bool caret = false;
        {
            std::lock_guard<std::mutex> guard(lock_);
            value = searchInputText_;
            focused = searchFocused_;
            caret = searchCaretVisible_;
        }
        RECT inputText{ searchBoxRect_.left + 34, searchBoxRect_.top,
            searchBoxRect_.right - 10, searchBoxRect_.bottom };
        if (value.empty()) {
            DrawTextLine(dc, smallFont_, RGB(105, 118, 138), L"搜索", inputText,
                DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
        } else {
            DrawTextLine(dc, smallFont_, cText, value, inputText,
                DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
            if (focused && caret) {
                int textW = TextWidth(dc, smallFont_, value);
                int x = (std::min)(inputText.left + textW + 2, inputText.right - 2);
                RECT cursor{ x, inputText.top + 6, x + 1, inputText.bottom - 6 };
                Fill(dc, cursor, cCyan);
            }
        }
    }

    RECT tag{ bounds.right - 118, 14, bounds.right - 52, topBand_ - 12 };
    RoundFill(dc, tag, 12, RGB(22, 42, 62));
    DrawTextLine(dc, smallFont_, cCyan, L"LIVE", tag, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

    RECT close{ bounds.right - 42, 10, bounds.right - 14, topBand_ - 10 };
    RoundFill(dc, close, 8, RGB(35, 42, 56));
    DrawTextLine(dc, titleFont_, RGB(175, 185, 200), L"\x2715", close, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

    std::wstring status;
    {
        std::lock_guard<std::mutex> guard(lock_);
        status = status_;
        if (!searchDisplayText_.empty()) {
            status = L"搜索框 / 日志消息 / 日志信息 / 临时编号：\"" + searchDisplayText_ +
                L"\" | 结果 " + std::to_wstring(MatchCountUnlocked()) + L" 条";
        }
    }
    RECT statusBox{ 16, topBand_, bounds.right - 16, topBand_ + statusBand_ - 6 };
    RoundFill(dc, statusBox, 10, RGB(13, 18, 27));
    RECT dot{ statusBox.left + 12, statusBox.top + 10, statusBox.left + 20, statusBox.top + 18 };
    RoundFill(dc, dot, 8, RGB(16, 185, 129));
    RECT statusText{ statusBox.left + 28, statusBox.top, statusBox.right - 12, statusBox.bottom };
    DrawTextLine(dc, smallFont_, cDim, CompactStatus(status), statusText, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);

    bool hasComposeBox = composeBoxRect_.right > composeBoxRect_.left && composeBoxRect_.bottom > composeBoxRect_.top;
    int areaBottom = hasComposeBox ? (composeBoxRect_.top - 4) : (bounds.bottom - 8);
    if (areaBottom < topBand_ + statusBand_ + 20) areaBottom = bounds.bottom - 8;
    RECT area{ 0, topBand_ + statusBand_, bounds.right, areaBottom };
    HRGN clip = CreateRectRgn(area.left, area.top, area.right, area.bottom);
    SelectClipRgn(dc, clip);

    int y = area.top + 4 - scroll_;
    int left = 12;
    int right = bounds.right - 16;
    {
        std::lock_guard<std::mutex> guard(lock_);
        int visibleIndex = 0;
        for (size_t i = 0; i < entries_.size(); ++i) {
            const auto& e = entries_[i];
            if (!EntryMatches(e)) continue;
            int h = EntryHeight(dc, e);
            if (y > bounds.bottom) break;
            if (y + h >= area.top) {
                if (e.serviceLine || e.infoLine) {
                    RECT card{ left, y + 1, right, y + h - 3 };
                    RoundFill(dc, card, 7, e.infoLine ? RGB(20, 34, 47) : RGB(44, 26, 32));
                    StrokeRound(dc, card, 7, e.infoLine ? RGB(38, 70, 92) : RGB(82, 42, 50));
                    RECT r{ card.left + 10, card.top + 4, card.right - 10, card.bottom - 4 };
                    if (e.infoLine) {
                        DrawWrappedText(dc, smallFont_, RGB(125, 211, 252), L"[" + e.time + L"] " + e.body, r, 2, subRowH_);
                    } else {
                        DrawTextLine(dc, smallFont_, cWarn, L"[" + e.time + L"] " + e.body, r,
                            DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
                    }
                } else {
                    RECT card{ left, y + 1, right, y + h - 3 };
                    RoundFill(dc, card, 7, (visibleIndex % 2 == 0) ? cCard : cCardAlt);
                    StrokeRound(dc, card, 7, RGB(39, 48, 65));

                    RECT timeRc{ card.left + 10, card.top + 7, card.left + 10 + kTimeColumnW, card.top + 7 + rowH_ };
                    DrawTextLine(dc, smallFont_, cTime, e.time, timeRc, DT_LEFT | DT_TOP | DT_SINGLELINE);

                    const int baseContentX = card.left + 12 + kTimeColumnW;
                    int contentX = baseContentX;
                    int contentRight = card.right - 12;
                    int lineTop = card.top + 6;
                    if (!e.author.empty()) {
                        std::wstring name = e.author + L":";
                        int nameWidth = TextWidth(dc, smallFont_, name);
                        RECT nameRc{ contentX, lineTop + 1, contentRight, lineTop + 1 + subRowH_ };
                        DrawTextLine(dc, smallFont_, cName, name, nameRc, DT_LEFT | DT_TOP | DT_SINGLELINE | DT_END_ELLIPSIS);
                        contentX += nameWidth + 6;
                        if (contentX > contentRight - 120) {
                            contentX = baseContentX;
                            lineTop += subRowH_ - 4;
                        }
                    }

                    const std::wstring primary = e.translated.empty() ? L"翻译中..." : e.translated;
                    int primaryH = WrappedTextHeight(dc, font_, primary, contentRight - contentX, rowH_, 4);
                    RECT primaryRc{ contentX, lineTop, contentRight, lineTop + primaryH };
                    DrawWrappedText(dc, font_, e.translated.empty() ? cDim : cTrans, primary, primaryRc, 4, rowH_);
                }
            }
            y += h;
            ++visibleIndex;
        }
    }

    SelectClipRgn(dc, nullptr);
    DeleteObject(clip);

    if (hasComposeBox) {
        RoundFill(dc, composeBoxRect_, 8, composeFocused_ ? RGB(13, 22, 34) : RGB(11, 16, 24));
        StrokeRound(dc, composeBoxRect_, 8, composeFocused_ ? cCyan : RGB(52, 64, 86));

        std::wstring value;
        std::wstring status;
        bool focused = false;
        bool caret = false;
        bool caretOn = false;
        std::wstring imeComp;
        int cursorPos = 0;
        {
            std::lock_guard<std::mutex> guard(lock_);
            value = composeInputText_;
            status = composeStatus_;
            focused = composeFocused_;
            caret = composeCaretVisible_;
            caretOn = composeCaretOn_;
            imeComp = composeImeComp_;
            cursorPos = composeCursorPos_;
        }

        RECT textRc{ composeBoxRect_.left + 12, composeBoxRect_.top,
            composeBoxRect_.right - 12, composeBoxRect_.bottom };
        if (!status.empty()) {
            DrawTextLine(dc, smallFont_, cCyan, status, textRc,
                DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
        } else if (value.empty() && imeComp.empty()) {
            DrawTextLine(dc, smallFont_, RGB(105, 118, 138), L"输入中文，回车翻译并发送到游戏聊天...", textRc,
                DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
        } else {
            // Build display text: text before cursor + IME comp + text after cursor
            std::wstring before = value.substr(0, cursorPos);
            std::wstring after = value.substr(cursorPos);
            std::wstring display = before + imeComp + after;

            // Draw the full text
            RECT displayRc = textRc;
            DrawTextLine(dc, smallFont_, cText, display, displayRc,
                DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);

            if (focused) {
                // Calculate cursor X position (before text + IME comp start)
                std::wstring beforeCursor = before;
                int cursorX = TextWidth(dc, smallFont_, beforeCursor);
                int caretX = textRc.left + cursorX;

                // Draw IME composition underline if active
                if (!imeComp.empty()) {
                    int compW = TextWidth(dc, smallFont_, imeComp);
                    int compLeft = textRc.left + TextWidth(dc, smallFont_, before);
                    int compRight = (std::min)(compLeft + compW, (int)(textRc.right - 2));
                    int underlineY = textRc.bottom - 4;
                    HPEN pen = CreatePen(PS_SOLID, 1, cCyan);
                    HGDIOBJ oldPen = SelectObject(dc, pen);
                    MoveToEx(dc, compLeft, underlineY, nullptr);
                    LineTo(dc, compRight, underlineY);
                    SelectObject(dc, oldPen);
                    DeleteObject(pen);
                }

                // Draw blinking cursor at the correct position
                if (caret && caretOn) {
                    RECT cursorRc{ caretX, textRc.top + 6,
                        caretX + 1, textRc.bottom - 6 };
                    Fill(dc, cursorRc, cCyan);
                }
            }
        }
    }

    int content = ContentHeight(dc);
    int view = area.bottom - area.top;
    if (content > view) {
        int trackTop = area.top + 8;
        int trackBottom = area.bottom - 8;
        RECT track{ bounds.right - 10, trackTop, bounds.right - 6, trackBottom };
        RoundFill(dc, track, 4, RGB(36, 44, 58));
        int thumbH = (std::max)(28, view * (trackBottom - trackTop) / content);
        int maxScroll = (std::max)(1, content - view);
        int thumbTop = trackTop + scroll_ * ((trackBottom - trackTop) - thumbH) / maxScroll;
        RECT thumb{ bounds.right - 10, thumbTop, bounds.right - 6, thumbTop + thumbH };
        RoundFill(dc, thumb, 4, RGB(92, 110, 140));
    }

}

void ChatPanel::UpdateContentWidth(int clientWidth)
{
    int left = 12;
    int right = clientWidth - 16;

    contentWidth_ = (std::max)(120, right - left - 24);
}

void ChatPanel::LayoutSearchBox(RECT bounds)
{
    searchBoxRect_ = {};

    int clientWidth = (int)bounds.right;
    int rightLimit = clientWidth - 138;
    int left = clientWidth >= 520 ? 204 : 142;
    int width = (std::min)(210, (std::max)(118, clientWidth / 3));
    if (clientWidth < 520) width = (std::max)(96, rightLimit - left);
    if (left + width > rightLimit) width = rightLimit - left;
    int height = (std::max)(24, (std::min)(30, topBand_ - 20));
    int top = (std::max)(8, (topBand_ - height) / 2);
    int bottom = top + height;

    if (clientWidth < 360 || rightLimit <= left + 64 || width < 72 || bottom <= top + 16) {
        SetSearchFocus(false);
        return;
    }

    int right = left + width;
    searchBoxRect_ = { left, top, right, bottom };
}

void ChatPanel::LayoutComposeBox(RECT bounds)
{
    composeBoxRect_ = {};

    int clientWidth = (int)bounds.right;
    int clientHeight = (int)bounds.bottom;
    int composeH = (std::max)(36, (std::min)(48, fontSize_ + 24));
    int margin = 10;
    int y = clientHeight - composeH - margin;
    if (y < topBand_ + statusBand_ + 60) {
        composeFocused_ = false;
        composeCaretVisible_ = false;
        return;
    }

    int left = margin + 6;
    int right = clientWidth - margin - 6;
    if (right - left < 120) return;

    composeBoxRect_ = { left, y, right, y + composeH };
}

void ChatPanel::SetSearchText(std::wstring text)
{
    if (text.size() > 80) text.resize(80);
    text = text::Trim(std::move(text));
    std::wstring lowered = LowerCopy(text);
    {
        std::lock_guard<std::mutex> guard(lock_);
        searchInputText_ = text;
        searchDisplayText_ = text;
        searchText_ = std::move(lowered);
    }
    ScrollToEnd();
    RenderLayered();
}

void ChatPanel::SetSearchFocus(bool focused)
{
    bool changed = false;
    {
        std::lock_guard<std::mutex> guard(lock_);
        changed = searchFocused_ != focused || searchCaretVisible_ != focused;
        searchFocused_ = focused;
        searchCaretVisible_ = focused;
    }
    if (changed) RenderLayered();
}

bool ChatPanel::SearchBoxHit(int x, int y) const
{
    return searchBoxRect_.right > searchBoxRect_.left &&
        x >= searchBoxRect_.left && x <= searchBoxRect_.right &&
        y >= searchBoxRect_.top && y <= searchBoxRect_.bottom;
}

bool ChatPanel::HandleSearchKey(UINT msg, WPARAM wp)
{
    if (!searchFocused_) return false;

    if (msg == WM_KEYDOWN) {
        if (wp == VK_BACK) {
            std::wstring next;
            {
                std::lock_guard<std::mutex> guard(lock_);
                next = searchInputText_;
                if (!next.empty()) next.pop_back();
            }
            SetSearchText(next);
            return true;
        }
        if (wp == VK_ESCAPE) {
            if (!searchInputText_.empty()) {
                SetSearchText(L"");
            } else {
                SetSearchFocus(false);
            }
            return true;
        }
        if (wp == VK_RETURN) {
            SetSearchFocus(false);
            return true;
        }
        if (wp == VK_LEFT || wp == VK_RIGHT || wp == VK_UP || wp == VK_DOWN ||
            wp == VK_HOME || wp == VK_END || wp == VK_DELETE || wp == VK_TAB) {
            return true;
        }
        return false;
    }

    if (msg == WM_CHAR) {
        wchar_t ch = (wchar_t)wp;
        if (ch < 0x20 || ch == 0x7F) return true;
        std::wstring next;
        {
            std::lock_guard<std::mutex> guard(lock_);
            next = searchInputText_;
            next.push_back(ch);
        }
        SetSearchText(next);
        return true;
    }

    return false;
}

bool ChatPanel::EntryMatches(const ChatEntry& entry) const
{
    if (searchText_.empty()) return !entry.searchOnly;
    std::wstring haystack;
    haystack.reserve(entry.time.size() + entry.channel.size() + entry.author.size() +
        entry.body.size() + entry.translated.size() + 8);
    haystack.append(entry.time).push_back(L' ');
    haystack.append(entry.channel).push_back(L' ');
    haystack.append(entry.author).push_back(L' ');
    haystack.append(entry.body).push_back(L' ');
    haystack.append(entry.translated);
    return LowerCopy(std::move(haystack)).find(searchText_) != std::wstring::npos;
}

int ChatPanel::MatchCountUnlocked() const
{
    int count = 0;
    for (const auto& e : entries_) {
        if (EntryMatches(e)) ++count;
    }
    return count;
}

int ChatPanel::EntryHeight(HDC dc, const ChatEntry& entry) const
{
    if (entry.serviceLine) return rowH_ + 8;
    if (entry.infoLine) return subRowH_ * 2 + 12;
    int textWidth = (std::max)(80, contentWidth_ - kTimeColumnW - 10);
    int h = 13;
    if (!entry.author.empty()) {
        int nameWidth = TextWidth(dc, smallFont_, entry.author + L":");
        if (nameWidth + 6 > textWidth - 120) h += subRowH_ - 4;
    }
    const std::wstring primary = entry.translated.empty() ? L"翻译中..." : entry.translated;
    h += WrappedTextHeight(dc, font_, primary, textWidth, rowH_, 4);
    h += 8;
    return (std::max)(rowH_ + 14, h);
}

int ChatPanel::ContentHeight(HDC dc) const
{
    std::lock_guard<std::mutex> guard(lock_);
    return ContentHeightUnlocked(dc);
}

int ChatPanel::ContentHeightUnlocked(HDC dc) const
{
    int total = 8;
    for (const auto& e : entries_) {
        if (!EntryMatches(e)) continue;
        total += EntryHeight(dc, e);
    }
    return total;
}

void ChatPanel::ResizeScroll()
{
    // No native scrollbar in the simple overlay. Mouse wheel still scrolls.
}

void ChatPanel::ScrollToEnd()
{
    if (!hwnd_) return;
    RECT rc{};
    GetClientRect(hwnd_, &rc);
    UpdateContentWidth(rc.right);
    int view = rc.bottom - topBand_ - statusBand_ - 8;
    HDC dc = GetDC(hwnd_);
    {
        std::lock_guard<std::mutex> guard(lock_);
        scroll_ = (std::max)(0, ContentHeightUnlocked(dc) - view);
    }
    ReleaseDC(hwnd_, dc);
    follow_ = true;
    ResizeScroll();
    RenderLayered();
}

void ChatPanel::OnWheel(int delta)
{
    if (!hwnd_) return;
    scroll_ -= (delta / WHEEL_DELTA) * rowH_ * 3;
    RECT rc{};
    GetClientRect(hwnd_, &rc);
    UpdateContentWidth(rc.right);
    int view = rc.bottom - topBand_ - statusBand_ - 8;
    HDC dc = GetDC(hwnd_);
    {
        std::lock_guard<std::mutex> guard(lock_);
        int content = ContentHeightUnlocked(dc);
        int maxScroll = (std::max)(0, content - view);
        scroll_ = (std::max)(0, (std::min)(scroll_, maxScroll));
        follow_ = scroll_ >= maxScroll - rowH_;
    }
    ReleaseDC(hwnd_, dc);
    ResizeScroll();
    RenderLayered();
}

void ChatPanel::OnClick(int x, int y)
{
    RECT rc{};
    GetClientRect(hwnd_, &rc);
    if (SearchBoxHit(x, y)) {
        SetFocus(hwnd_);
        SetSearchFocus(true);
        SetComposeFocus(false);
        return;
    }
    if (ComposeBoxHit(x, y)) {
        SetFocus(hwnd_);
        SetComposeFocus(true);
        SetSearchFocus(false);
        return;
    }
    if (searchFocused_) SetSearchFocus(false);
    if (composeFocused_) SetComposeFocus(false);
    if (y >= 6 && y <= topBand_ - 6 && x >= rc.right - 34 && x <= rc.right - 10) {
        if (closeButtonExits_) {
            closing_ = true;
            DestroyWindow(hwnd_);
        } else {
            ShowWindow(hwnd_, SW_HIDE);
        }
    }
}

void ChatPanel::SetComposeStatus(const std::wstring& text)
{
    {
        std::lock_guard<std::mutex> guard(lock_);
        composeStatus_ = text;
    }
    if (hwnd_) RenderLayered();
}

void ChatPanel::SetComposeFocus(bool focused)
{
    bool changed = false;
    {
        std::lock_guard<std::mutex> guard(lock_);
        changed = composeFocused_ != focused || composeCaretVisible_ != focused;
        composeFocused_ = focused;
        composeCaretVisible_ = focused;
        if (focused) {
            composeCaretOn_ = true;
        } else {
            composeCaretOn_ = false;
            composeImeComp_.clear();
        }
    }
    if (focused) {
        StartComposeCaret();
    } else {
        StopComposeCaret();
    }
    if (changed) RenderLayered();
}

bool ChatPanel::ComposeBoxHit(int x, int y) const
{
    return composeBoxRect_.right > composeBoxRect_.left &&
        x >= composeBoxRect_.left && x <= composeBoxRect_.right &&
        y >= composeBoxRect_.top && y <= composeBoxRect_.bottom;
}

bool ChatPanel::HandleComposeKey(UINT msg, WPARAM wp)
{
    if (!composeFocused_) return false;

    if (msg == WM_KEYDOWN) {
        if (wp == VK_LEFT) {
            {
                std::lock_guard<std::mutex> guard(lock_);
                if (composeCursorPos_ > 0) --composeCursorPos_;
            }
            RenderLayered();
            return true;
        }
        if (wp == VK_RIGHT) {
            {
                std::lock_guard<std::mutex> guard(lock_);
                if (composeCursorPos_ < (int)composeInputText_.size()) ++composeCursorPos_;
            }
            RenderLayered();
            return true;
        }
        if (wp == VK_HOME) {
            {
                std::lock_guard<std::mutex> guard(lock_);
                composeCursorPos_ = 0;
            }
            RenderLayered();
            return true;
        }
        if (wp == VK_END) {
            {
                std::lock_guard<std::mutex> guard(lock_);
                composeCursorPos_ = (int)composeInputText_.size();
            }
            RenderLayered();
            return true;
        }
        if (wp == VK_DELETE) {
            {
                std::lock_guard<std::mutex> guard(lock_);
                if (composeCursorPos_ < (int)composeInputText_.size()) {
                    composeInputText_.erase(composeCursorPos_, 1);
                }
            }
            RenderLayered();
            return true;
        }
        if (wp == VK_BACK) {
            {
                std::lock_guard<std::mutex> guard(lock_);
                if (composeCursorPos_ > 0) {
                    composeInputText_.erase(composeCursorPos_ - 1, 1);
                    --composeCursorPos_;
                }
            }
            RenderLayered();
            return true;
        }
        if (wp == VK_ESCAPE) {
            {
                std::lock_guard<std::mutex> guard(lock_);
                composeInputText_.clear();
                composeCursorPos_ = 0;
            }
            SetComposeFocus(false);
            RenderLayered();
            return true;
        }
        if (wp == VK_RETURN) {
            std::wstring text;
            ComposeCallback cb;
            HWND hwnd = nullptr;
            {
                std::lock_guard<std::mutex> guard(lock_);
                text = text::Trim(composeInputText_);
                cb = composeCallback_;
                composeInputText_.clear();
                composeCursorPos_ = 0;
                composeStatus_ = L"";
                hwnd = hwnd_;
            }
            SetComposeFocus(false);
            // Release focus so the game can receive keyboard simulation
            if (hwnd && GetFocus() == hwnd) SetFocus(nullptr);
            if (!text.empty() && cb) {
                cb(text);
            }
            RenderLayered();
            return true;
        }
        if (wp == VK_UP || wp == VK_DOWN || wp == VK_TAB) {
            return true;
        }
        return false;
    }

    if (msg == WM_CHAR) {
        wchar_t ch = (wchar_t)wp;
        if (ch < 0x20 || ch == 0x7F) return true;
        {
            std::lock_guard<std::mutex> guard(lock_);
            if (composeInputText_.size() < 200) {
                composeInputText_.insert(composeCursorPos_, 1, ch);
                ++composeCursorPos_;
            }
        }
        RenderLayered();
        return true;
    }

    return false;
}

void ChatPanel::StartComposeCaret()
{
    if (composeTimerId_) return;
    composeCaretOn_ = true;
    composeTimerId_ = SetTimer(hwnd_, 1, 530, nullptr);
}

void ChatPanel::StopComposeCaret()
{
    if (composeTimerId_) {
        KillTimer(hwnd_, composeTimerId_);
        composeTimerId_ = 0;
    }
    composeCaretOn_ = false;
}

RECT ChatPanel::ComposeCaretRect(HDC dc) const
{
    if (!composeFocused_) return{};
    std::wstring before = composeInputText_.substr(0, composeCursorPos_);
    int cursorX = TextWidth(dc, smallFont_, before);
    RECT textRc{ composeBoxRect_.left + 12, composeBoxRect_.top,
        composeBoxRect_.right - 12, composeBoxRect_.bottom };
    return{ textRc.left + cursorX, textRc.top + 6,
        textRc.left + cursorX + 1, textRc.bottom - 6 };
}

LRESULT ChatPanel::HandleImeStartComposition()
{
    HIMC himc = ImmGetContext(hwnd_);
    if (himc) {
        // Set composition window position near the caret
        COMPOSITIONFORM cf = {};
        RECT textRc{ composeBoxRect_.left + 12, composeBoxRect_.top,
            composeBoxRect_.right - 12, composeBoxRect_.bottom };
        cf.dwStyle = CFS_POINT;
        cf.ptCurrentPos.x = textRc.left;
        cf.ptCurrentPos.y = textRc.top;
        ImmSetCompositionWindow(himc, &cf);
        ImmReleaseContext(hwnd_, himc);
    }
    return TRUE;
}

LRESULT ChatPanel::HandleImeComposition(WPARAM wp, LPARAM lp)
{
    HIMC himc = ImmGetContext(hwnd_);
    if (!himc) return TRUE;

    if (lp & GCS_COMPSTR) {
        // Get the composition string (what the IME is currently composing)
        LONG len = ImmGetCompositionStringW(himc, GCS_COMPSTR, nullptr, 0);
        if (len > 0) {
            std::wstring comp(len / sizeof(wchar_t), L'\0');
            ImmGetCompositionStringW(himc, GCS_COMPSTR, &comp[0], len);
            {
                std::lock_guard<std::mutex> guard(lock_);
                composeImeComp_ = comp;
            }
        }
        RenderLayered();
    }

    if (lp & GCS_RESULTSTR) {
        // Get the finalized string from IME
        LONG len = ImmGetCompositionStringW(himc, GCS_RESULTSTR, nullptr, 0);
        if (len > 0) {
            std::wstring result(len / sizeof(wchar_t), L'\0');
            ImmGetCompositionStringW(himc, GCS_RESULTSTR, &result[0], len);
            {
                std::lock_guard<std::mutex> guard(lock_);
                composeInputText_.insert(composeCursorPos_, result);
                composeCursorPos_ += (int)result.size();
                composeImeComp_.clear();
            }
        } else {
            std::lock_guard<std::mutex> guard(lock_);
            composeImeComp_.clear();
        }
        RenderLayered();
    }

    ImmReleaseContext(hwnd_, himc);
    return TRUE;
}

LRESULT ChatPanel::HandleImeEndComposition()
{
    {
        std::lock_guard<std::mutex> guard(lock_);
        composeImeComp_.clear();
    }
    RenderLayered();
    return TRUE;
}
