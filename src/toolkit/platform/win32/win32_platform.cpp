#include "win32_platform.hpp"
#include "Win32OpenGlRenderingBackend.hpp"
#include "toolkit/lunasvg_image_loader.hpp"
#include "toolkit/painters/win32_painter.hpp"
#include "toolkit/theme.hpp"
#include "toolkit/window.hpp"
#include "win32_image_loader.hpp"

// clang-format off
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <objidl.h>
#include <gdiplus.h>
#include <Dwmapi.h>
#pragma comment(lib, "dwmapi.lib")
// clang-format on

#include <GL/gl.h>
#include <algorithm>
#include <cmath>
#include <memory>
#include <objidl.h>
#include <spdlog/spdlog.h>
#include <sstream>

namespace toolkit {

// NOTE: usualy code in this library tends to use `auto` for variables. However
//       for this module in particular, I think deviating from this gui and using
//       explicit Win32 types is the best. This will make it easier to find online
//       code and port exising snippets from C. This is how the platform works,
//       and I do not want to deviate from it.

// NOTE: this code is riddled with the number "96". In Windows, 96 DPI is 100% scale.

static Win32PlatformApplication *s_win32_app = nullptr;
Win32PlatformApplication *win32_app_instance() { return s_win32_app; }

// --- DPI helpers ---

static void enable_dpi_awareness() {
    HMODULE user32 = GetModuleHandleW(L"user32.dll");
    if (user32) {
        using Fn = BOOL(WINAPI *)(void *);
        auto fn = reinterpret_cast<Fn>(GetProcAddress(user32, "SetProcessDpiAwarenessContext"));
        // DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2 = ((DPI_AWARENESS_CONTEXT)-4)
        if (fn && fn(reinterpret_cast<void *>(static_cast<intptr_t>(-4)))) {
            return;
        }
    }
    auto shcore = LoadLibraryW(L"shcore.dll");
    if (shcore) {
        using Fn = long(WINAPI *)(int);
        auto fn = reinterpret_cast<Fn>(GetProcAddress(shcore, "SetProcessDpiAwareness"));
        if (fn) {
            fn(2); // PROCESS_PER_MONITOR_DPI_AWARE
        }
        FreeLibrary(shcore);
    }
}

static UINT get_system_dpi() {
    static auto fn = reinterpret_cast<UINT(WINAPI *)()>(
        GetProcAddress(GetModuleHandleW(L"user32.dll"), "GetDpiForSystem"));
    if (fn) {
        return fn();
    }
    return 96;
}

static float get_window_scale(HWND hwnd) {
    static auto fn = reinterpret_cast<UINT(WINAPI *)(HWND)>(
        GetProcAddress(GetModuleHandleW(L"user32.dll"), "GetDpiForWindow"));
    if (fn && hwnd) {
        return static_cast<float>(fn(hwnd)) / 96.0f;
    }
    return static_cast<float>(get_system_dpi()) / 96.0f;
}

// --- Helpers ---

static std::wstring utf8_to_wide(std::string_view s) {
    if (s.empty()) {
        return {};
    }
    int len = MultiByteToWideChar(CP_UTF8, 0, s.data(), static_cast<int>(s.size()), nullptr, 0);
    std::wstring result(len, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s.data(), static_cast<int>(s.size()), result.data(), len);
    return result;
}

static std::string wide_to_utf8(std::wstring_view w) {
    if (w.empty()) {
        return {};
    }
    int len = WideCharToMultiByte(CP_UTF8, 0, w.data(), static_cast<int>(w.size()), nullptr, 0,
                                  nullptr, nullptr);
    std::string result(len, '\0');
    WideCharToMultiByte(CP_UTF8, 0, w.data(), static_cast<int>(w.size()), result.data(), len,
                        nullptr, nullptr);
    return result;
}

static char vk_to_base_char(WPARAM vk) {
    if (vk >= 'A' && vk <= 'Z') {
        return static_cast<char>('a' + (vk - 'A'));
    }
    if (vk >= '0' && vk <= '9') {
        return static_cast<char>(vk);
    }
    return 0;
}

static Key vk_to_key(WPARAM vk) {
    switch (vk) {
    case VK_BACK:
        return Key::Backspace;
    case VK_DELETE:
        return Key::Delete;
    case VK_LEFT:
        return Key::Left;
    case VK_RIGHT:
        return Key::Right;
    case VK_UP:
        return Key::Up;
    case VK_DOWN:
        return Key::Down;
    case VK_HOME:
        return Key::Home;
    case VK_END:
        return Key::End;
    case VK_PRIOR:
        return Key::PageUp;
    case VK_NEXT:
        return Key::PageDown;
    case VK_RETURN:
        return Key::Enter;
    case VK_ESCAPE:
        return Key::Escape;
    case VK_TAB:
        return Key::Tab;
    case VK_LSHIFT:
        return Key::LeftShift;
    case VK_RSHIFT:
        return Key::RightShift;
    case VK_SHIFT:
        return Key::LeftShift;
    case VK_LCONTROL:
        return Key::LeftControl;
    case VK_RCONTROL:
        return Key::RightControl;
    case VK_CONTROL:
        return Key::LeftControl;
    case VK_LMENU:
        return Key::LeftAlt;
    case VK_RMENU:
        return Key::RightAlt;
    case VK_MENU:
        return Key::LeftAlt;
    case VK_LWIN:
        return Key::LeftSuper;
    case VK_RWIN:
        return Key::RightSuper;
    case VK_F1:
        return Key::F1;
    case VK_F2:
        return Key::F2;
    case VK_F3:
        return Key::F3;
    case VK_F4:
        return Key::F4;
    case VK_F5:
        return Key::F5;
    case VK_F6:
        return Key::F6;
    case VK_F7:
        return Key::F7;
    case VK_F8:
        return Key::F8;
    case VK_F9:
        return Key::F9;
    case VK_F10:
        return Key::F10;
    case VK_F11:
        return Key::F11;
    case VK_F12:
        return Key::F12;
    case VK_ADD:
        return Key::Plus;
    case VK_SUBTRACT:
        return Key::Minus;
    case 0x30:
        return Key::Number0;
    case 0x31:
        return Key::Number1;
    case 0x32:
        return Key::Number2;
    case 0x33:
        return Key::Number3;
    case 0x34:
        return Key::Number4;
    case 0x35:
        return Key::Number5;
    case 0x36:
        return Key::Number6;
    case 0x37:
        return Key::Number7;
    case 0x38:
        return Key::Number8;
    case 0x39:
        return Key::Number9;
    default:
        return Key::NoKey;
    }
}

static int detect_click_count(Win32PlatformApplication::WindowData &data, int button, int x, int y,
                              DWORD time) {
    UINT dblclick_time = GetDoubleClickTime();
    int dblclick_dist = GetSystemMetrics(SM_CXDOUBLECLK);
    bool same_button = (button == data.last_press_button);
    bool in_time = (time - data.last_press_time) < dblclick_time;
    bool in_range = std::abs(x - data.last_press_x) < dblclick_dist &&
                    std::abs(y - data.last_press_y) < dblclick_dist;
    if (same_button && in_time && in_range) {
        data.click_count++;
    } else {
        data.click_count = 1;
    }
    data.last_press_time = time;
    data.last_press_x = x;
    data.last_press_y = y;
    data.last_press_button = button;
    return data.click_count;
}

void Win32PlatformApplication::paint_window(HWND hwnd, Window *win) {
    PAINTSTRUCT ps;
    HDC hdc = BeginPaint(hwnd, &ps);
    float scale = get_window_scale(hwnd);
    int lw = static_cast<int>(win->size().width);
    int lh = static_cast<int>(win->size().height);
    if (lw <= 0 || lh <= 0) {
        EndPaint(hwnd, &ps);
        return;
    }
    int pw = static_cast<int>(std::ceil(lw * scale));
    int ph = static_cast<int>(std::ceil(lh * scale));

    auto *win_plat = static_cast<Win32PlatformWindow *>(win->platform_window());

    if (win_plat->hglrc) {
        // FIXME: does this code belogns here?
        wglMakeCurrent(hdc, win_plat->hglrc);
        glViewport(0, 0, pw, ph);
        glMatrixMode(GL_PROJECTION);
        glLoadIdentity();
        glOrtho(0, lw, lh, 0, -1, 1);
        glMatrixMode(GL_MODELVIEW);
        glLoadIdentity();

        glClearColor(1, 1, 1, 1);
        glClear(GL_COLOR_BUFFER_BIT);
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

        GLPainter painter(static_cast<float>(lh), scale, &win_plat->rasterizer_);
        win->handle_paint(painter);

        glFlush();
        SwapBuffers(hdc);
        wglMakeCurrent(nullptr, nullptr);
        EndPaint(hwnd, &ps);
        return;
    }

    HDC mem_dc = CreateCompatibleDC(hdc);
    HBITMAP hbm = CreateCompatibleBitmap(hdc, pw, ph);
    HBITMAP old_bm = static_cast<HBITMAP>(SelectObject(mem_dc, hbm));

    GDIPainter painter(mem_dc, scale, &win_plat->rasterizer_);
    win->handle_paint(painter);

    BitBlt(hdc, 0, 0, pw, ph, mem_dc, 0, 0, SRCCOPY);
    SelectObject(mem_dc, old_bm);
    DeleteObject(hbm);
    DeleteDC(mem_dc);
    EndPaint(hwnd, &ps);
}

LRESULT CALLBACK tk_tooltip_proc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    return DefWindowProcW(hwnd, msg, wp, lp);
}

