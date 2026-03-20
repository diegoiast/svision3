#include "win32_platform.hpp"
#ifdef TOOLKIT_HAS_CAIRO
#include "toolkit/painters/cairo_painter.hpp"
#endif
#include "toolkit/painters/win32_painter.hpp"
#include "toolkit/theme.hpp"
#include "toolkit/window.hpp"

#include <GL/gl.h>

#ifdef TOOLKIT_HAS_CAIRO
#include <cairo.h>
#endif
#include <spdlog/spdlog.h>

#include <algorithm>
#include <cmath>
#include <cstring>

namespace toolkit {

static Win32PlatformApplication *s_win32_app = nullptr;
Win32PlatformApplication *win32_app_instance() { return s_win32_app; }

// --- DPI helpers ---

static void enable_dpi_awareness() {
    auto user32 = GetModuleHandleW(L"user32.dll");
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

#ifndef TOOLKIT_HAS_CAIRO
static void paint_window_gdiplus(HWND hwnd, Window *win, HDC hdc, float scale, int pw, int ph) {
    HDC mem_dc = CreateCompatibleDC(hdc);
    HBITMAP hbm = CreateCompatibleBitmap(hdc, pw, ph);
    HBITMAP old_bm = static_cast<HBITMAP>(SelectObject(mem_dc, hbm));

    {
        GDIPainter painter(mem_dc, scale);
        win->handle_paint(painter);
    }

    BitBlt(hdc, 0, 0, pw, ph, mem_dc, 0, 0, SRCCOPY);

    SelectObject(mem_dc, old_bm);
    DeleteObject(hbm);
    DeleteDC(mem_dc);
}
#endif

static void paint_window(HWND hwnd, Window *win) {
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

        Win32TextRasterizer rasterizer;
        GLPainter painter(static_cast<float>(lh), scale, rasterizer);
        win->handle_paint(painter);

        glFlush();
        SwapBuffers(hdc);
        wglMakeCurrent(nullptr, nullptr);
        EndPaint(hwnd, &ps);
        return;
    }

#ifdef TOOLKIT_HAS_CAIRO
    cairo_surface_t *surface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, pw, ph);
    cairo_t *cr = cairo_create(surface);
    cairo_scale(cr, scale, scale);
    CairoPainter painter(cr);
    win->handle_paint(painter);
    cairo_surface_flush(surface);
    unsigned char *data = cairo_image_surface_get_data(surface);
    BITMAPINFO bmi = {};
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = pw;
    bmi.bmiHeader.biHeight = -ph;
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;
    SetDIBitsToDevice(hdc, 0, 0, pw, ph, 0, 0, 0, ph, data, &bmi, DIB_RGB_COLORS);
    cairo_destroy(cr);
    cairo_surface_destroy(surface);
#else
    paint_window_gdiplus(hwnd, win, hdc, scale, pw, ph);
#endif
    EndPaint(hwnd, &ps);
}

