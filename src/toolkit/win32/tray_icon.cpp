// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Diego Iastrubni <diegoiast@gmail.com>

// Windows system tray backend -- Shell_NotifyIcon. See include/toolkit/tray_icon.hpp
// for the cross-platform contract; the split of left-click (show/hide the owner
// window) and right-click (a menu of Commands) is the same as the Linux backend's,
// only rendered by the shell here rather than by a tray host over D-Bus.
//
// Unlike SNI/DBusMenu, the menu is ours to draw: TrackPopupMenuEx takes real screen
// coordinates, so there is no absolute-positioning gap to work around.
//
// EVENTUALLY (not implemented, revisit if/when actually needed):
//  - The right-click menu is fixed at construction, matching the Linux backend.
//    Enabled/checked state *is* re-read on every popup, but the set of items and
//    their icons is not.
//  - Balloon notifications (NIF_INFO) are not exposed; Window::show_toast() is the
//    toolkit's own in-window answer to that.

#include "toolkit/tray_icon.hpp"
#include "toolkit/window.hpp"

#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
// Must follow windows.h.
#include <shellapi.h>

#include <spdlog/spdlog.h>
#include <string>
#include <vector>

namespace toolkit {

namespace {

constexpr UINT kTrayCallbackMessage = WM_APP + 100;
constexpr UINT kTrayIconId = 1;
constexpr wchar_t kTrayClassName[] = L"TKTrayIcon";

auto to_wide(std::string const &s) -> std::wstring {
    if (s.empty()) {
        return {};
    }
    auto len = MultiByteToWideChar(CP_UTF8, 0, s.data(), static_cast<int>(s.size()), nullptr, 0);
    if (len <= 0) {
        return {};
    }
    auto out = std::wstring(static_cast<size_t>(len), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s.data(), static_cast<int>(s.size()), out.data(), len);
    return out;
}

// A top-down 32bpp DIB holding `img`. Menu item bitmaps (MIIM_BITMAP) are alpha-blended by the
// menu renderer and expect premultiplied ARGB, whereas an icon's colour bitmap wants the straight
// alpha ImageData already carries -- hence the flag rather than two near-identical functions.
auto dib_from_image(ImageData const &img, bool premultiply) -> HBITMAP {
    BITMAPINFO bi = {};
    bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bi.bmiHeader.biWidth = img.width;
    bi.bmiHeader.biHeight = -img.height;
    bi.bmiHeader.biPlanes = 1;
    bi.bmiHeader.biBitCount = 32;
    bi.bmiHeader.biCompression = BI_RGB;

    void *bits = nullptr;
    auto screen_dc = GetDC(nullptr);
    auto bitmap = CreateDIBSection(screen_dc, &bi, DIB_RGB_COLORS, &bits, nullptr, 0);
    ReleaseDC(nullptr, screen_dc);
    if (!bitmap || !bits) {
        return nullptr;
    }

    auto count = static_cast<size_t>(img.width) * static_cast<size_t>(img.height);
    auto const *src = img.pixels.data();
    auto *dst = static_cast<uint8_t *>(bits);
    auto swapped = img.format == PixelFormat::RGBA;
    for (size_t i = 0; i < count; i++) {
        auto b = src[i * 4 + (swapped ? 2 : 0)];
        auto g = src[i * 4 + 1];
        auto r = src[i * 4 + (swapped ? 0 : 2)];
        auto a = src[i * 4 + 3];
        if (premultiply) {
            b = static_cast<uint8_t>(b * a / 255);
            g = static_cast<uint8_t>(g * a / 255);
            r = static_cast<uint8_t>(r * a / 255);
        }
        dst[i * 4 + 0] = b;
        dst[i * 4 + 1] = g;
        dst[i * 4 + 2] = r;
        dst[i * 4 + 3] = a;
    }
    return bitmap;
}

auto icon_from_image(ImageData const &img) -> HICON {
    if (img.pixels.empty() || img.width <= 0 || img.height <= 0) {
        return nullptr;
    }
    auto color = dib_from_image(img, false);
    if (!color) {
        return nullptr;
    }

    // CreateIconIndirect wants an AND mask even for a 32bpp colour bitmap. An all-zero one means
    // "take every pixel from the colour bitmap", leaving its alpha channel to do the masking.
    auto mask = CreateBitmap(img.width, img.height, 1, 1, nullptr);
    auto mask_dc = CreateCompatibleDC(nullptr);
    auto previous = static_cast<HBITMAP>(SelectObject(mask_dc, mask));
    PatBlt(mask_dc, 0, 0, img.width, img.height, BLACKNESS);
    SelectObject(mask_dc, previous);
    DeleteDC(mask_dc);

    ICONINFO info = {};
    info.fIcon = TRUE;
    info.hbmColor = color;
    info.hbmMask = mask;
    auto icon = CreateIconIndirect(&info);
    DeleteObject(color);
    DeleteObject(mask);
    return icon;
}

// Everything the window procedure needs to reach from a bare HWND. TrayIcon::Impl is private to
// the class, so a free function cannot name it -- keeping the state in a base type it *can* name
// is the only reason this is split off.
struct TrayState {
    HWND hwnd = nullptr;
    HICON hicon = nullptr;
    // LoadIconW's shared system icons must never be passed to DestroyIcon, and the fallback path
    // below can hand us one.
    bool owns_icon = false;
    bool icon_added = false;
    NOTIFYICONDATAW nid = {};
    UINT taskbar_created = 0;