LRESULT CALLBACK tk_wnd_proc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    auto *app = s_win32_app;
    if (!app) {
        return DefWindowProcW(hwnd, msg, wp, lp);
    }
    auto it = app->window_map.find(hwnd);
    if (it == app->window_map.end()) {
        return DefWindowProcW(hwnd, msg, wp, lp);
    }
    auto &data = it->second;
    auto win = data.owner;

    switch (msg) {
    case WM_NCHITTEST: {
        if (win->options().csd) {
            return HTCLIENT;
        }
        break;
    }
    case WM_NCCALCSIZE: {
        if (win->options().csd && wp) {
            auto *params = reinterpret_cast<NCCALCSIZE_PARAMS *>(lp);
            if (IsZoomed(hwnd)) {
                auto monitor = MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST);
                if (monitor) {
                    MONITORINFO mi = {sizeof(mi)};
                    if (GetMonitorInfoW(monitor, &mi)) {
                        params->rgrc[0] = mi.rcWork;
                    }
                }
            }
            return 0;
        }
        break;
    }
    case WM_SYSCOMMAND:
        // For CSD windows, we still want standard system commands to work.
        // They will be handled by DefWindowProcW.
        break;
    case WM_PAINT:
        Win32PlatformApplication::paint_window(hwnd, win);
        return 0;
    case WM_ERASEBKGND:
        return 1;
    case WM_SIZE: {
        float scale = get_window_scale(hwnd);
        float nw = static_cast<float>(LOWORD(lp)) / scale;
        float nh = static_cast<float>(HIWORD(lp)) / scale;
        if (nw != win->size().width || nh != win->size().height) {
            win->handle_resize({nw, nh});
            Win32PlatformApplication::paint_window(hwnd, win);
        }
        return 0;
    }
    case WM_GETMINMAXINFO: {
        auto *mmi = reinterpret_cast<MINMAXINFO *>(lp);
        auto min_s = win->min_size();
        auto max_s = win->max_size();
        float scale = get_window_scale(hwnd);
        DWORD style = win->options().csd ? WS_POPUP : WS_OVERLAPPEDWINDOW;
        if (min_s.width > 0 && min_s.height > 0) {
            RECT r = {0, 0, static_cast<LONG>(min_s.width * scale),
                      static_cast<LONG>(min_s.height * scale)};
            AdjustWindowRectEx(&r, style, FALSE, 0);
            mmi->ptMinTrackSize.x = r.right - r.left;
            mmi->ptMinTrackSize.y = r.bottom - r.top;
        }
        if (max_s.width > 0 && max_s.height > 0) {
            RECT r = {0, 0, static_cast<LONG>(max_s.width * scale),
                      static_cast<LONG>(max_s.height * scale)};
            AdjustWindowRectEx(&r, style, FALSE, 0);
            mmi->ptMaxTrackSize.x = r.right - r.left;
            mmi->ptMaxTrackSize.y = r.bottom - r.top;
        }
        return 0;
    }
    case WM_LBUTTONDOWN:
    case WM_RBUTTONDOWN:
    case WM_MBUTTONDOWN:
    case WM_XBUTTONDOWN: {
        float scale = get_window_scale(hwnd);
        int mx = static_cast<int>(static_cast<short>(LOWORD(lp)));
        int my = static_cast<int>(static_cast<short>(HIWORD(lp)));
        int button = 0;
        if (msg == WM_LBUTTONDOWN) {
            button = 0;
        } else if (msg == WM_RBUTTONDOWN) {
            button = 1;
        } else if (msg == WM_MBUTTONDOWN) {
            button = 2;
        } else if (msg == WM_XBUTTONDOWN) {
            button = (GET_XBUTTON_WPARAM(wp) == XBUTTON1) ? 3 : 4;
        }

        int clicks = detect_click_count(data, button, mx, my, GetMessageTime());
        MouseEvent e{};
        e.type = MouseEvent::Type::Press;
        e.position = {static_cast<float>(mx) / scale, static_cast<float>(my) / scale};
        e.button = button;
        e.click_count = clicks;
        e.shift = (wp & MK_SHIFT) != 0;
        e.ctrl = (GetKeyState(VK_CONTROL) & 0x8000) != 0;
        e.super = (GetKeyState(VK_LWIN) & 0x8000) != 0 || (GetKeyState(VK_RWIN) & 0x8000) != 0;
        win->handle_mouse(e);
        SetCapture(hwnd);
        return TRUE;
    }
    case WM_LBUTTONUP:
    case WM_RBUTTONUP:
    case WM_MBUTTONUP:
    case WM_XBUTTONUP: {
        ReleaseCapture();
        float scale = get_window_scale(hwnd);
        int mx = static_cast<int>(static_cast<short>(LOWORD(lp)));
        int my = static_cast<int>(static_cast<short>(HIWORD(lp)));
        int button = 0;
        if (msg == WM_LBUTTONUP) {
            button = 0;
        } else if (msg == WM_RBUTTONUP) {
            button = 1;
        } else if (msg == WM_MBUTTONUP) {
            button = 2;
        } else if (msg == WM_XBUTTONUP) {
            button = (GET_XBUTTON_WPARAM(wp) == XBUTTON1) ? 3 : 4;
        }

        MouseEvent e{};
        e.type = MouseEvent::Type::Release;
        e.position = {static_cast<float>(mx) / scale, static_cast<float>(my) / scale};
        e.button = button;
        win->handle_mouse(e);
        return TRUE;
    }
    case WM_MOUSEMOVE: {
        float scale = get_window_scale(hwnd);
        int mx = static_cast<int>(static_cast<short>(LOWORD(lp)));
        int my = static_cast<int>(static_cast<short>(HIWORD(lp)));
        bool held = (wp & (MK_LBUTTON | MK_RBUTTON | MK_MBUTTON)) != 0;
        MouseEvent e{};
        e.type = held ? MouseEvent::Type::Drag : MouseEvent::Type::Move;
        e.position = {static_cast<float>(mx) / scale, static_cast<float>(my) / scale};
        win->handle_mouse(e);
        return 0;
    }
    case WM_MOUSEWHEEL: {
        float scale = get_window_scale(hwnd);
        short delta = static_cast<short>(HIWORD(wp));
        POINT pt = {static_cast<int>(static_cast<short>(LOWORD(lp))),
                    static_cast<int>(static_cast<short>(HIWORD(lp)))};
        ScreenToClient(hwnd, &pt);
        MouseEvent e{};
        e.type = MouseEvent::Type::Scroll;
        e.position = {static_cast<float>(pt.x) / scale, static_cast<float>(pt.y) / scale};
        e.scroll_dy = static_cast<float>(delta) / WHEEL_DELTA * 20.0f;
        win->handle_mouse(e);
        return 0;
    }
    case WM_MOUSEHWHEEL: {
        float scale = get_window_scale(hwnd);
        short delta = static_cast<short>(HIWORD(wp));
        POINT pt = {static_cast<int>(static_cast<short>(LOWORD(lp))),
                    static_cast<int>(static_cast<short>(HIWORD(lp)))};
        ScreenToClient(hwnd, &pt);
        MouseEvent e{};
        e.type = MouseEvent::Type::Scroll;
        e.position = {static_cast<float>(pt.x) / scale, static_cast<float>(pt.y) / scale};
        e.scroll_dx = static_cast<float>(delta) / WHEEL_DELTA * 20.0f;
        win->handle_mouse(e);
        return 0;
    }
    case WM_KEYDOWN:
    case WM_SYSKEYDOWN: {
        bool lshift = (GetKeyState(VK_LSHIFT) & 0x8000) != 0;
        bool rshift = (GetKeyState(VK_RSHIFT) & 0x8000) != 0;
        bool lctrl = (GetKeyState(VK_LCONTROL) & 0x8000) != 0;
        bool rctrl = (GetKeyState(VK_RCONTROL) & 0x8000) != 0;
        bool lalt = (GetKeyState(VK_LMENU) & 0x8000) != 0;
        bool ralt = (GetKeyState(VK_RMENU) & 0x8000) != 0;
        bool lsuper = (GetKeyState(VK_LWIN) & 0x8000) != 0;
        bool rsuper = (GetKeyState(VK_RWIN) & 0x8000) != 0;
        bool shift = lshift || rshift;
        bool ctrl = lctrl || rctrl;
        bool alt = lalt || ralt;
        bool super = lsuper || rsuper;
        Key key = vk_to_key(wp);
        // Skip modifier-only key repeats (bit 30 = previous key state).
        // Holding Ctrl/Shift/Alt/Win produces repeated WM_KEYDOWN that would
        // re-trigger shortcuts like Ctrl+Shift, even though the state didn't change.
        bool is_modifier = wp == VK_CONTROL || wp == VK_LCONTROL || wp == VK_RCONTROL ||
                           wp == VK_SHIFT || wp == VK_LSHIFT || wp == VK_RSHIFT || wp == VK_MENU ||
                           wp == VK_LMENU || wp == VK_RMENU || wp == VK_LWIN || wp == VK_RWIN;
        if (is_modifier && (lp & 0x40000000)) {
            break;
        }
        if (key != Key::NoKey || ctrl || alt || super) {
            KeyEvent ke;
            ke.type = KeyEvent::Type::Press;
            ke.key = key;
            ke.shift = shift;
            ke.ctrl = ctrl;
            ke.alt = alt;
            ke.super = super;
            ke.lshift = lshift;
            ke.rshift = rshift;
            ke.lctrl = lctrl;
            ke.rctrl = rctrl;
            ke.lalt = lalt;
            ke.ralt = ralt;
            ke.lsuper = lsuper;
            ke.rsuper = rsuper;
            if ((ctrl || alt) && key == Key::NoKey) {
                char ch = vk_to_base_char(wp);
                if (ch) {
                    ke.text = std::string(1, ch);
                }
            }
            win->handle_key(ke);
            return 0;
        }
        break;
    }
    case WM_CHAR: {
        wchar_t wc = static_cast<wchar_t>(wp);
        auto has_ctrl = (GetKeyState(VK_CONTROL) & 0x8000) != 0;
        auto has_alt = (GetKeyState(VK_MENU) & 0x8000) != 0;
        if (wc < 32) {
            break;
        }
        // Ctrl (without Alt) combos are handled via WM_KEYDOWN with modifiers set.
        // WM_CHAR does not carry modifier state, so inserting the character
        // would double-fire shortcuts like Ctrl+Shift+L → direction change + "L".
        // AltGr (Ctrl+Alt) should still produce text input via WM_CHAR.
        if (has_ctrl && !has_alt) {
            break;
        }
        char utf8[8] = {};
        WideCharToMultiByte(CP_UTF8, 0, &wc, 1, utf8, sizeof(utf8) - 1, nullptr, nullptr);
        KeyEvent ke;
        ke.type = KeyEvent::Type::Press;
        ke.text = utf8;
        win->handle_key(ke);
        return 0;
    }
    case WM_SYSCHAR:
        return 0;
    case WM_SETCURSOR:
        if (LOWORD(lp) == HTCLIENT && data.current_cursor) {
            SetCursor(data.current_cursor);
            return TRUE;
        }
        break;
    case WM_TIMER: {
        auto tid = static_cast<UINT_PTR>(wp);
        auto it = app->timers.find(tid);
        if (it != app->timers.end()) {
            auto &info = it->second;
            if (info.hwnd == hwnd) {
                auto cb = info.callback;
                if (!info.repeats) {
                    KillTimer(hwnd, tid);
                    app->timers.erase(it);
                }
                cb();
                return 0;
            }
        }
        break;
    }
    case WM_KILLFOCUS:
        win->hide_tooltip();
        return 0;
    case WM_ACTIVATE:
        if (LOWORD(wp) == WA_INACTIVE) {
            win->hide_tooltip();
            win->handle_activate(false);
        } else {
            win->handle_activate(true);
        }
        break;
    case WM_DPICHANGED: {
        auto *suggested = reinterpret_cast<RECT *>(lp);
        SetWindowPos(hwnd, nullptr, suggested->left, suggested->top,
                     suggested->right - suggested->left, suggested->bottom - suggested->top,
                     SWP_NOZORDER | SWP_NOACTIVATE);
        InvalidateRect(hwnd, nullptr, FALSE);
        return 0;
    }
    case WM_CLOSE:
        DestroyWindow(hwnd);
        return 0;
    case WM_DESTROY: {
        auto it = app->window_map.find(hwnd);
        if (it != app->window_map.end()) {
            auto *plat_win =
                static_cast<Win32PlatformWindow *>(it->second.owner->platform_window());
            if (plat_win && plat_win->modal_parent_hwnd) {
                EnableWindow(plat_win->modal_parent_hwnd, TRUE);
                SetForegroundWindow(plat_win->modal_parent_hwnd);
                plat_win->modal_parent_hwnd = nullptr;
            }
            app->window_map.erase(it);
        }
        if (app->window_map.empty()) {
            PostQuitMessage(0);
        }
        return 0;
    }
    default:
        break;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