// --- WndProc ---

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
    Window *win = data.owner;

    switch (msg) {
    case WM_PAINT:
        paint_window(hwnd, win);
        return 0;
    case WM_ERASEBKGND:
        return 1;
    case WM_SIZE: {
        float scale = get_window_scale(hwnd);
        float nw = static_cast<float>(LOWORD(lp)) / scale;
        float nh = static_cast<float>(HIWORD(lp)) / scale;
        if (nw != win->size().width || nh != win->size().height) {
            win->handle_resize({nw, nh});
            paint_window(hwnd, win);
        }
        return 0;
    }
    case WM_GETMINMAXINFO: {
        auto *mmi = reinterpret_cast<MINMAXINFO *>(lp);
        auto min_s = win->min_size();
        auto max_s = win->max_size();
        float scale = get_window_scale(hwnd);
        DWORD style = WS_OVERLAPPEDWINDOW;
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
    case WM_MBUTTONDOWN: {
        float scale = get_window_scale(hwnd);
        int mx = static_cast<int>(static_cast<short>(LOWORD(lp)));
        int my = static_cast<int>(static_cast<short>(HIWORD(lp)));
        int button = (msg == WM_LBUTTONDOWN) ? 0 : (msg == WM_RBUTTONDOWN) ? 1 : 2;
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
        return 0;
    }
    case WM_LBUTTONUP:
    case WM_RBUTTONUP:
    case WM_MBUTTONUP: {
        ReleaseCapture();
        float scale = get_window_scale(hwnd);
        int mx = static_cast<int>(static_cast<short>(LOWORD(lp)));
        int my = static_cast<int>(static_cast<short>(HIWORD(lp)));
        int button = (msg == WM_LBUTTONUP) ? 0 : (msg == WM_RBUTTONUP) ? 1 : 2;
        MouseEvent e{};
        e.type = MouseEvent::Type::Release;
        e.position = {static_cast<float>(mx) / scale, static_cast<float>(my) / scale};
        e.button = button;
        win->handle_mouse(e);
        return 0;
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
        bool ctrl = (GetKeyState(VK_CONTROL) & 0x8000) != 0;
        bool alt = (GetKeyState(VK_MENU) & 0x8000) != 0;
        bool shift = (GetKeyState(VK_SHIFT) & 0x8000) != 0;
        bool super = (GetKeyState(VK_LWIN) & 0x8000) != 0 || (GetKeyState(VK_RWIN) & 0x8000) != 0;
        Key key = vk_to_key(wp);
        if (key != Key::NoKey || ctrl || alt || super) {
            KeyEvent ke;
            ke.type = KeyEvent::Type::Press;
            ke.key = key;
            ke.shift = shift;
            ke.ctrl = ctrl;
            ke.alt = alt;
            ke.super = super;
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
        if (wc < 32) {
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
        bool managed = (app->window_map.find(hwnd) != app->window_map.end());
        if (managed) {
            app->window_map.erase(hwnd);
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

#include <gdiplus.h>
#include <objidl.h>

Win32PlatformApplication::Win32PlatformApplication() {
    enable_dpi_awareness();
    s_win32_app = this;
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

std::string_view Win32PlatformApplication::painter_name() const {
#ifdef TOOLKIT_HAS_CAIRO
    return "Cairo";
#else
    return "GDI+";
#endif
}

float Win32PlatformApplication::scale_factor() const {
    return static_cast<float>(get_system_dpi()) / 96.0f;
}

SystemFonts Win32PlatformApplication::system_fonts() const {
    NONCLIENTMETRICSW ncm = {sizeof(NONCLIENTMETRICSW)};
    if (SystemParametersInfoW(SPI_GETNONCLIENTMETRICS, sizeof(ncm), &ncm, 0)) {
        float scale = scale_factor();
        float size = static_cast<float>(std::abs(ncm.lfMessageFont.lfHeight)) / scale;
        return {wide_to_utf8(ncm.lfMessageFont.lfFaceName), "Consolas", size};
    }
    // 12pt at 96 DPI = 16px
    return {"Segoe UI", "Consolas", 16.0f};
}

std::unique_ptr<PlatformWindow> Win32PlatformApplication::create_window(std::string_view title,
                                                                        Size size, Window *owner) {
    return std::make_unique<Win32PlatformWindow>(this, title, size, owner);
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

// --- Win32PlatformWindow ---

Win32PlatformWindow::Win32PlatformWindow(Win32PlatformApplication *app, std::string_view title,
                                         Size size, Window *owner)
    : app_(app), owner_(owner) {
    arrow_cursor = LoadCursorW(nullptr, IDC_ARROW);
    ibeam_cursor = LoadCursorW(nullptr, IDC_IBEAM);
    hand_cursor = LoadCursorW(nullptr, IDC_HAND);
    not_allowed_cursor = LoadCursorW(nullptr, IDC_NO);
    std::wstring wtitle = utf8_to_wide(title);
    float scale = static_cast<float>(get_system_dpi()) / 96.0f;

    DWORD style = WS_OVERLAPPEDWINDOW;
    DWORD adjust_style = WS_OVERLAPPEDWINDOW;

    RECT r = {0, 0, static_cast<LONG>(size.width * scale), static_cast<LONG>(size.height * scale)};
    AdjustWindowRectEx(&r, adjust_style, FALSE, 0);
    hwnd = CreateWindowExW(0, Win32PlatformApplication::kWindowClassName, wtitle.c_str(), style,
                           CW_USEDEFAULT, CW_USEDEFAULT, r.right - r.left, r.bottom - r.top,
                           nullptr, nullptr, app_->hinstance, nullptr);

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
    if (hwnd) {
        if (hglrc) {
            wglDeleteContext(hglrc);
        }
        DestroyWindow(hwnd);
        hwnd = nullptr;
    }
}

void Win32PlatformWindow::show() {
    ShowWindow(hwnd, SW_SHOW);
    UpdateWindow(hwnd);
}

void Win32PlatformWindow::close() {
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
        ShowWindow(hwnd, IsZoomed(hwnd) ? SW_RESTORE : SW_MAXIMIZE);
    }
}

void Win32PlatformWindow::set_size(Size s) {
    if (!hwnd) {
        return;
    }
    float scale = get_window_scale(hwnd);
    RECT r = {0, 0, static_cast<LONG>(s.width * scale), static_cast<LONG>(s.height * scale)};
    AdjustWindowRectEx(&r, WS_OVERLAPPEDWINDOW, FALSE, 0);
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

void Win32PlatformWindow::show_tooltip_window(std::string const &text, Point local_pos) {
    float scale = get_window_scale(hwnd);
    auto const &style = Theme::current().tooltip;
    float pad = style.padding, font_sz = style.font_size;
    auto text_sz = Painter::measure_text(text, font_sz);
    auto fm = Painter::measure_font_metrics(font_sz);
    float w = text_sz.width + pad * 2, h = fm.height + pad * 2;
    POINT pt = {static_cast<LONG>(local_pos.x * scale), static_cast<LONG>(local_pos.y * scale)};
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

#ifdef TOOLKIT_HAS_CAIRO
    cairo_surface_t *surface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, piw, pih);
    cairo_t *cr = cairo_create(surface);
    cairo_scale(cr, scale, scale);
    CairoPainter painter(cr);
    Rect r{0, 0, w, h};
    painter.fill_rounded_rect(r, style.background, style.corner_radius);
    painter.draw_rounded_rect(r, style.border, style.corner_radius, style.border_width);
    painter.draw_text(text, {pad, pad + fm.ascent}, style.text, font_sz);
    cairo_surface_flush(surface);
    unsigned char *data = cairo_image_surface_get_data(surface);
    int stride = cairo_image_surface_get_stride(surface);
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
    std::memcpy(bits, data, stride * pih);
    HBITMAP old_bm = static_cast<HBITMAP>(SelectObject(mem_dc, hbm));
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
    cairo_destroy(cr);
    cairo_surface_destroy(surface);
#else
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

    {
        GDIPainter painter(mem_dc, scale);
        Rect r{0, 0, w, h};
        painter.fill_rounded_rect(r, style.background, style.corner_radius);
        painter.draw_rounded_rect(r, style.border, style.corner_radius, style.border_width);
        painter.draw_text(text, {pad, pad + fm.ascent}, style.text, font_sz);
    }

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
#endif
    ShowWindow(tooltip_hwnd, SW_SHOWNOACTIVATE);
}

void Win32PlatformWindow::hide_tooltip_window() {
    if (tooltip_hwnd) {
        ShowWindow(tooltip_hwnd, SW_HIDE);
    }
}

bool Win32PlatformWindow::save_to_png(std::string const &path) {
#ifdef TOOLKIT_HAS_CAIRO
    return cairo_save_to_png(owner_, path);
#else
    return GDIPainter::save_to_png(owner_, path);
#endif
}

float Win32PlatformWindow::scale_factor() const { return get_window_scale(hwnd); }

Size Win32PlatformApplication::measure_text(std::string_view text, float font_size,
                                            FontFamily font) {
#ifdef TOOLKIT_HAS_CAIRO
    return cairo_measure_text(text, font_size, font);
#else
    return GDIPainter::measure_text_gdiplus(text, font_size, font);
#endif
}

Painter::FontMetrics Win32PlatformApplication::measure_font_metrics(float font_size,
                                                                    FontFamily font) {
#ifdef TOOLKIT_HAS_CAIRO
    return cairo_measure_font_metrics(font_size, font);
#else
    return GDIPainter::font_metrics_gdiplus(font_size, font);
#endif
}

} // namespace toolkit