    std::vector<Command::Ptr> right_click_actions;
    // Parallel to right_click_actions; null where a command has no icon. Built once, because the
    // menu is rebuilt on every popup and creating these per popup would leak them.
    std::vector<HBITMAP> menu_bitmaps;

    // Weak: the tray icon never keeps the window alive, and outliving it is normal (the window
    // can be closed while the icon stays in the tray).
    std::weak_ptr<Window> owner_window;
    // Tracks what the last toggle did, not the window's actual state -- there is no is_visible()
    // on Window to read back. Same caveat as the Linux backend: fine for a click toggle, not a
    // substitute for real visibility tracking.
    bool window_shown = true;

    ~TrayState() {
        if (icon_added) {
            Shell_NotifyIconW(NIM_DELETE, &nid);
        }
        if (hwnd) {
            DestroyWindow(hwnd);
        }
        if (hicon && owns_icon) {
            DestroyIcon(hicon);
        }
        for (auto bitmap : menu_bitmaps) {
            if (bitmap) {
                DeleteObject(bitmap);
            }
        }
    }

    void toggle_window() {
        auto window = owner_window.lock();
        if (!window) {
            return;
        }
        if (window_shown) {
            window->hide();
        } else {
            window->show();
        }
        window_shown = !window_shown;
    }

    void show_context_menu() {
        if (right_click_actions.empty()) {
            return;
        }
        auto menu = CreatePopupMenu();
        if (!menu) {
            return;
        }

        // Labels have to stay alive until TrackPopupMenuEx returns: MENUITEMINFOW::dwTypeData is
        // a plain pointer the menu reads from, not a copy it takes.
        auto labels = std::vector<std::wstring>{};
        labels.reserve(right_click_actions.size());
        for (auto const &cmd : right_click_actions) {
            labels.push_back(to_wide(cmd->name()));
        }

        for (size_t i = 0; i < right_click_actions.size(); i++) {
            auto const &cmd = right_click_actions[i];
            MENUITEMINFOW item = {};
            item.cbSize = sizeof(item);
            item.fMask = MIIM_ID | MIIM_STRING | MIIM_STATE;
            // 1-based: TrackPopupMenuEx returns 0 for "nothing was chosen".
            item.wID = static_cast<UINT>(i + 1);
            item.dwTypeData = labels[i].data();
            item.fState = cmd->is_enabled() ? MFS_ENABLED : MFS_DISABLED;
            if (cmd->is_checked()) {
                item.fState |= MFS_CHECKED;
            }
            if (menu_bitmaps[i]) {
                item.fMask |= MIIM_BITMAP;
                item.hbmpItem = menu_bitmaps[i];
            }
            InsertMenuItemW(menu, static_cast<UINT>(i), TRUE, &item);
        }

        POINT cursor = {};
        GetCursorPos(&cursor);

        // The SetForegroundWindow/WM_NULL pair is the documented workaround for a tray menu that
        // otherwise stays on screen after the user clicks away: a popup menu only tracks
        // dismissal for the foreground window, so this window has to take that role and then be
        // nudged out of the menu's modal loop afterwards.
        SetForegroundWindow(hwnd);
        auto chosen = TrackPopupMenuEx(menu, TPM_RIGHTBUTTON | TPM_RETURNCMD, cursor.x, cursor.y,
                                       hwnd, nullptr);
        PostMessageW(hwnd, WM_NULL, 0, 0);
        DestroyMenu(menu);

        if (chosen >= 1 && static_cast<size_t>(chosen) <= right_click_actions.size()) {
            right_click_actions[static_cast<size_t>(chosen) - 1]->execute();
        }
    }
};

LRESULT CALLBACK tray_wnd_proc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    auto *self = reinterpret_cast<TrayState *>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    if (!self) {
        return DefWindowProcW(hwnd, msg, wp, lp);
    }

    if (msg == kTrayCallbackMessage) {
        switch (LOWORD(lp)) {
        case WM_LBUTTONUP:
            self->toggle_window();
            return 0;
        case WM_RBUTTONUP:
        case WM_CONTEXTMENU:
            self->show_context_menu();
            return 0;
        default:
            return 0;
        }
    }