// --- Win32PlatformApplication ---

Win32PlatformApplication::Win32PlatformApplication() {
    enable_dpi_awareness();
    s_win32_app = this;
    set_rasterizer(&app_rasterizer_);
    hinstance = GetModuleHandleW(nullptr);
    main_thread_id = GetCurrentThreadId();

    Gdiplus::GdiplusStartupInput gdiplusStartupInput;
    Gdiplus::GdiplusStartup(&gdiplus_token, &gdiplusStartupInput, nullptr);

    WNDCLASSEXW wc = {};
    wc.cbSize = sizeof(wc);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = tk_wnd_proc;
    wc.hInstance = hinstance;
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wc.lpszClassName = kWindowClassName;
    RegisterClassExW(&wc);
    WNDCLASSEXW tc = {};
    tc.cbSize = sizeof(tc);
    tc.lpfnWndProc = tk_tooltip_proc;
    tc.hInstance = hinstance;
    tc.lpszClassName = kTooltipClassName;
    RegisterClassExW(&tc);

    if (const char *env = std::getenv("SVISION_PAINT")) {
        if (std::string_view(env) == "opengl") {
            opengl_requested = true;
        }
    }

    spdlog::debug("Win32 backend initialized (opengl={})", opengl_requested);
}

Win32PlatformApplication::~Win32PlatformApplication() {
    UnregisterClassW(kWindowClassName, hinstance);
    UnregisterClassW(kTooltipClassName, hinstance);
    if (gdiplus_token) {
        Gdiplus::GdiplusShutdown(gdiplus_token);
    }
    s_win32_app = nullptr;
}

float Win32PlatformApplication::scale_factor() const {
    return static_cast<float>(get_system_dpi()) / 96.0f;
}

SystemFonts Win32PlatformApplication::system_fonts() const {
    NONCLIENTMETRICSW ncm = {sizeof(NONCLIENTMETRICSW)};
    if (SystemParametersInfoW(SPI_GETNONCLIENTMETRICS, sizeof(ncm), &ncm, 0)) {
        float dpi = static_cast<float>(get_system_dpi());
        // 72 = points per inch (typographic constant); see
        // https://en.wikipedia.org/wiki/Point_(typography)
        float size = static_cast<float>(std::abs(ncm.lfMessageFont.lfHeight)) * 72.0f / dpi;
        return {wide_to_utf8(ncm.lfMessageFont.lfFaceName), "Consolas", size};
    }
    return {"Segoe UI", "Consolas", 9.0f};
}

std::unique_ptr<PlatformWindow> Win32PlatformApplication::create_window(std::string_view title,
                                                                        Size size, Window *owner,
                                                                        WindowOptions options) {
    return std::make_unique<Win32PlatformWindow>(this, title, size, owner, options);
}
std::shared_ptr<ImageLoaderInterface> Win32PlatformApplication::get_image_loader() {
    if (!image_loader_) {
        image_loader_ = std::make_shared<Win32ImageLoader>();
    }
    return image_loader_;
}

std::shared_ptr<SVGLoaderInterface> Win32PlatformApplication::get_svg_loader() {
    if (!svg_loader_) {
        svg_loader_ = std::make_shared<LunasvgImageLoader>();
    }
    return svg_loader_;
}