    // Explorer restarting takes every tray icon with it and then broadcasts this; ours has to be
    // added again or it is gone for the rest of the session.
    if (self->taskbar_created != 0 && msg == self->taskbar_created) {
        Shell_NotifyIconW(NIM_ADD, &self->nid);
        return 0;
    }

    return DefWindowProcW(hwnd, msg, wp, lp);
}

auto register_tray_class(HINSTANCE hinstance) -> bool {
    static auto registered = [hinstance] {
        WNDCLASSEXW wc = {};
        wc.cbSize = sizeof(wc);
        wc.lpfnWndProc = tray_wnd_proc;
        wc.hInstance = hinstance;
        wc.lpszClassName = kTrayClassName;
        return RegisterClassExW(&wc) != 0;
    }();
    return registered;
}

} // namespace

struct TrayIcon::Impl : TrayState {};

TrayIcon::TrayIcon() : impl_(std::make_unique<Impl>()) {}
TrayIcon::~TrayIcon() = default;

std::unique_ptr<TrayIcon> TrayIconBuilder::build() const {
    // `id` has no Windows counterpart: StatusNotifierItem needs it to identify the item on the
    // bus, while Shell_NotifyIcon identifies ours by (hWnd, uID) alone. `icon_name` likewise:
    // a freedesktop icon-theme name means nothing here, icon() is what gets drawn.
    auto hinstance = GetModuleHandleW(nullptr);
    if (!register_tray_class(hinstance)) {
        spdlog::warn("TrayIcon: RegisterClassEx failed, error={}", GetLastError());
        return nullptr;
    }

    auto tray = std::unique_ptr<TrayIcon>(new TrayIcon());
    auto *self = tray->impl_.get();
    self->right_click_actions = actions_;
    self->owner_window = owner_window_;

    // Deliberately *not* HWND_MESSAGE: a message-only window is excluded from broadcasts, and
    // TaskbarCreated (see the window procedure) is one. So this is a real top-level window that
    // is simply never shown, with WS_EX_TOOLWINDOW to keep it out of the taskbar and Alt+Tab.
    self->hwnd = CreateWindowExW(WS_EX_TOOLWINDOW, kTrayClassName, L"", WS_POPUP, 0, 0, 0, 0,
                                 nullptr, nullptr, hinstance, nullptr);
    if (!self->hwnd) {
        spdlog::error("TrayIcon: CreateWindowEx failed, error={}", GetLastError());
        return nullptr;
    }
    SetWindowLongPtrW(self->hwnd, GWLP_USERDATA,
                      reinterpret_cast<LONG_PTR>(static_cast<TrayState *>(self)));

    self->taskbar_created = RegisterWindowMessageW(L"TaskbarCreated");
    if (self->taskbar_created != 0) {
        // An elevated process would otherwise have the broadcast filtered out by UIPI, since
        // Explorer runs unelevated.
        ChangeWindowMessageFilterEx(self->hwnd, self->taskbar_created, MSGFLT_ALLOW, nullptr);
    }

    // Falling back to the owner window's own icon, and then to the stock application icon, keeps
    // a tray icon on screen rather than failing the whole call when the caller passed none.
    if (icon_ && !icon_->pixels.empty()) {
        self->hicon = icon_from_image(*icon_);
    }
    if (!self->hicon) {
        if (auto window = owner_window_.lock()) {
            if (auto window_icon = window->get_icon()) {
                self->hicon = icon_from_image(*window_icon);
            }
        }
    }
    self->owns_icon = self->hicon != nullptr;
    if (!self->hicon) {
        spdlog::warn("TrayIcon: no icon supplied, falling back to the stock application icon");
        self->hicon = LoadIconW(nullptr, IDI_APPLICATION);
    }

    for (auto const &cmd : self->right_click_actions) {
        HBITMAP bitmap = nullptr;
        if (auto image = cmd->icon_image(); image && !image->pixels.empty()) {
            bitmap = dib_from_image(*image, true);
        }
        self->menu_bitmaps.push_back(bitmap);
    }

    self->nid.cbSize = sizeof(self->nid);
    self->nid.hWnd = self->hwnd;
    self->nid.uID = kTrayIconId;
    self->nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    self->nid.uCallbackMessage = kTrayCallbackMessage;
    self->nid.hIcon = self->hicon;
    // szTip is a fixed 128-wchar buffer; a longer tooltip is truncated rather than rejected.
    wcsncpy_s(self->nid.szTip, to_wide(tooltip_).c_str(), _TRUNCATE);

    if (!Shell_NotifyIconW(NIM_ADD, &self->nid)) {
        spdlog::error("TrayIcon: Shell_NotifyIcon(NIM_ADD) failed, error={}", GetLastError());
        return nullptr;
    }
    self->icon_added = true;
    return tray;
}

} // namespace toolkit