int Win32PlatformApplication::run() {
    spdlog::info("Starting run loop (Win32)");
    MSG msg;
    while (GetMessageW(&msg, nullptr, 0, 0)) {
        if (msg.message == WM_TK_INVOKE && msg.hwnd == nullptr) {
            std::vector<std::function<void()>> fns;
            {
                std::lock_guard lock(posted_mutex);
                fns.swap(posted_fns);
            }
            for (auto &fn : fns) {
                fn();
            }
            continue;
        }
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    return static_cast<int>(msg.wParam);
}

void Win32PlatformApplication::run_until(std::function<bool()> should_exit) {
    while (!should_exit()) {
        MSG msg;
        while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
            if (msg.message == WM_QUIT) {
                PostQuitMessage(static_cast<int>(msg.wParam));
                return;
            }
            if (msg.message == WM_TK_INVOKE && msg.hwnd == nullptr) {
                std::vector<std::function<void()>> fns;
                {
                    std::lock_guard lock(posted_mutex);
                    fns.swap(posted_fns);
                }
                for (auto &fn : fns) {
                    fn();
                }
                continue;
            }
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
        if (should_exit()) {
            break;
        }
        MsgWaitForMultipleObjectsEx(0, nullptr, 10, QS_ALLINPUT, MWMO_INPUTAVAILABLE);
    }
}

void Win32PlatformApplication::quit() { PostQuitMessage(0); }

void Win32PlatformApplication::post_to_main_thread(std::function<void()> fn) {
    {
        std::lock_guard lock(posted_mutex);
        posted_fns.push_back(std::move(fn));
    }
    PostThreadMessageW(main_thread_id, WM_TK_INVOKE, 0, 0);
}

std::string Win32PlatformApplication::clipboard_get_text() {
    if (!OpenClipboard(nullptr)) {
        return {};
    }
    HANDLE h = GetClipboardData(CF_UNICODETEXT);
    if (!h) {
        CloseClipboard();
        return {};
    }
    auto *data = static_cast<wchar_t *>(GlobalLock(h));
    if (!data) {
        CloseClipboard();
        return {};
    }
    int len = WideCharToMultiByte(CP_UTF8, 0, data, -1, nullptr, 0, nullptr, nullptr);
    std::string result;
    if (len > 1) {
        result.resize(len - 1);
        WideCharToMultiByte(CP_UTF8, 0, data, -1, result.data(), len, nullptr, nullptr);
    }
    GlobalUnlock(h);
    CloseClipboard();
    return result;
}

void Win32PlatformApplication::clipboard_set_text(std::string const &text) {
    int wlen =
        MultiByteToWideChar(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), nullptr, 0);
    if (wlen <= 0) {
        return;
    }
    HGLOBAL hg = GlobalAlloc(GMEM_MOVEABLE, (wlen + 1) * sizeof(wchar_t));
    if (!hg) {
        return;
    }
    auto *dest = static_cast<wchar_t *>(GlobalLock(hg));
    MultiByteToWideChar(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), dest, wlen);
    dest[wlen] = L'\0';
    GlobalUnlock(hg);
    if (OpenClipboard(nullptr)) {
        EmptyClipboard();
        SetClipboardData(CF_UNICODETEXT, hg);
        CloseClipboard();
    } else {
        GlobalFree(hg);
    }
}

Win32PlatformWindow::Win32PlatformWindow(Win32PlatformApplication *app, std::string_view title,
                                         Size size, Window *owner, WindowOptions options)
    : app_(app), owner_(owner) {
    arrow_cursor = LoadCursorW(nullptr, IDC_ARROW);
    ibeam_cursor = LoadCursorW(nullptr, IDC_IBEAM);
    hand_cursor = LoadCursorW(nullptr, IDC_HAND);
    not_allowed_cursor = LoadCursorW(nullptr, IDC_NO);
    resize_ew_cursor = LoadCursorW(nullptr, IDC_SIZEWE);
    resize_ns_cursor = LoadCursorW(nullptr, IDC_SIZENS);
    resize_nw_cursor = LoadCursorW(nullptr, IDC_SIZENWSE);
    resize_nesw_cursor = LoadCursorW(nullptr, IDC_SIZENESW);
    move_cursor = LoadCursorW(nullptr, IDC_SIZEALL);
    std::wstring wtitle = utf8_to_wide(title);
    float scale = static_cast<float>(get_system_dpi()) / 96.0f;

    DWORD style = WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN;
    DWORD adjust_style = WS_OVERLAPPEDWINDOW;

    if (options.csd) {
        // Use WS_OVERLAPPEDWINDOW to get standard animations even for CSD.
        // We handle WM_NCCALCSIZE to remove the standard frame.
        style = WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN;
        // For CSD, client area == window area.
        adjust_style = WS_POPUP;
    }

    RECT r = {0, 0, static_cast<LONG>(size.width * scale), static_cast<LONG>(size.height * scale)};
    AdjustWindowRectEx(&r, adjust_style, FALSE, 0);
    hwnd = CreateWindowExW(0, Win32PlatformApplication::kWindowClassName, wtitle.c_str(), style,
                           CW_USEDEFAULT, CW_USEDEFAULT, r.right - r.left, r.bottom - r.top,
                           nullptr, nullptr, app_->hinstance, nullptr);

    MARGINS margins = {1, 1, 1, 1};
    DwmExtendFrameIntoClientArea(hwnd, &margins);

    // After creation, we might have a different scale if we are on a different monitor
    float actual_scale = get_window_scale(hwnd);
    if (std::abs(actual_scale - scale) > 0.001f) {
        RECT r2 = {0, 0, static_cast<LONG>(size.width * actual_scale),
                   static_cast<LONG>(size.height * actual_scale)};
        AdjustWindowRectEx(&r2, adjust_style, FALSE, 0);
        SetWindowPos(hwnd, nullptr, 0, 0, r2.right - r2.left, r2.bottom - r2.top,
                     SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
    }

    app_->window_map[hwnd] = {owner, arrow_cursor};

    if (app_->opengl_requested) {
        HDC hdc = GetDC(hwnd);
        PIXELFORMATDESCRIPTOR pfd = {};
        pfd.nSize = sizeof(pfd);
        pfd.nVersion = 1;
        pfd.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
        pfd.iPixelType = PFD_TYPE_RGBA;
        pfd.cColorBits = 32;
        pfd.cDepthBits = 24;
        pfd.cStencilBits = 8;
        pfd.iLayerType = PFD_MAIN_PLANE;

        int pf = ChoosePixelFormat(hdc, &pfd);
        SetPixelFormat(hdc, pf, &pfd);
        hglrc = wglCreateContext(hdc);
        if (!hglrc) {
            spdlog::warn("Failed to create WGL context, falling back to Cairo/GDI+");
        }
        ReleaseDC(hwnd, hdc);
    }
    if (hglrc) {
        backend_ = std::make_unique<Win32OpenGlRenderingBackend>(hwnd, hglrc, &rasterizer_);
    } else {
        backend_ = std::make_unique<Win32GDIRenderingBackend>(&rasterizer_);
    }
}

Win32PlatformWindow::~Win32PlatformWindow() {
    for (auto it = app_->timers.begin(); it != app_->timers.end();) {
        if (it->second.window == owner_) {
            KillTimer(hwnd, it->first);
            it = app_->timers.erase(it);
        } else {
            ++it;
        }
    }
    if (modal_parent_hwnd) {
        EnableWindow(modal_parent_hwnd, TRUE);
        SetForegroundWindow(modal_parent_hwnd);
        modal_parent_hwnd = nullptr;
    }
    if (hwnd) {
        if (hglrc) {
            wglDeleteContext(hglrc);
        }
        if (hicon) {
            DestroyIcon(hicon);
            hicon = nullptr;
        }
        DestroyWindow(hwnd);
        hwnd = nullptr;
    }
}

std::string_view Win32PlatformWindow::painter_name() const {
    if (hglrc) {
        return "OpenGL";
    }
    return "GDI+";
}

void Win32PlatformWindow::show() {
    ShowWindow(hwnd, SW_SHOW);
    UpdateWindow(hwnd);
}

void Win32PlatformWindow::close() {
    if (modal_parent_hwnd) {
        EnableWindow(modal_parent_hwnd, TRUE);
        SetForegroundWindow(modal_parent_hwnd);
        modal_parent_hwnd = nullptr;
    }
    if (hwnd) {
        DestroyWindow(hwnd);
        hwnd = nullptr;
    }
}

void Win32PlatformWindow::minimize() {
    if (hwnd) {
        ShowWindow(hwnd, SW_MINIMIZE);
    }
}

void Win32PlatformWindow::maximize() {
    if (hwnd) {
        ShowWindow(hwnd, SW_MAXIMIZE);
    }
}

void Win32PlatformWindow::restore() {
    if (hwnd) {
        ShowWindow(hwnd, SW_RESTORE);
    }
}

void Win32PlatformWindow::set_size(Size s) {
    if (!hwnd) {
        return;
    }
    float scale = get_window_scale(hwnd);
    RECT r = {0, 0, static_cast<LONG>(s.width * scale), static_cast<LONG>(s.height * scale)};
    DWORD style = owner_->options().csd ? WS_POPUP : WS_OVERLAPPEDWINDOW;
    AdjustWindowRectEx(&r, style, FALSE, 0);
    SetWindowPos(hwnd, nullptr, 0, 0, r.right - r.left, r.bottom - r.top,
                 SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
}

void Win32PlatformWindow::request_redraw() {
    if (hwnd) {
        InvalidateRect(hwnd, nullptr, FALSE);
    }
}

void Win32PlatformWindow::set_min_size(Size) {
    // Enforced via WM_GETMINMAXINFO
}

void Win32PlatformWindow::set_max_size(Size) {
    // Enforced via WM_GETMINMAXINFO
}

int Win32PlatformWindow::start_timer(float interval_sec, std::function<void()> callback,
                                     bool repeats) {
    UINT_PTR tid = app_->next_timer_id++;
    UINT ms = static_cast<UINT>(interval_sec * 1000.0f);
    if (ms == 0) {
        ms = 1;
    }
    if (SetTimer(hwnd, tid, ms, nullptr) == 0) {
        spdlog::error("Failed to start Win32 timer with ID {}", tid);
        return 0;
    }
    Win32PlatformApplication::TimerInfo info;
    info.toolkit_id = static_cast<int>(tid);
    info.window = owner_;
    info.hwnd = hwnd;
    info.repeats = repeats;
    info.callback = std::move(callback);
    app_->timers[tid] = std::move(info);
    return static_cast<int>(tid);
}

void Win32PlatformWindow::stop_timer(int timer_id) {
    auto tid = static_cast<UINT_PTR>(timer_id);
    auto it = app_->timers.find(tid);
    if (it != app_->timers.end()) {
        KillTimer(it->second.hwnd, tid);
        app_->timers.erase(it);
    }
}

void Win32PlatformWindow::set_title(std::string_view t) {
    SetWindowTextW(hwnd, std::wstring(t.begin(), t.end()).c_str());
}

void Win32PlatformWindow::set_icon(Icon const &icon) {
    if (icon_ == icon && hicon) {
        return;
    }
    icon_ = icon;
    if (hicon) {
        DestroyIcon(hicon);
        hicon = nullptr;
    }

    if (!icon || icon->pixels.empty() || !hwnd) {
        return;
    }

    int width = icon->width;
    int height = icon->height;

    BITMAPINFO bi = {0};
    bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bi.bmiHeader.biWidth = width;
    bi.bmiHeader.biHeight = -height;
    bi.bmiHeader.biPlanes = 1;
    bi.bmiHeader.biBitCount = 32;
    bi.bmiHeader.biCompression = BI_RGB;

    void *bits = nullptr;
    HDC hdc = GetDC(nullptr);
    HBITMAP hBitmap = CreateDIBSection(hdc, &bi, DIB_RGB_COLORS, &bits, nullptr, 0);
    ReleaseDC(nullptr, hdc);

    if (!hBitmap) {
        return;
    }

    uint8_t *dest = static_cast<uint8_t *>(bits);
    const uint8_t *src = icon->pixels.data();
    for (int i = 0; i < width * height; i++) {
        dest[i * 4 + 0] = src[i * 4 + 2]; // B
        dest[i * 4 + 1] = src[i * 4 + 1]; // G
        dest[i * 4 + 2] = src[i * 4 + 0]; // R
        dest[i * 4 + 3] = src[i * 4 + 3]; // A
    }

    // Create a zeroed-out AND mask
    HBITMAP hMonoBitmap = CreateBitmap(width, height, 1, 1, nullptr);
    HDC hdcMask = CreateCompatibleDC(nullptr);
    HBITMAP hOldMask = (HBITMAP)SelectObject(hdcMask, hMonoBitmap);
    PatBlt(hdcMask, 0, 0, width, height, BLACKNESS);
    SelectObject(hdcMask, hOldMask);
    DeleteDC(hdcMask);

    ICONINFO ii = {0};
    ii.fIcon = TRUE;
    ii.xHotspot = 0;
    ii.yHotspot = 0;
    ii.hbmMask = hMonoBitmap;
    ii.hbmColor = hBitmap;

    hicon = CreateIconIndirect(&ii);

    if (hicon) {
        spdlog::info("Win32PlatformWindow::set_icon created HICON={:p} ({}x{})", (void *)hicon,
                     width, height);
        SendMessageW(hwnd, WM_SETICON, ICON_SMALL, (LPARAM)hicon);
        SendMessageW(hwnd, WM_SETICON, ICON_BIG, (LPARAM)hicon);
    } else {
        spdlog::error("Win32PlatformWindow::set_icon CreateIconIndirect failed, error={}",
                      GetLastError());
    }

    DeleteObject(hBitmap);
    DeleteObject(hMonoBitmap);
}

// Helper to convert HICON to ImageData
static std::shared_ptr<ImageData> hicon_to_image(HICON hicon) {
    if (!hicon) {
        return nullptr;
    }

    ICONINFO ii;
    if (!GetIconInfo(hicon, &ii)) {
        return nullptr;
    }

    int width = 0;
    int height = 0;
    bool has_alpha = false;

    if (ii.hbmColor) {
        BITMAP bmp;
        GetObject(ii.hbmColor, sizeof(BITMAP), &bmp);
        width = bmp.bmWidth;
        height = bmp.bmHeight;
        has_alpha = (bmp.bmBitsPixel == 32);
    } else if (ii.hbmMask) {
        BITMAP bmp;
        GetObject(ii.hbmMask, sizeof(BITMAP), &bmp);
        width = bmp.bmWidth;
        height = bmp.bmHeight / 2;
    } else {
        return nullptr;
    }

    auto img = std::make_shared<ImageData>();
    img->width = width;
    img->height = height;
    img->channels = 4;
    img->pixels.resize(width * height * 4);

    HDC hdc = GetDC(nullptr);
    BITMAPINFO bi = {0};
    bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bi.bmiHeader.biWidth = width;
    bi.bmiHeader.biHeight = -height;
    bi.bmiHeader.biPlanes = 1;
    bi.bmiHeader.biBitCount = 32;
    bi.bmiHeader.biCompression = BI_RGB;

    if (ii.hbmColor) {
        GetDIBits(hdc, ii.hbmColor, 0, height, img->pixels.data(), &bi, DIB_RGB_COLORS);
    } else {
        GetDIBits(hdc, ii.hbmMask, 0, height, img->pixels.data(), &bi, DIB_RGB_COLORS);
    }
    ReleaseDC(nullptr, hdc);

    // Convert BGRA to RGBA and check if alpha is all zeros
    bool all_alpha_zero = true;
    for (size_t i = 0; i < img->pixels.size(); i += 4) {
        std::swap(img->pixels[i], img->pixels[i + 2]);
        if (img->pixels[i + 3] != 0) {
            all_alpha_zero = false;
        }
    }

    // If alpha is all zeros (common for 24-bit icons loaded into 32-bit DIB)
    // or if we know it doesn't have alpha, set it to opaque.
    if (all_alpha_zero || !has_alpha) {
        for (size_t i = 0; i < img->pixels.size(); i += 4) {
            img->pixels[i + 3] = 255;
        }
    }

    DeleteObject(ii.hbmMask);
    if (ii.hbmColor) {
        DeleteObject(ii.hbmColor);
    }
    return img;
}

static constexpr std::string_view default_windows_icon_xpm = R"(/* XPM */
static char *icon[] = {
"16 16 4 1",
"  c None",
". c #0078D4",
"X c #FFFFFF",
"o c #76B9ED",
"                ",
"  ............  ",
"  .oooooooooo.  ",
"  .oXXXXXXXXo.  ",
"  .oX      Xo.  ",
"  .oX      Xo.  ",
"  .oX      Xo.  ",
"  .oX      Xo.  ",
"  .oX      Xo.  ",
"  .oX      Xo.  ",
"  .oX      Xo.  ",
"  .oXXXXXXXXo.  ",
"  .oooooooooo.  ",
"  ............  ",
"                ",
"                "
};)";

Icon Win32PlatformWindow::get_icon() {
    if (icon_) {
        return icon_;
    }
    HICON hicon_small = (HICON)SendMessageW(hwnd, WM_GETICON, ICON_SMALL, 0);
    if (!hicon_small) {
        hicon_small = (HICON)GetClassLongPtrW(hwnd, GCLP_HICONSM);
    }
    if (hicon_small) {
        icon_ = hicon_to_image(hicon_small);
        if (icon_) {
            return icon_;
        }
    }
    icon_ = parse_xpm(default_windows_icon_xpm);
    return icon_;
}

void Win32PlatformWindow::show_system_menu(Point p) {
    HMENU hMenu = GetSystemMenu(hwnd, FALSE);
    if (!hMenu) {
        return;
    }

    WINDOWPLACEMENT wp = {sizeof(wp)};
    GetWindowPlacement(hwnd, &wp);
    bool maximized = (wp.showCmd == SW_SHOWMAXIMIZED);
    bool minimized = (wp.showCmd == SW_SHOWMINIMIZED);

    EnableMenuItem(hMenu, SC_RESTORE,
                   MF_BYCOMMAND | (maximized || minimized ? MF_ENABLED : MF_GRAYED));
    EnableMenuItem(hMenu, SC_MOVE, MF_BYCOMMAND | (maximized ? MF_GRAYED : MF_ENABLED));
    EnableMenuItem(hMenu, SC_SIZE, MF_BYCOMMAND | (maximized ? MF_GRAYED : MF_ENABLED));
    EnableMenuItem(hMenu, SC_MINIMIZE, MF_BYCOMMAND | (minimized ? MF_GRAYED : MF_ENABLED));
    EnableMenuItem(hMenu, SC_MAXIMIZE, MF_BYCOMMAND | (maximized ? MF_GRAYED : MF_ENABLED));

    float scale = get_window_scale(hwnd);
    POINT pt = {static_cast<LONG>(p.x * scale), static_cast<LONG>(p.y * scale)};
    ClientToScreen(hwnd, &pt);

    // TrackPopupMenu with hwnd (instead of TPM_RETURNCMD) sends WM_SYSCOMMAND
    // to the window proc, which is more reliable for system menu integration.
    int cmd = TrackPopupMenu(hMenu, TPM_LEFTBUTTON | TPM_RETURNCMD, pt.x, pt.y, 0, hwnd, nullptr);
    if (cmd) {
        PostMessageW(hwnd, WM_SYSCOMMAND, cmd, 0);
    }
}

void Win32PlatformWindow::set_cursor(CursorShape shape) {
    HCURSOR hc;
    switch (shape) {
    case CursorShape::IBeam:
        hc = ibeam_cursor;
        break;
    case CursorShape::Hand:
        hc = hand_cursor;
        break;
    case CursorShape::NotAllowed:
        hc = not_allowed_cursor;
        break;
    case CursorShape::ResizeEW:
        hc = resize_ew_cursor;
        break;
    case CursorShape::ResizeNS:
        hc = resize_ns_cursor;
        break;
    case CursorShape::ResizeNW:
        hc = resize_nw_cursor;
        break;
    case CursorShape::ResizeNESW:
        hc = resize_nesw_cursor;
        break;
    case CursorShape::Move:
        hc = move_cursor;
        break;
    default:
        hc = arrow_cursor;
        break;
    }
    auto it = app_->window_map.find(hwnd);
    if (it != app_->window_map.end()) {
        it->second.current_cursor = hc;
    }
    SetCursor(hc);
}

void Win32PlatformWindow::start_system_move(uint32_t /*serial*/) {
    ReleaseCapture();
    SendMessageW(hwnd, WM_SYSCOMMAND, SC_MOVE | HTCAPTION, 0);
}

void Win32PlatformWindow::start_system_resize(WindowEdge edge, uint32_t /*serial*/) {
    ReleaseCapture();
    WPARAM wparam = 0;
    switch (edge) {
    case WindowEdge::Left:
        wparam = SC_SIZE | WMSZ_LEFT;
        break;
    case WindowEdge::Right:
        wparam = SC_SIZE | WMSZ_RIGHT;
        break;
    case WindowEdge::Top:
        wparam = SC_SIZE | WMSZ_TOP;
        break;
    case WindowEdge::Bottom:
        wparam = SC_SIZE | WMSZ_BOTTOM;
        break;
    case WindowEdge::TopLeft:
        wparam = SC_SIZE | WMSZ_TOPLEFT;
        break;
    case WindowEdge::TopRight:
        wparam = SC_SIZE | WMSZ_TOPRIGHT;
        break;
    case WindowEdge::BottomLeft:
        wparam = SC_SIZE | WMSZ_BOTTOMLEFT;
        break;
    case WindowEdge::BottomRight:
        wparam = SC_SIZE | WMSZ_BOTTOMRIGHT;
        break;
    default:
        return;
    }
    SendMessageW(hwnd, WM_SYSCOMMAND, wparam, 0);
}
void Win32PlatformWindow::show_tooltip_window(std::string const &text, Point pos) {

    float scale = get_window_scale(hwnd);
    auto const &style = Theme::current().style.tooltip;
    auto &palette = Theme::current().palette;
    float pad = style.padding, font_sz = palette.fonts.size;
    auto text_sz = rasterizer_.measure(text, font_sz);
    auto fm = rasterizer_.metrics(font_sz);
    float w = text_sz.width + pad * 2, h = fm.height + pad * 2;
    POINT pt = {static_cast<LONG>(pos.x * scale), static_cast<LONG>(pos.y * scale)};
    ClientToScreen(hwnd, &pt);
    int piw = std::max(1, static_cast<int>(std::ceil(w * scale)));
    int pih = std::max(1, static_cast<int>(std::ceil(h * scale)));
    int sx = pt.x, sy = pt.y - pih - 4;
    HMONITOR hmon = MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST);
    MONITORINFO mi = {sizeof(mi)};
    GetMonitorInfoW(hmon, &mi);
    RECT work = mi.rcWork;
    if (sx + piw > work.right) {
        sx = work.right - piw - 2;
    }
    if (sx < work.left) {
        sx = work.left + 2;
    }
    if (sy < work.top) {
        sy = pt.y + 20;
    }
    DWORD ex_style = WS_EX_TOOLWINDOW | WS_EX_TOPMOST | WS_EX_NOACTIVATE | WS_EX_LAYERED;
    if (!tooltip_hwnd) {
        tooltip_hwnd =
            CreateWindowExW(ex_style, Win32PlatformApplication::kTooltipClassName, L"", WS_POPUP,
                            sx, sy, piw, pih, hwnd, nullptr, app_->hinstance, nullptr);
    } else {
        MoveWindow(tooltip_hwnd, sx, sy, piw, pih, FALSE);
    }

    HDC screen_dc = GetDC(nullptr);
    HDC mem_dc = CreateCompatibleDC(screen_dc);
    BITMAPINFO bmi = {};
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = piw;
    bmi.bmiHeader.biHeight = -pih;
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;
    void *bits = nullptr;
    HBITMAP hbm = CreateDIBSection(mem_dc, &bmi, DIB_RGB_COLORS, &bits, nullptr, 0);
    HBITMAP old_bm = static_cast<HBITMAP>(SelectObject(mem_dc, hbm));

    backend_->render_to_buffer(app_, piw, pih, scale, bits, [&](Painter &p) {
        auto fm = p.font_metrics(font_sz);
        Rect r{0, 0, w, h};
        p.fill_rounded_rect(r, palette.tooltip, Theme::current().style.corner_radius);
        // p.draw_rounded_rect(r, style.border, style.corner_radius, style.border_width);
        // p.draw_text(text, {pad, pad + fm.ascent}, style.text, font_sz);
        p.draw_text(text, {pad, pad + fm.ascent}, palette.text, palette.fonts.size);
    });

    POINT pt_pos = {sx, sy};
    SIZE tsz = {piw, pih};
    POINT pt_src = {0, 0};
    BLENDFUNCTION blend = {AC_SRC_OVER, 0, 255, AC_SRC_ALPHA};
    UpdateLayeredWindow(tooltip_hwnd, screen_dc, &pt_pos, &tsz, mem_dc, &pt_src, 0, &blend,
                        ULW_ALPHA);

    SelectObject(mem_dc, old_bm);
    DeleteObject(hbm);
    DeleteDC(mem_dc);
    ReleaseDC(nullptr, screen_dc);
    ShowWindow(tooltip_hwnd, SW_SHOWNOACTIVATE);
}

void Win32PlatformWindow::hide_tooltip_window() {
    if (tooltip_hwnd) {
        ShowWindow(tooltip_hwnd, SW_HIDE);
    }
}

void Win32PlatformWindow::set_modal_for(PlatformWindow *parent) {
    auto *p = static_cast<Win32PlatformWindow *>(parent);
    modal_parent_hwnd = p->hwnd;
    SetWindowLongPtrW(hwnd, GWLP_HWNDPARENT, reinterpret_cast<LONG_PTR>(p->hwnd));
    EnableWindow(p->hwnd, FALSE);
}

void Win32PlatformWindow::grab_pointer() { SetCapture(hwnd); }

void Win32PlatformWindow::ungrab_pointer() { ReleaseCapture(); }

Icon Win32PlatformWindow::capture() { return GDIPainter::capture(owner_); }

float Win32PlatformWindow::scale_factor() const { return get_window_scale(hwnd); }

} // namespace toolkit
